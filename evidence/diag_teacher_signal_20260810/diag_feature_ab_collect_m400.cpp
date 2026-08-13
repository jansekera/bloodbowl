// diag_feature_ab_collect.cpp  (2026-08-10, offline feature A/B — P0-b)
//
// STANDALONE READ-ONLY collection harness for the offline feature A/B
// (evidence/fable_offline_feature_ab_20260810.md). Plays full games with the
// production-fairtest search config (MCTS-100, C=1.0, dirichlet 0, vfBlend
// 0.15, policy net LOADED with blend 0 => heuristic prior floors active,
// exactly like diag_f1_cage_advance_harness.cpp mode 2 / era runs) and logs
// every fresh macro-search decision as one JSONL row:
//   - 73 state features + perspective + races + seed
//   - top-20 candidates by visits: visit count/fraction, root prior,
//     FULL macro identity (type, playerId, targetId, targetPos),
//     the production 23 action features (extractMacroFeatures)
//     and 16 NEW extended features computed here from the engine's own
//     rules API (never touching production code):
//       [0]  actor AG/6
//       [1]  actor MA (stats.movement)/9
//       [2]  actor movementRemaining/9
//       [3]  MA-normalized Chebyshev distance to effective target (clamp 0-2)/2
//       [4]  source progress along attack axis (0=own EZ, 1=opp EZ)
//       [5]  source lateral y/14
//       [6]  target progress
//       [7]  target lateral y/14
//       [8]  forward delta (signed)
//       [9]  lateral delta /14
//       [10] Chebyshev move distance /14
//       [11] actor identity ((id-1)%11)/10
//       [12] p(fail) estimate composed per macro type from REAL rules math:
//            dodge (calculateDodgeTarget incl. first approach step via
//            pickApproachStep), GFI count, pickup (calculatePickupTargetAt,
//            SureHands), block dice (countAssists+getBlockDiceInfo, Block
//            skill, attacker/defender chooses)
//       [13] enemy TZ count on actor's square /3
//       [14] enemy TZ count on effective target square (excl. actor) /3
//       [15] normalized primary roll target ((target-1)/6):
//            pickup target for PICKUP, first-step dodge target for
//            movement macros leaving a TZ; else 0
//
// Build:
//   g++ -O2 -std=c++20 -I$ROOT/engine/include -I$ROOT/engine/third_party \
//       diag_feature_ab_collect.cpp -L$ROOT/engine/build -lbb_engine \
//       -Wl,-rpath,$ROOT/engine/build -o diag_feature_ab_collect
//
// argv: repoRoot raceHome raceAway nGames seedBase outJsonl
// Seeds 92,000,000+ (disjoint from 30M/31M/34M/37M/51M/63M/91M runs).

#include "bb/game_state.h"
#include "bb/game_simulator.h"
#include "bb/action_resolver.h"
#include "bb/macro_mcts.h"
#include "bb/macro_actions.h"
#include "bb/mcts.h"
#include "bb/roster.h"
#include "bb/rules_engine.h"
#include "bb/value_function.h"
#include "bb/policy_network.h"
#include "bb/feature_extractor.h"
#include "bb/helpers.h"
#include "bb/dice.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

using namespace bb;

static constexpr int N_NEW_FEATURES = 16;

static int endzoneX(TeamSide side) { return side == TeamSide::HOME ? 25 : 0; }

static float progressOf(TeamSide side, int x) {
    int p = (side == TeamSide::HOME) ? x : (25 - x);
    if (p < 0) p = 0;
    if (p > 25) p = 25;
    return p / 25.0f;
}

