// diag_blitz_offense_floor_harness.cpp  (2026-07-23)
//
// Standalone READ-ONLY diagnostic for the BLITZ offense-side prior-floor gap
// (project_bloodbowl_blitz_offense_floor_gap_20260722). Template method:
// diag_advance_vs_block_harness.cpp (2026-07-16 ADVANCE floor diagnosis).
//
// Question: on OFFENSE, MacroType::BLITZ has NO prior floor
// (engine/src/macro_mcts.cpp:365-367 -- `if (onDef) minPrior = 0.20f;`),
// while BLOCK/CAGE/ADVANCE get unconditional 0.12 floors. Does that
// structurally starve BLITZ of MCTS visits in real offensive states where
// BLITZ is objectively the best (highest one-ply Q) macro?
//
// Method:
//   1. 8 REAL turn-start states mined from the 24-game 21.07 replay corpus
//      (mine_offense_blitz_states.py; criteria: offense, carrier marked,
//      free blitzer in range, assists present, no urgency floors active).
//   2. For each: real expand() root priors + one-ply Monte Carlo Q per root
//      macro (greedyExpandMacro real dice + real MacroMCTSSearch::simulate()).
//   3. For states where BLITZ has the top (or top-2) one-ply Q: full
//      MacroMCTSSearch::search() over 400 seeds with the production replay
//      config (iters=100, C=1.0, vfBlend=0, policy loaded w/ policyBlend=0
//      => heuristic prior floors ACTIVE). Report chosen% + avg visit share
//      per macro vs its prior and Q.
//
// The floor-gap hypothesis is CONFIRMED for a state if BLITZ has the top
// one-ply Q there but its visit share tracks its (floorless, renormalized-
// down) prior and it is (almost) never the chosen macro.
//
// NOTE: `#define private public` before including macro_mcts.h exposes
// expand()/simulate() -- same diagnostic-only trick as the ADVANCE harness;
// no engine source is modified.
//
// Build (repo root):
//   g++ -O2 -std=c++20 -Iengine/include -Iengine/third_party \
//       scratchpad/blitz_floor_20260723/diag_blitz_offense_floor_harness.cpp \
//       -Lengine/build -lbb_engine -Wl,-rpath,$PWD/engine/build \
//       -o scratchpad/blitz_floor_20260723/diag_blitz_offense_floor
//
// Optional argv: [repoRoot] [nSeeds] (defaults ".", 400)

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
// Real turn-start snapshots (mine_offense_blitz_states.py output).
// state ints: 0=STANDING, 1=PRONE; ids missing from snapshot -> KO.
// ---------------------------------------------------------------------------
struct Snap { int id, x, y, st; };

struct Cand {
    const char* label;
    const char* homeRace;
    const char* awayRace;
    TeamSide active;
    int half, turn;          // team turnNumber (both teams set equal)
    int homeScore, awayScore;
    int ballX, ballY, carrierId;
    std::vector<Snap> snaps;
    std::vector<int> koIds;  // missing from snapshot
};

