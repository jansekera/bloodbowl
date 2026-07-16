// diag_advance_vs_block_harness.cpp  (2026-07-16)
//
// Standalone READ-ONLY diagnostic: why did MacroMCTS pick BLOCK (carrier id3
// -> skaven id21) instead of ADVANCE at g0001.json.gz turns[23] (H2, home T4,
// orc thrower carrier sitting at (0,4) in his own endzone, home leading 1-0)?
//
// Reconstructs the exact turn-start state from the replay snapshot and runs
// the REAL engine code (linked against engine/build/libbb_engine.so):
//   1. getAvailableMacros + the real expand() priors at the root
//   2. per-root-macro one-ply Monte Carlo Q: greedyExpandMacro (real dice)
//      followed by the real MacroMCTSSearch::simulate() leaf heuristic
//   3. term-by-term decomposition of simulate() for representative post-BLOCK
//      / post-ADVANCE states (mirror is asserted equal to the real simulate()
//      to 1e-9 on every state it touches)
//   4. full MacroMCTSSearch::search() over many seeds with the production
//      config used to generate the replays (iters=100, C=1.0, vfBlend=0,
//      policy net loaded with policyBlend=0 -> heuristic prior floors active)
//   5. variants: carrier unmarked (id21 moved away), different reroll counts
//
// NOTE: `#define private public` before including macro_mcts.h exposes
// MacroMCTSSearch::simulate()/expand() for direct invocation. Access
// specifiers do not change layout or mangling here, so the calls land in the
// exact compiled code inside libbb_engine.so. Diagnostic-only trick; no
// engine source is modified.
//
// Build (repo root):
//   g++ -O2 -std=c++20 -Iengine/include -Iengine/third_party \
//       diag_advance_vs_block_harness.cpp \
//       -Lengine/build -lbb_engine -Wl,-rpath,$PWD/engine/build \
//       -o /tmp/.../diag_advance_vs_block && /tmp/.../diag_advance_vs_block

#include "bb/game_state.h"
#include "bb/macro_actions.h"
#include "bb/mcts.h"
#include "bb/value_function.h"
#include "bb/feature_extractor.h"
#include "bb/policy_network.h"
#include "bb/policies.h"
#include "bb/dice.h"
#include "bb/game_simulator.h"
#include "bb/roster.h"
#include "bb/helpers.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <string>
#include <vector>

#define private public
#include "bb/macro_mcts.h"
#undef private

using namespace bb;

static const char* macroName(MacroType t) {
    switch (t) {
        case MacroType::SCORE: return "SCORE";
        case MacroType::ADVANCE: return "ADVANCE";
        case MacroType::CAGE: return "CAGE";
        case MacroType::BLITZ: return "BLITZ";
        case MacroType::BLOCK: return "BLOCK";
        case MacroType::PICKUP: return "PICKUP";
        case MacroType::PASS_ACTION: return "PASS_ACTION";
        case MacroType::FOUL: return "FOUL";
        case MacroType::REPOSITION: return "REPOSITION";
        case MacroType::END_TURN: return "END_TURN";
        case MacroType::BLITZ_AND_SCORE: return "BLITZ_AND_SCORE";
        case MacroType::HAND_OFF_SCORE: return "HAND_OFF_SCORE";
        case MacroType::PASS_SCORE: return "PASS_SCORE";
        case MacroType::CHAIN_SCORE: return "CHAIN_SCORE";
        default: return "?";
    }
}

static std::string macroLabel(const Macro& m) {
    char buf[96];
    if (m.type == MacroType::REPOSITION)
        snprintf(buf, sizeof buf, "%s p%d->(%d,%d)", macroName(m.type), m.playerId,
                 (int)m.targetPos.x, (int)m.targetPos.y);
    else
        snprintf(buf, sizeof buf, "%s p%d t%d", macroName(m.type), m.playerId, m.targetId);
    return buf;
}