// Effective target square of a macro (mirrors the expansion targets).
static Position effectiveTarget(const GameState& state, const Macro& macro,
                                const Player* actor) {
    switch (macro.type) {
        case MacroType::REPOSITION:
            return macro.targetPos;
        case MacroType::BLOCK:
        case MacroType::BLITZ:
        case MacroType::BLITZ_AND_SCORE:
        case MacroType::FOUL:
        case MacroType::PASS_ACTION:
        case MacroType::HAND_OFF_SCORE:
        case MacroType::PASS_SCORE:
        case MacroType::CHAIN_SCORE:
            if (macro.targetId > 0 && macro.targetId <= 22) {
                const Player& t = state.getPlayer(macro.targetId);
                if (t.isOnPitch()) return t.position;
            }
            return {-1, -1};
        case MacroType::PICKUP:
            if (!state.ball.isHeld) return state.ball.position;
            return {-1, -1};
        case MacroType::SCORE:
        case MacroType::ADVANCE:
            if (actor && actor->isOnPitch()) {
                return {static_cast<int8_t>(endzoneX(actor->teamSide)),
                        actor->position.y};
            }
            return {-1, -1};
        case MacroType::CAGE:
            if (state.ball.isHeld && state.ball.carrierId > 0) {
                const Player& c = state.getPlayer(state.ball.carrierId);
                if (c.isOnPitch()) return c.position;
            }
            if (!state.ball.isHeld) return state.ball.position;
            return {-1, -1};
        default:
            return {-1, -1};
    }
}

static float pFailRoll(int target) {  // P(d6 < target), target clamped 2-6
    if (target < 2) target = 2;
    if (target > 6) return 1.0f;  // defensive; engine clamps to 6
    return (target - 1) / 6.0f;
}