static const std::vector<Cand> CANDS = {
    {"C1 g0003[20] H2 T3 away(wood-elf) 0-0 carrier21@(14,7) marked by dwarf id6",
     "dwarf", "wood-elf", TeamSide::AWAY, 2, 3, 0, 0, 14, 7, 21,
     {{1,13,4,0},{2,11,6,1},{3,14,10,1},{4,12,4,0},{5,11,7,0},{6,13,8,0},{7,12,3,0},
      {8,10,7,0},{9,7,3,0},{10,7,7,0},{11,11,5,0},{12,12,6,1},{13,14,9,0},{14,12,7,0},
      {15,13,11,0},{16,13,6,0},{17,14,6,0},{18,13,9,0},{19,13,10,0},{20,15,6,0},
      {21,14,7,0},{22,13,7,0}}, {}},
    {"C2 g0023[22] H2 T4 away(wood-elf) 0-0 carrier13@(14,7) marked x3 (id4,...)",
     "dwarf", "wood-elf", TeamSide::AWAY, 2, 4, 0, 0, 14, 7, 13,
     {{1,13,2,1},{2,11,6,1},{3,7,7,0},{4,14,6,0},{5,13,8,0},{6,6,6,0},{7,13,6,0},
      {8,7,6,0},{9,8,5,0},{10,11,4,0},{11,5,5,0},{12,13,3,1},{13,14,7,0},{15,14,9,0},
      {16,15,4,0},{17,15,5,0},{18,13,7,0},{19,15,7,0},{20,14,3,0},{21,13,4,0},
      {22,14,5,0}}, {14}},
    {"C3 g0009[20] H2 T3 away(human) 0-1 carrier22@(13,6) marked by elf id7",
     "wood-elf", "human", TeamSide::AWAY, 2, 3, 1, 0, 13, 6, 22,
     {{1,11,3,1},{2,11,8,1},{3,12,8,0},{4,13,3,1},{5,11,7,0},{6,10,8,0},{7,12,5,0},
      {8,6,6,0},{9,2,5,0},{10,0,7,0},{11,0,6,0},{13,14,7,0},{14,13,5,0},{15,12,7,1},
      {16,12,4,0},{17,15,6,0},{18,12,3,0},{19,12,6,0},{20,13,4,0},{21,16,9,0},
      {22,13,6,0}}, {12}},
    {"C4 g0002[4] H1 T3 home(skaven) 0-0 carrier11@(11,4) marked by dwarf id14",
     "skaven", "dwarf", TeamSide::HOME, 1, 3, 0, 0, 11, 4, 11,
     {{1,12,2,0},{2,13,4,0},{3,12,6,0},{4,11,5,0},{5,11,3,0},{6,14,3,0},{7,12,7,0},
      {8,12,4,0},{9,12,3,0},{10,12,8,0},{11,11,4,0},{12,14,5,1},{13,13,7,0},{14,12,5,0},
      {15,15,4,0},{16,12,0,0},{17,14,6,0},{18,14,7,0},{19,13,3,1},{20,18,4,0},
      {21,17,5,0},{22,18,7,0}}, {}},
    {"C5 g0004[2] H1 T2 home(wood-elf) 0-0 carrier11@(10,6) marked by human id15",
     "wood-elf", "human", TeamSide::HOME, 1, 2, 0, 0, 10, 6, 11,
     {{1,12,6,0},{2,13,8,1},{3,13,6,0},{4,13,7,0},{5,11,3,0},{6,11,6,0},{7,10,5,0},
      {8,12,8,0},{9,11,7,0},{10,10,4,0},{11,10,6,0},{12,13,2,0},{13,14,6,1},{14,14,9,0},
      {15,11,5,0},{16,13,4,0},{17,8,5,0},{18,15,4,0},{19,14,7,0},{20,16,4,0},
      {21,17,5,0},{22,17,7,0}}, {}},
    {"C6 g0012[2] H1 T2 home(skaven) 0-0 carrier8@(11,11) marked by dwarf id14",
     "skaven", "dwarf", TeamSide::HOME, 1, 2, 0, 0, 11, 11, 8,
     {{1,11,8,0},{2,12,5,1},{3,12,7,1},{4,10,9,0},{5,9,8,0},{6,10,10,0},{7,12,8,0},
      {8,11,11,0},{9,10,8,0},{10,10,11,0},{11,11,9,0},{12,9,6,0},{13,14,7,1},{14,11,10,0},
      {15,13,4,0},{16,13,7,0},{17,13,11,0},{18,14,11,0},{19,11,6,0},{20,18,11,0},
      {21,18,5,0},{22,18,7,0}}, {}},
    {"C7 g0007[4] H1 T3 home(skaven) 0-0 carrier1@(7,9) marked by dwarf id16",
     "skaven", "dwarf", TeamSide::HOME, 1, 3, 0, 0, 7, 9, 1,
     {{1,7,9,0},{2,5,10,0},{3,13,7,0},{4,8,8,0},{5,6,7,0},{6,13,4,0},{7,6,8,0},
      {8,9,10,1},{9,6,9,1},{10,9,8,0},{11,7,7,0},{12,14,5,1},{13,14,8,1},{14,8,11,0},
      {15,9,3,0},{16,7,8,0},{17,14,7,0},{18,7,11,1},{19,14,6,0},{20,12,11,0},
      {21,12,10,0},{22,14,4,0}}, {}},
    {"C8 g0009[22] H2 T4 away(human) 0-1 carrier22@(13,5) marked x2 (id3,...)",
     "wood-elf", "human", TeamSide::AWAY, 2, 4, 1, 0, 13, 5, 22,
     {{1,11,3,1},{2,11,8,1},{3,13,6,0},{4,14,2,1},{5,11,7,0},{6,8,6,0},{7,12,4,0},
      {8,12,1,0},{9,10,2,0},{10,6,5,0},{11,6,4,0},{13,14,7,0},{14,12,5,0},{15,12,7,1},
      {16,13,3,0},{17,14,5,0},{18,12,3,0},{19,12,6,0},{20,13,4,0},{21,16,9,0},
      {22,13,5,0}}, {12}},
};