// ---------------------------------------------------------------------------
// State reconstruction from g0001.json.gz turns[23] (start of home T4, H2).
// Snapshot state ints: 0=STANDING, 1=PRONE. Away id20 absent -> KO.
// ---------------------------------------------------------------------------
struct Snap { int id, x, y, st; };
static const Snap SNAP[] = {
    {1, 10, 5, 0}, {2, 11, 7, 0}, {3, 0, 4, 0}, {4, 15, 1, 0}, {5, 9, 4, 1},
    {6, 10, 4, 0}, {7, 10, 7, 0}, {8, 6, 3, 0}, {9, 4, 3, 0}, {10, 7, 3, 0},
    {11, 8, 7, 0},
    {12, 16, 2, 1}, {13, 14, 5, 1}, {14, 12, 6, 0}, {15, 12, 4, 0},
    {16, 13, 2, 0}, {17, 13, 4, 0}, {18, 11, 5, 0}, {19, 13, 6, 0},
    {21, 0, 3, 0}, {22, 14, 3, 0},
};

static GameState makeState(int rerolls) {
    GameState s;
    const TeamRoster* orc = getDevelopedRoster("orc", 1200);
    const TeamRoster* skv = getDevelopedRoster("skaven", 1200);
    if (!orc || !skv) { fprintf(stderr, "roster load failed\n"); exit(1); }
    setupHalf(s, *orc, *skv, TeamSide::AWAY);  // populates stats/skills per id

    s.half = 2;
    s.phase = GamePhase::PLAY;
    s.activeTeam = TeamSide::HOME;
    s.weather = Weather::NICE;
    s.kickingTeam = TeamSide::AWAY;  // not used by the macro search path
    s.turnoverPending = false;
    s.currentActivationId = -1;

    s.homeTeam.score = 1; s.awayTeam.score = 0;
    s.homeTeam.turnNumber = 4; s.awayTeam.turnNumber = 4;
    s.homeTeam.rerolls = rerolls; s.awayTeam.rerolls = rerolls;
    s.homeTeam.rerollUsedThisTurn = s.awayTeam.rerollUsedThisTurn = false;
    s.homeTeam.blitzUsedThisTurn = s.awayTeam.blitzUsedThisTurn = false;
    s.homeTeam.passUsedThisTurn = s.awayTeam.passUsedThisTurn = false;
    s.homeTeam.foulUsedThisTurn = s.awayTeam.foulUsedThisTurn = false;

    // Default everyone off-pitch, then place snapshot players.
    for (auto& p : s.players) {
        p.state = PlayerState::OFF_PITCH;
        p.position = {0, 0};
        p.hasMoved = p.hasActed = p.usedBlitz = false;
        p.lostTacklezones = p.proUsedThisTurn = false;
    }
    for (const auto& sn : SNAP) {
        Player& p = s.getPlayer(sn.id);
        p.state = (sn.st == 0) ? PlayerState::STANDING : PlayerState::PRONE;
        p.position = {(int8_t)sn.x, (int8_t)sn.y};
        p.movementRemaining = p.stats.movement;
    }
    s.getPlayer(20).state = PlayerState::KO;  // KO'd on home T3 (turns[21])

    s.ball.position = {0, 4};
    s.ball.isHeld = true;
    s.ball.carrierId = 3;
    return s;
}