static void computeNewFeatures(const GameState& state, const Macro& macro,
                               float* nf) {
    for (int i = 0; i < N_NEW_FEATURES; ++i) nf[i] = 0.0f;
    if (macro.type == MacroType::END_TURN) return;

    const Player* actor = nullptr;
    if (macro.playerId > 0 && macro.playerId <= 22) {
        const Player& p = state.getPlayer(macro.playerId);
        if (p.isOnPitch()) actor = &p;
    }
    Position eff = effectiveTarget(state, macro, actor);

    if (actor) {
        nf[0] = actor->stats.agility / 6.0f;
        nf[1] = actor->stats.movement / 9.0f;
        nf[2] = actor->movementRemaining / 9.0f;
        nf[4] = progressOf(actor->teamSide, actor->position.x);
        nf[5] = actor->position.y / 14.0f;
        nf[11] = ((actor->id - 1) % 11) / 10.0f;
        int tzSrc = countTacklezones(state, actor->position, actor->teamSide);
        nf[13] = std::min(tzSrc, 3) / 3.0f;
    }
    if (eff.isOnPitch() && actor) {
        nf[6] = progressOf(actor->teamSide, eff.x);
        nf[7] = eff.y / 14.0f;
        nf[8] = nf[6] - nf[4];
        nf[9] = (eff.y - actor->position.y) / 14.0f;
        int cheb = actor->position.distanceTo(eff);
        nf[10] = std::min(cheb, 14) / 14.0f;
        float maNorm = static_cast<float>(cheb) /
                       std::max<int>(1, actor->movementRemaining);
        nf[3] = std::min(maNorm, 2.0f) / 2.0f;
        int tzTgt = countTacklezones(state, eff, actor->teamSide, actor->id);
        nf[14] = std::min(tzTgt, 3) / 3.0f;
    }

    // --- p(fail) composition [12] + primary roll target [15] ---
    float pDodge = 0.0f, pGfi = 0.0f, pPickup = 0.0f, pBlock = 0.0f,
          pOther = 0.0f;
    bool movementMacro =
        macro.type == MacroType::REPOSITION || macro.type == MacroType::ADVANCE ||
        macro.type == MacroType::SCORE || macro.type == MacroType::PICKUP ||
        macro.type == MacroType::BLITZ || macro.type == MacroType::BLITZ_AND_SCORE;

    if (actor && movementMacro && eff.isOnPitch() &&
        !(eff == actor->position)) {
        // dodge to leave current square (first step only — estimate)
        if (countTacklezones(state, actor->position, actor->teamSide) > 0) {
            Position step = pickApproachStep(state, *actor, actor->position, eff);
            if (step.isOnPitch() && !(step == actor->position)) {
                int t = calculateDodgeTarget(state, *actor, step,
                                             actor->position);
                pDodge = pFailRoll(t);
                if (actor->hasSkill(SkillName::Dodge)) pDodge *= pDodge;
                if (macro.type != MacroType::PICKUP) {
                    nf[15] = (std::min(std::max(t, 2), 7) - 1) / 6.0f;
                }
            }
        }
        // GFI squares
        int cheb = actor->position.distanceTo(eff);
        int dist = cheb;
        if (macro.type == MacroType::SCORE || macro.type == MacroType::ADVANCE) {
            dist = std::abs(actor->position.x - endzoneX(actor->teamSide));
        }
        int gfis = std::max(0, std::min(dist - actor->movementRemaining, 2));
        if (gfis > 0) pGfi = 1.0f - std::pow(5.0f / 6.0f, gfis);
    }

    if (macro.type == MacroType::PICKUP && actor && !state.ball.isHeld) {
        int t = calculatePickupTargetAt(state, *actor, state.ball.position);
        pPickup = pFailRoll(t);
        if (actor->hasSkill(SkillName::SureHands)) pPickup *= pPickup;
        nf[15] = (std::min(std::max(t, 2), 7) - 1) / 6.0f;
    }

    if ((macro.type == MacroType::BLOCK || macro.type == MacroType::BLITZ ||
         macro.type == MacroType::BLITZ_AND_SCORE) &&
        actor && macro.targetId > 0 && macro.targetId <= 22) {
        const Player& def = state.getPlayer(macro.targetId);
        if (def.isOnPitch()) {
            int attST = actor->stats.strength;
            if (macro.type != MacroType::BLOCK &&
                actor->hasSkill(SkillName::Horns)) {
                attST += 1;
            }
            int attAssists = countAssists(state, def.position, actor->teamSide,
                                          actor->id, def.id, def.id);
            int defAssists = countAssists(state, actor->position, def.teamSide,
                                          def.id, actor->id, actor->id);
            BlockDiceInfo info = getBlockDiceInfo(attST + attAssists,
                                                  def.stats.strength + defAssists);
            float perDie = (1.0f + (actor->hasSkill(SkillName::Block) ? 0.0f
                                                                       : 1.0f)) /
                           6.0f;
            if (info.attackerChooses) {
                pBlock = std::pow(perDie, static_cast<float>(info.count));
            } else {
                pBlock = 1.0f - std::pow(1.0f - perDie,
                                         static_cast<float>(info.count));
            }
        }
    }

    switch (macro.type) {
        case MacroType::PASS_ACTION:
        case MacroType::PASS_SCORE:
        case MacroType::CHAIN_SCORE:
            if (actor) {
                int t = std::min(std::max(7 - actor->stats.agility, 2), 6);
                pOther = pFailRoll(t);  // crude: throw roll only
            } else {
                pOther = 0.4f;
            }
            break;
        case MacroType::HAND_OFF_SCORE:
            pOther = 0.15f;
            break;
        case MacroType::FOUL:
            pOther = 1.0f / 6.0f;  // ejection proxy (doubles)
            break;
        case MacroType::CAGE:
            pOther = 0.05f;
            break;
        default:
            break;
    }

    float pOk = (1.0f - pDodge) * (1.0f - pGfi) * (1.0f - pPickup) *
                (1.0f - pBlock) * (1.0f - pOther);
    nf[12] = 1.0f - pOk;
}

// --- decision-logging macro policy: replicates MacroMCTSPolicy::operator()
// (macro_mcts.cpp:988-1092) minus staged planners, but keeps the FULL macro
// of every root candidate from lastChildVisits() for the JSONL log. ---
struct LoggingMacroPolicy {
    MacroMCTSSearch search;
    DiceRoller expansionDice;
    std::vector<Action> plan;
    size_t planIdx = 0;

    FILE* out;
    const char* raceH;
    const char* raceA;
    uint32_t seed;
    int* decisionCounter;

    LoggingMacroPolicy(const ValueFunction* vf, MCTSConfig cfg, uint32_t s,
                       FILE* o, const char* rh, const char* ra, uint32_t gameSeed,
                       int* ctr)
        : search(vf, cfg, s), expansionDice(s ^ 0x9e3779b9u), out(o), raceH(rh),
          raceA(ra), seed(gameSeed), decisionCounter(ctr) {}