static GameState makeState(const Cand& c, int rerolls) {
    GameState s;
    const TeamRoster* home = getDevelopedRoster(c.homeRace, 1200);
    const TeamRoster* away = getDevelopedRoster(c.awayRace, 1200);
    if (!home || !away) { fprintf(stderr, "roster load failed (%s/%s)\n", c.homeRace, c.awayRace); exit(1); }
    setupHalf(s, *home, *away, TeamSide::AWAY);  // populates stats/skills per id

    s.half = c.half;
    s.phase = GamePhase::PLAY;
    s.activeTeam = c.active;
    s.weather = Weather::NICE;
    s.kickingTeam = TeamSide::AWAY;  // not used by the macro search path
    s.turnoverPending = false;
    s.currentActivationId = -1;

    s.homeTeam.score = c.homeScore; s.awayTeam.score = c.awayScore;
    s.homeTeam.turnNumber = c.turn; s.awayTeam.turnNumber = c.turn;
    s.homeTeam.rerolls = rerolls; s.awayTeam.rerolls = rerolls;
    s.homeTeam.rerollUsedThisTurn = s.awayTeam.rerollUsedThisTurn = false;
    s.homeTeam.blitzUsedThisTurn = s.awayTeam.blitzUsedThisTurn = false;
    s.homeTeam.passUsedThisTurn = s.awayTeam.passUsedThisTurn = false;
    s.homeTeam.foulUsedThisTurn = s.awayTeam.foulUsedThisTurn = false;

    for (auto& p : s.players) {
        p.state = PlayerState::OFF_PITCH;
        p.position = {0, 0};
        p.hasMoved = p.hasActed = p.usedBlitz = false;
        p.lostTacklezones = p.proUsedThisTurn = false;
    }
    for (const auto& sn : c.snaps) {
        Player& p = s.getPlayer(sn.id);
        p.state = (sn.st == 0) ? PlayerState::STANDING : PlayerState::PRONE;
        p.position = {(int8_t)sn.x, (int8_t)sn.y};
        p.movementRemaining = p.stats.movement;
    }
    for (int id : c.koIds) s.getPlayer(id).state = PlayerState::KO;

    s.ball.position = {(int8_t)c.ballX, (int8_t)c.ballY};
    s.ball.isHeld = true;
    s.ball.carrierId = c.carrierId;
    return s;
}