// ---------------------------------------------------------------------------
// Mirror of MacroMCTSSearch::simulate() with per-term printout.
// vfBlend=0 / leafLookahead=false paths only (the production replay config).
// Asserted equal to the real simulate() on every state it is used on.
// ---------------------------------------------------------------------------
static double simMirror(const GameState& state, TeamSide perspective, bool print,
                        std::map<std::string, double>* termsOut = nullptr) {
    const TeamState& my = state.getTeamState(perspective);
    const TeamState& opp = state.getTeamState(opponent(perspective));
    double heuristic = 0.0, scoringBonus = 0.0;
    std::map<std::string, double> T;

    T["scoreDiff*0.5"] = (my.score - opp.score) * 0.5;
    heuristic += T["scoreDiff*0.5"];

    int turnsLeft = std::max(0, 9 - my.turnNumber);

    if (state.ball.isHeld && state.ball.carrierId > 0) {
        const Player& carrier = state.getPlayer(state.ball.carrierId);
        int ezX = (carrier.teamSide == TeamSide::HOME) ? 25 : 0;
        int dist = std::abs(carrier.position.x - ezX);
        int ma = carrier.stats.movement;
        double proximity = 1.0 - dist / 25.0;

        if (carrier.teamSide == perspective) {
            T["possession"] = 0.1; heuristic += 0.1;
            T["SB:proximity"] = 0.25 * proximity; scoringBonus += T["SB:proximity"];

            if (turnsLeft >= 3) {
                double cageProxSum = 0.0; int cageN = 0;
                state.forEachOnPitch(perspective, [&](const Player& p) {
                    if (p.state != PlayerState::STANDING) return;
                    if (p.id == carrier.id) return;
                    if (p.position.distanceTo(carrier.position) > 4) return;
                    int pd = std::abs(p.position.x - ezX);
                    cageProxSum += 1.0 - pd / 25.0;
                    cageN++;
                });
                if (cageN > 0) {
                    T["SB:cageAdvance"] = 0.20 * (cageProxSum / cageN);
                    scoringBonus += T["SB:cageAdvance"];
                }
                T["(cageN)"] = cageN;
            }

            if (dist <= (int)carrier.movementRemaining) {
                T["SB:safeWalkIn"] = 0.4; scoringBonus += 0.4;
            } else if (dist <= carrier.movementRemaining + 2) {
                T["SB:gfiScore"] = 0.2; scoringBonus += 0.2;
            }

            if (turnsLeft > 0 && dist > 0) {
                int idealDist = turnsLeft * ma;
                double pacing = 1.0 - std::abs(dist - idealDist) / (double)std::max(idealDist, 1);
                if (pacing > 0) { T["SB:stallPacing"] = 0.1 * pacing; scoringBonus += 0.1 * pacing; }
                T["(idealDist)"] = idealDist; T["(dist)"] = dist;
            }

            if (turnsLeft <= 2 && dist <= ma + 2) { T["SB:urgency"] = 0.3; scoringBonus += 0.3; }
            if (turnsLeft <= 1) {
                if (dist <= (int)carrier.movementRemaining) { T["SB:ottSafe"] = 0.8; scoringBonus += 0.8; }
                else if (dist <= carrier.movementRemaining + 2) { T["SB:ottGfi"] = 0.5; scoringBonus += 0.5; }
            }

            if (dist > (int)carrier.movementRemaining + 2) {
                auto adj = carrier.position.getAdjacent();
                for (auto& apos : adj) {
                    if (!apos.isOnPitch()) continue;
                    const Player* tm = state.getPlayerAtPosition(apos);
                    if (!tm || tm->teamSide != perspective) continue;
                    if (tm->state != PlayerState::STANDING) continue;
                    int tmDist = std::abs(tm->position.x - ezX);
                    if (tmDist > 0 && tmDist <= (int)tm->movementRemaining + 2) {
                        T["SB:handoffPot"] = 0.15; scoringBonus += 0.15; break;
                    }
                }
            }
        } else {
            T["oppPossession"] = -0.1 - 0.25 * proximity;
            heuristic += T["oppPossession"];
            if (dist <= (int)carrier.movementRemaining) { T["oppCanScore"] = -0.4; heuristic += -0.4; }
        }
    } else if (!state.ball.isHeld && state.ball.isOnPitch()) {
        T["looseBall"] = -0.1; heuristic += -0.1;
        int nearestDist = 999;
        state.forEachOnPitch(perspective, [&](const Player& p) {
            if (p.state != PlayerState::STANDING) return;
            int d = p.position.distanceTo(state.ball.position);
            if (d < nearestDist) nearestDist = d;
        });
        if (nearestDist <= 2) { T["nearLoose"] = 0.08; heuristic += 0.08; }
        else if (nearestDist <= 4) { T["nearLoose"] = 0.04; heuristic += 0.04; }
    }

    if (state.ball.isHeld && state.ball.carrierId > 0) {
        const Player& bh = state.getPlayer(state.ball.carrierId);
        if (bh.teamSide != perspective && bh.isOnPitch() && bh.state == PlayerState::STANDING) {
            int carrierTZ = countTacklezones(state, bh.position, bh.teamSide);
            if (carrierTZ > 0) { T["markOppCarrier"] = 0.08 * std::min(carrierTZ, 3); heuristic += T["markOppCarrier"]; }
            int y = bh.position.y;
            if (y <= 2 || y >= 12) { T["sidelineTrap"] = 0.10; heuristic += 0.10; }
            else if (y <= 4 || y >= 10) { T["sidelineTrap"] = 0.05; heuristic += 0.05; }
            if (bh.stats.agility >= 4) {
                int tzCount = countTacklezones(state, bh.position, bh.teamSide);
                if (tzCount >= 2) { T["containAgile"] = 0.06 * std::min(tzCount - 1, 2); heuristic += T["containAgile"]; }
            }
        }
    }

    {
        int bashExposure = 0;
        state.forEachOnPitch(perspective, [&](const Player& p) {
            if (p.state != PlayerState::STANDING) return;
            auto adj = p.position.getAdjacent();
            for (auto& apos : adj) {
                if (!apos.isOnPitch()) continue;
                const Player* o = state.getPlayerAtPosition(apos);
                if (o && o->teamSide != perspective && o->state == PlayerState::STANDING &&
                    o->stats.strength >= 4) { bashExposure++; break; }
            }
        });
        if (bashExposure) { T["bashExposure"] = -0.05 * bashExposure; heuristic += T["bashExposure"]; }
    }

    int myPlayers = 0, oppPlayers = 0;
    state.forEachOnPitch(perspective, [&](const Player& p) { if (p.state == PlayerState::STANDING) myPlayers++; });
    state.forEachOnPitch(opponent(perspective), [&](const Player& p) { if (p.state == PlayerState::STANDING) oppPlayers++; });
    T["playerDiff*0.03"] = (myPlayers - oppPlayers) * 0.03;
    heuristic += T["playerDiff*0.03"];

    heuristic = std::clamp(heuristic, -1.0, 1.0);
    double total = std::clamp(heuristic + scoringBonus, -1.0, 1.0);

    if (print) {
        for (auto& [k, v] : T)
            if (k[0] != '(') printf("      %-18s %+0.4f\n", k.c_str(), v);
        if (T.count("(idealDist)"))
            printf("      (dist=%g idealDist=%g cageN=%g)\n",
                   T.count("(dist)") ? T["(dist)"] : -1, T["(idealDist)"],
                   T.count("(cageN)") ? T["(cageN)"] : 0);
        printf("      heuristic=%+0.4f scoringBonus=%+0.4f TOTAL=%+0.4f\n",
               heuristic, total - heuristic, total);
    }
    if (termsOut) *termsOut = T;
    return total;
}