    void logDecision(const GameState& state) {
        const auto& cvs = search.lastChildVisits();
        if (cvs.empty()) return;
        long total = 0;
        for (auto& cv : cvs) total += cv.visits;
        if (total <= 0) return;

        std::vector<MacroChildVisitInfo> sorted(cvs.begin(), cvs.end());
        std::sort(sorted.begin(), sorted.end(),
                  [](const MacroChildVisitInfo& a, const MacroChildVisitInfo& b) {
                      return a.visits > b.visits;
                  });
        int k = std::min<int>(20, static_cast<int>(sorted.size()));

        float sf[NUM_FEATURES];
        extractFeatures(state, state.activeTeam, sf);

        fprintf(out, "{\"seed\":%u,\"race_h\":\"%s\",\"race_a\":\"%s\","
                     "\"persp\":\"%s\",\"di\":%d,\"sf\":[",
                seed, raceH, raceA,
                state.activeTeam == TeamSide::HOME ? "home" : "away",
                (*decisionCounter)++);
        for (int i = 0; i < NUM_FEATURES; ++i) {
            fprintf(out, i ? ",%.5g" : "%.5g", sf[i]);
        }
        fprintf(out, "],\"cands\":[");
        for (int i = 0; i < k; ++i) {
            const Macro& mc = sorted[i].macro;
            float f[NUM_ACTION_FEATURES];
            extractMacroFeatures(state, mc, f);
            float nf[N_NEW_FEATURES];
            computeNewFeatures(state, mc, nf);
            fprintf(out, "%s{\"n\":%d,\"v\":%.5g,\"pr\":%.5g,\"t\":%d,"
                         "\"pid\":%d,\"tid\":%d,\"tx\":%d,\"ty\":%d,\"f\":[",
                    i ? "," : "", sorted[i].visits,
                    static_cast<double>(sorted[i].visits) / total,
                    sorted[i].prior, static_cast<int>(mc.type), mc.playerId,
                    mc.targetId, static_cast<int>(mc.targetPos.x),
                    static_cast<int>(mc.targetPos.y));
            for (int j = 0; j < NUM_ACTION_FEATURES; ++j) {
                fprintf(out, j ? ",%.5g" : "%.5g", f[j]);
            }
            fprintf(out, "],\"nf\":[");
            for (int j = 0; j < N_NEW_FEATURES; ++j) {
                fprintf(out, j ? ",%.5g" : "%.5g", nf[j]);
            }
            fprintf(out, "]}");
        }
        fprintf(out, "]}\n");
    }

    Action operator()(const GameState& state) {
        if (planIdx < plan.size()) {
            const Action& planned = plan[planIdx];
            std::vector<Action> available;
            getAvailableActions(state, available);
            for (auto& a : available) {
                if (a.type == planned.type && a.playerId == planned.playerId &&
                    a.targetId == planned.targetId && a.target == planned.target) {
                    planIdx++;
                    return planned;
                }
            }
            plan.clear();
            planIdx = 0;
        }

        Macro best = search.search(state);
        logDecision(state);

        GameState planState = state.clone();
        auto expansion = greedyExpandMacro(planState, best, expansionDice);
        plan = std::move(expansion.actions);
        planIdx = 0;

        if (plan.empty()) {
            return Action{ActionType::END_TURN, -1, -1, {-1, -1}};
        }

        std::vector<Action> available;
        getAvailableActions(state, available);
        const Action& first = plan[0];
        for (auto& a : available) {
            if (a.type == first.type && a.playerId == first.playerId &&
                a.targetId == first.targetId && a.target == first.target) {
                planIdx = 1;
                return first;
            }
        }
        plan.clear();
        return Action{ActionType::END_TURN, -1, -1, {-1, -1}};
    }
};