static MCTSConfig makeConfig(const PolicyNetwork* pol) {
    // == replay production config (same as diag_advance_vs_block_harness.cpp):
    // mcts_iterations=100, exploration_c=1.0, dirichlet_alpha=0.0,
    // vf_blend=0.0, n_rollouts=1, leaf_lookahead=false,
    // policy loaded with policy_blend=0.0 (non-null => prior floors ACTIVE).
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

struct MacroStat {
    Macro macro;
    float prior = 0;
    double meanQ = 0, sd = 0, pTO = 0;
};

int main(int argc, char** argv) {
    std::string root = (argc > 1) ? argv[1] : ".";
    int nSeeds = (argc > 2) ? atoi(argv[2]) : 400;
    int KArg = (argc > 3) ? atoi(argv[3]) : 2000;
    bool forceFull = (argc > 4) && atoi(argv[4]) != 0;
    setvbuf(stdout, nullptr, _IOLBF, 0);  // line-buffered progress when piped
    auto vf = loadValueFunction(root + "/weights_best.json");
    auto pol = loadPolicyNetworkFromFile(root + "/weights_policy.json");
    printf("vf=%s policy=%s (vfBlend=0 => VF unused in leaf; policy non-null gates prior floors)\n",
           vf ? "loaded" : "NULL", pol ? "loaded" : "NULL");
    if (!pol) { fprintf(stderr, "policy required for floors to be active\n"); return 1; }

    const int K = KArg;  // one-ply MC samples per macro

    int confirmedStates = 0, blitzTopQStates = 0;

    for (const auto& cand : CANDS) {
        GameState state = makeState(cand, /*rerolls=*/2);
        MCTSConfig cfg = makeConfig(pol.get());
        MacroMCTSSearch search(vf.get(), cfg, 12345);

        MacroMCTSNode root_;
        root_.visits = 1;
        search.expand(&root_, state);

        printf("\n================================================================\n");
        printf("%s\n", cand.label);
        printf("root macros: n=%zu\n", root_.children.size());

        // one-ply MC Q per root macro
        std::vector<MacroStat> stats;
        for (auto& ch : root_.children) {
            DiceRoller d(777);
            double sum = 0, sum2 = 0; int nTO = 0;
            for (int k = 0; k < K; ++k) {
                GameState sim = state.clone();
                auto res = greedyExpandMacro(sim, ch.macro, d);
                double v = search.simulate(sim, cand.active);
                sum += v; sum2 += v * v;
                if (res.turnover) nTO++;
            }
            MacroStat ms;
            ms.macro = ch.macro;
            ms.prior = ch.prior;
            ms.meanQ = sum / K;
            ms.sd = std::sqrt(std::max(0.0, sum2 / K - ms.meanQ * ms.meanQ));
            ms.pTO = (double)nTO / K;
            stats.push_back(ms);
        }
        std::vector<int> byQ(stats.size());
        for (size_t i = 0; i < stats.size(); ++i) byQ[i] = (int)i;
        std::sort(byQ.begin(), byQ.end(), [&](int a, int b) { return stats[a].meanQ > stats[b].meanQ; });

        printf("%-30s %8s %8s %8s %6s  %s\n", "macro (sorted by one-ply Q)", "meanQ", "sd", "prior", "p(TO)", "Qrank/Prank");
        // prior ranks
        std::vector<int> byP(stats.size());
        for (size_t i = 0; i < stats.size(); ++i) byP[i] = (int)i;
        std::sort(byP.begin(), byP.end(), [&](int a, int b) { return stats[a].prior > stats[b].prior; });
        std::vector<int> prank(stats.size());
        for (size_t r = 0; r < byP.size(); ++r) prank[byP[r]] = (int)r + 1;

        int blitzQrank = -1;
        for (size_t r = 0; r < byQ.size(); ++r) {
            const MacroStat& ms = stats[byQ[r]];
            bool isBlitz = (ms.macro.type == MacroType::BLITZ);
            if (isBlitz && blitzQrank < 0) blitzQrank = (int)r + 1;
            printf("%-30s %+8.4f %8.4f %8.4f %6.3f  Q#%zu/P#%d%s\n",
                   macroLabel(ms.macro).c_str(), ms.meanQ, ms.sd, ms.prior, ms.pTO,
                   r + 1, prank[byQ[r]], isBlitz ? "   <== BLITZ" : "");
        }
        if (blitzQrank < 0) { printf("  (no BLITZ macro generated at root)\n"); continue; }
        printf("BLITZ one-ply Q rank: %d of %zu\n", blitzQrank, stats.size());

        if (blitzQrank > 3 && !forceFull) {
            printf("BLITZ not top-3 by Q here -- skipping full-search stage for this state.\n");
            continue;
        }
        blitzTopQStates++;

        // full production search over nSeeds
        std::map<std::string, int> chosen;
        std::map<std::string, double> visitShare;
        for (int i = 0; i < nSeeds; ++i) {
            MacroMCTSSearch s2(vf.get(), cfg, 1000 + i);
            Macro best = s2.search(state);
            chosen[macroLabel(best)]++;
            int tot = 0;
            for (auto& cv : s2.lastChildVisits()) tot += cv.visits;
            for (auto& cv : s2.lastChildVisits())
                visitShare[macroLabel(cv.macro)] += tot ? (double)cv.visits / tot : 0;
        }
        printf("\nFULL SEARCH x%d seeds (production config):\n", nSeeds);
        printf("%-30s %8s %12s %8s %8s\n", "macro", "chosen%", "visitShare%", "prior", "meanQ");
        std::vector<std::pair<double, std::string>> rows;
        std::map<std::string, const MacroStat*> byLabel;
        for (auto& ms : stats) byLabel[macroLabel(ms.macro)] = &ms;
        for (auto& [k, v] : chosen) rows.push_back({(double)v, k});
        for (auto& [k, v] : visitShare)
            if (!chosen.count(k)) rows.push_back({0.0, k});
        std::sort(rows.rbegin(), rows.rend());
        double blitzChosen = 0, blitzShare = 0, blitzPrior = 0, blitzQ = 0;
        for (auto& [v, k] : rows) {
            const MacroStat* ms = byLabel.count(k) ? byLabel[k] : nullptr;
            double ch = 100.0 * v / nSeeds;
            double vs = 100.0 * visitShare[k] / nSeeds;
            printf("%-30s %7.1f%% %11.1f%% %8.4f %+8.4f%s\n", k.c_str(), ch, vs,
                   ms ? ms->prior : -1.0, ms ? ms->meanQ : 0.0,
                   (ms && ms->macro.type == MacroType::BLITZ) ? "   <== BLITZ" : "");
            if (ms && ms->macro.type == MacroType::BLITZ && vs > blitzShare) {
                blitzChosen = ch; blitzShare = vs; blitzPrior = ms->prior; blitzQ = ms->meanQ;
            }
        }
        printf("\nVERDICT for this state: BLITZ Qrank=%d prior=%.4f visitShare=%.1f%% chosen=%.1f%%\n",
               blitzQrank, blitzPrior, blitzShare, blitzChosen);
        if (blitzQrank <= 2 && blitzChosen < 15.0) {
            printf("  -> UNDER-SELECTED despite top-2 Q: consistent with missing offense floor.\n");
            confirmedStates++;
        } else {
            printf("  -> BLITZ selected adequately here; floor gap not binding in this state.\n");
        }
    }

    printf("\n================================================================\n");
    printf("SUMMARY: %d/%zu states had BLITZ in top-2 one-ply Q; floor-gap symptom confirmed in %d of them.\n",
           blitzTopQStates, CANDS.size(), confirmedStates);
    printf("DONE\n");
    return 0;
}