// ---------------------------------------------------------------------------

static MCTSConfig makeConfig(const PolicyNetwork* pol) {
    // == replay production config (diag_perplayer_grounding.py -> bb_module):
    // mcts_iterations=100, exploration_c=1.0, dirichlet_alpha=0.0,
    // vf_blend=0.0, n_rollouts=1, leaf_lookahead=false,
    // policy_weights_path=weights_policy.json with policy_blend=0.0
    // (policy pointer non-null => heuristic prior floors in expand() active).
    MCTSConfig cfg;
    cfg.maxIterations = 100;
    cfg.timeBudgetMs = 0;
    cfg.explorationC = 1.0;
    cfg.dirichletAlpha = 0.0f;
    cfg.dirichletWeight = 0.25f;
    cfg.vfBlend = 0.0f;
    cfg.nRollouts = 1;
    cfg.leafLookahead = false;
    cfg.policy = pol;
    cfg.policyBlend = 0.0f;
    return cfg;
}

int main(int argc, char** argv) {
    std::string root = (argc > 1) ? argv[1] : ".";
    auto vf = loadValueFunction(root + "/weights_best.json");
    auto pol = loadPolicyNetworkFromFile(root + "/weights_policy.json");
    printf("vf=%s policy=%s (vfBlend=0 so VF unused in leaf; policy non-null gates prior floors)\n",
           vf ? "loaded" : "NULL", pol ? "loaded" : "NULL");

    GameState state = makeState(/*rerolls=*/2);

    // --- sanity: stats of the key players ---
    for (int id : {3, 21, 9, 18, 1}) {
        const Player& p = state.getPlayer(id);
        printf("p%-2d side=%s (%d,%d) st=%d MA%d ST%d AG%d block=%d dodge=%d sureHands=%d\n",
               id, p.teamSide == TeamSide::HOME ? "H" : "A", (int)p.position.x,
               (int)p.position.y, (int)p.state, p.stats.movement, p.stats.strength,
               p.stats.agility, p.hasSkill(SkillName::Block),
               p.hasSkill(SkillName::Dodge), p.hasSkill(SkillName::SureHands));
    }

    // --- 1. root macros + real expand() priors ---
    MCTSConfig cfg = makeConfig(pol.get());
    MacroMCTSSearch search(vf.get(), cfg, 12345);

    MacroMCTSNode root_;
    root_.visits = 1;
    search.expand(&root_, state);
    printf("\n=== ROOT MACROS (real expand(), n=%zu) ===\n", root_.children.size());
    for (auto& c : root_.children)
        printf("  prior=%.4f  %s\n", c.prior, macroLabel(c.macro).c_str());

    // --- baseline leaf value of the root state itself ---
    printf("\n=== ROOT STATE simulate() decomposition (perspective HOME) ===\n");
    double realV = search.simulate(state, TeamSide::HOME);
    double mirV = simMirror(state, TeamSide::HOME, true);
    printf("  real simulate()=%.6f mirror=%.6f %s\n", realV, mirV,
           std::fabs(realV - mirV) < 1e-9 ? "MATCH" : "*** MISMATCH ***");

    // --- 2. one-ply Monte Carlo Q per root macro ---
    printf("\n=== ONE-PLY MC Q (greedyExpandMacro + real simulate(), K=3000) ===\n");
    printf("%-28s %8s %8s %8s %10s %10s\n", "macro", "meanQ", "sd", "p(TO)", "Q|noTO", "Q|TO");
    const int K = 3000;
    for (auto& c : root_.children) {
        DiceRoller d(777);
        double sum = 0, sum2 = 0, sumTO = 0, sumOK = 0;
        int nTO = 0;
        for (int k = 0; k < K; ++k) {
            GameState sim = state.clone();
            auto res = greedyExpandMacro(sim, c.macro, d);
            double v = search.simulate(sim, TeamSide::HOME);
            sum += v; sum2 += v * v;
            if (res.turnover) { nTO++; sumTO += v; } else sumOK += v;
        }
        double mean = sum / K;
        double sd = std::sqrt(std::max(0.0, sum2 / K - mean * mean));
        printf("%-28s %+8.4f %8.4f %8.3f %+10.4f %+10.4f\n",
               macroLabel(c.macro).c_str(), mean, sd, (double)nTO / K,
               nTO < K ? sumOK / (K - nTO) : 0.0, nTO ? sumTO / nTO : 0.0);
    }

    // --- 3. representative post-state decompositions ---
    auto showOutcome = [&](const Macro& m, bool wantTO, const char* label, int maxTries = 4000) {
        for (int seed = 1; seed < maxTries; ++seed) {
            DiceRoller d(seed);
            GameState sim = state.clone();
            auto res = greedyExpandMacro(sim, m, d);
            if (res.turnover != wantTO) continue;
            const Player& car = sim.getPlayer(3);
            printf("\n  -- %s (seed %d): carrier(3)@(%d,%d) st=%d ball@(%d,%d) held=%d carrierId=%d TO=%d\n",
                   label, seed, (int)car.position.x, (int)car.position.y, (int)car.state,
                   (int)sim.ball.position.x, (int)sim.ball.position.y,
                   (int)sim.ball.isHeld, sim.ball.carrierId, (int)res.turnover);
            double rv = search.simulate(sim, TeamSide::HOME);
            double mv = simMirror(sim, TeamSide::HOME, true);
            printf("      real=%.6f mirror=%.6f %s\n", rv, mv,
                   std::fabs(rv - mv) < 1e-9 ? "MATCH" : "*** MISMATCH ***");
            return;
        }
        printf("\n  -- %s: no such outcome in %d seeds\n", label, maxTries);
    };

    Macro blockM{}, advM{};
    bool haveBlock = false, haveAdv = false;
    for (auto& c : root_.children) {
        if (c.macro.type == MacroType::BLOCK && c.macro.playerId == 3 && c.macro.targetId == 21) {
            blockM = c.macro; haveBlock = true;
        }
        if (c.macro.type == MacroType::ADVANCE && c.macro.playerId == 3) {
            advM = c.macro; haveAdv = true;
        }
    }
    printf("\n=== REPRESENTATIVE POST-STATES ===");
    if (haveBlock) {
        showOutcome(blockM, false, "post BLOCK 3->21, no turnover");
        showOutcome(blockM, true, "post BLOCK 3->21, turnover");
    }
    if (haveAdv) {
        showOutcome(advM, false, "post ADVANCE(3), no turnover");
        showOutcome(advM, true, "post ADVANCE(3), turnover");
    } else {
        printf("\n  ADVANCE(3) NOT GENERATED AT ROOT\n");
    }

    // --- 4. full production search, many seeds ---
    auto runSearches = [&](const GameState& st, const char* label, int nSeeds) {
        std::map<std::string, int> chosen;
        std::map<std::string, double> visitShare;
        std::map<std::string, int> visitCnt;
        for (int i = 0; i < nSeeds; ++i) {
            MacroMCTSSearch s2(vf.get(), cfg, 1000 + i);
            Macro best = s2.search(st);
            chosen[macroLabel(best)]++;
            int tot = 0;
            for (auto& cv : s2.lastChildVisits()) tot += cv.visits;
            for (auto& cv : s2.lastChildVisits()) {
                visitShare[macroLabel(cv.macro)] += tot ? (double)cv.visits / tot : 0;
                visitCnt[macroLabel(cv.macro)]++;
            }
        }
        printf("\n=== FULL SEARCH x%d seeds: %s ===\n", nSeeds, label);
        std::vector<std::pair<int, std::string>> byCount;
        for (auto& [k, v] : chosen) byCount.push_back({v, k});
        std::sort(byCount.rbegin(), byCount.rend());
        for (auto& [v, k] : byCount)
            printf("  chosen %4.1f%%  avg-visit-share %5.1f%%  %s\n",
                   100.0 * v / nSeeds,
                   visitCnt[k] ? 100.0 * visitShare[k] / nSeeds : 0.0, k.c_str());
    };

    runSearches(state, "exact turn-23 state (rerolls=2)", 400);

    // --- 5. variants ---
    { // rerolls sensitivity
        GameState s0 = makeState(0);
        runSearches(s0, "rerolls=0", 200);
    }
    { // carrier unmarked: move id21 far away (still on pitch)
        GameState s1 = makeState(2);
        s1.getPlayer(21).position = {11, 1};
        runSearches(s1, "id21 moved to (11,1): carrier UNMARKED", 400);

        // one-ply Q of ADVANCE in the unmarked state
        MacroMCTSNode r1; r1.visits = 1;
        MacroMCTSSearch s3(vf.get(), cfg, 999);
        s3.expand(&r1, s1);
        printf("\n  unmarked-state root priors:\n");
        for (auto& c : r1.children)
            printf("    prior=%.4f  %s\n", c.prior, macroLabel(c.macro).c_str());
        printf("  one-ply MC Q (K=3000):\n");
        for (auto& c : r1.children) {
            DiceRoller d(777);
            double sum = 0; int nTO = 0;
            for (int k = 0; k < K; ++k) {
                GameState sim = s1.clone();
                auto res = greedyExpandMacro(sim, c.macro, d);
                sum += s3.simulate(sim, TeamSide::HOME);
                if (res.turnover) nTO++;
            }
            printf("    %-28s meanQ=%+0.4f p(TO)=%.3f\n",
                   macroLabel(c.macro).c_str(), sum / K, (double)nTO / K);
        }
    }
    { // where does ADVANCE actually take the carrier? show the pacing math.
        printf("\n=== carrierStallAwareSteps context ===\n");
        printf("  dist=25 turnsRemaining=5 -> idealStepsThisTurn=ceil(25/5)=5\n");
        printf("  id21 adjacent (blitzable) -> steps=min(5, mvRemaining=5)=5\n");
        printf("  simulate() pacing: idealDist=turnsLeft*MA=5*5=25 == dist -> pacing=1.0 (MAX at own goal line)\n");
        printf("  hypothetical carrier x -> leaf total (all else fixed, id21 removed):\n");
        GameState s2 = makeState(2);
        s2.getPlayer(21).state = PlayerState::KO;  // remove marker for clean gradient
        s2.getPlayer(21).position = {0, 0};
        for (int x = 0; x <= 12; ++x) {
            s2.getPlayer(3).position = {(int8_t)x, 4};
            s2.ball.position = {(int8_t)x, 4};
            double v = search.simulate(s2, TeamSide::HOME);
            std::map<std::string, double> T;
            simMirror(s2, TeamSide::HOME, false, &T);
            printf("    x=%2d total=%+0.4f  prox=%+0.4f pacing=%+0.4f cage=%+0.4f (cageN=%g)\n",
                   x, v, T.count("SB:proximity") ? T["SB:proximity"] : 0,
                   T.count("SB:stallPacing") ? T["SB:stallPacing"] : 0,
                   T.count("SB:cageAdvance") ? T["SB:cageAdvance"] : 0,
                   T.count("(cageN)") ? T["(cageN)"] : 0);
        }
    }

    printf("\nDONE\n");
    return 0;
}