int main(int argc, char** argv) {
    if (argc < 7) {
        fprintf(stderr,
                "usage: %s repoRoot raceHome raceAway nGames seedBase out.jsonl\n",
                argv[0]);
        return 1;
    }
    std::string root = argv[1];
    const char* raceH = argv[2];
    const char* raceA = argv[3];
    int nGames = atoi(argv[4]);
    uint32_t seedBase = static_cast<uint32_t>(strtoul(argv[5], nullptr, 10));
    const char* outPath = argv[6];
    setvbuf(stdout, nullptr, _IOLBF, 0);

    auto vf = loadValueFunction(root + "/weights_best.json");
    auto pol = loadPolicyNetworkFromFile(root + "/weights_policy.json");
    if (!vf || !pol) {
        fprintf(stderr, "failed to load weights from %s\n", root.c_str());
        return 1;
    }
    printf("config: iters=400 C=1.0 dirichlet=0 vfBlend=0.15 policyBlend=0 "
           "(policy loaded => heuristic prior floors ACTIVE), %s vs %s, "
           "%d games, seeds %u+\n", raceH, raceA, nGames, seedBase);

    const TeamRoster* homeRoster = getDevelopedRoster(raceH, 1200);
    const TeamRoster* awayRoster = getDevelopedRoster(raceA, 1200);
    if (!homeRoster || !awayRoster) {
        fprintf(stderr, "roster load failed\n");
        return 1;
    }

    FILE* out = fopen(outPath, "a");
    if (!out) {
        fprintf(stderr, "cannot open %s\n", outPath);
        return 1;
    }

    MCTSConfig cfg;
    cfg.maxIterations = 400;
    cfg.timeBudgetMs = 0;
    cfg.explorationC = 1.0;
    cfg.dirichletAlpha = 0.0f;
    cfg.vfBlend = 0.15f;
    cfg.nRollouts = 1;
    cfg.policy = pol.get();
    cfg.policyBlend = 0.0f;

    for (int g = 0; g < nGames; ++g) {
        uint32_t seed = seedBase + static_cast<uint32_t>(g);
        int decisions = 0;
        LoggingMacroPolicy homePol(vf.get(), cfg, seed * 2654435761u + 11u, out,
                                   raceH, raceA, seed, &decisions);
        LoggingMacroPolicy awayPol(vf.get(), cfg, seed * 2654435761u + 47u, out,
                                   raceH, raceA, seed, &decisions);

        GameState state;
        DiceRoller dice(seed * 2u + 1u);
        constexpr int MAX_ACTIONS = 5000;
        const TeamSide openingKickingTeam = TeamSide::AWAY;
        state.half = 1;
        state.kickingTeam = openingKickingTeam;
        setupHalf(state, *homeRoster, *awayRoster, state.kickingTeam);
        simpleKickoff(state, dice);

        std::vector<Action> actions;
        int totalActions = 0;
        while (state.phase != GamePhase::GAME_OVER && totalActions < MAX_ACTIONS) {
            if (state.phase == GamePhase::TOUCHDOWN) {
                state.kickingTeam = state.getPlayer(state.ball.carrierId).teamSide;
                setupDrive(state, *homeRoster, *awayRoster, state.kickingTeam);
                simpleKickoff(state, dice);
                continue;
            }
            if (state.phase == GamePhase::HALF_TIME) {
                state.half = 2;
                state.kickingTeam = opponent(openingKickingTeam);
                setupHalf(state, *homeRoster, *awayRoster, state.kickingTeam);
                simpleKickoff(state, dice);
                continue;
            }
            actions.clear();
            getAvailableActions(state, actions);
            if (actions.empty()) {
                Action endTurn;
                endTurn.type = ActionType::END_TURN;
                executeAction(state, endTurn, dice, nullptr);
                totalActions++;
                continue;
            }
            Action chosen = (state.activeTeam == TeamSide::HOME)
                                ? homePol(state) : awayPol(state);
            executeAction(state, chosen, dice, nullptr);
            totalActions++;
        }
        fflush(out);
        printf("game %d/%d seed=%u score=%d:%d decisions=%d actions=%d\n",
               g + 1, nGames, seed, state.homeTeam.score, state.awayTeam.score,
               decisions, totalActions);
    }
    fclose(out);
    printf("DONE\n");
    return 0;
}
