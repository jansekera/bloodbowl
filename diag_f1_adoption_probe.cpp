// diag_f1_adoption_probe (2026-08-04): WHY did the F1 A/B show
// "cage plans adopted 0.00/game" across all 3200 games?
//
// Plays real games with the PRODUCTION config (both sides base -- the cand
// arm's states are the same distribution, since the gated feature never
// fired) and, at the FIRST action selection of every team-turn (the exact
// point MacroMCTSPolicy::nextStagedMacro builds staged plans), probes:
//   - classifyTurnGoal histogram
//   - on ADVANCE_BALL: CageAdvancePlanner::build() verdict + diagnostics
//     (builtCorners, requiredPace, achievablePace, rawStep, resistance)
//
// Build:
//   g++ -O2 -std=c++20 -Iengine/include -Iengine/third_party \
//       diag_f1_adoption_probe.cpp \
//       -Lengine/build -lbb_engine -Wl,-rpath,$PWD/engine/build \
//       -o diag_f1_adoption_probe
// Run (measurement only, never alongside training):
//   ./diag_f1_adoption_probe . <nGames=8> <matchup:0=dwarf-skaven,2=dwarf-mirror>

#include "bb/game_state.h"
#include "bb/game_simulator.h"
#include "bb/action_resolver.h"
#include "bb/macro_mcts.h"
#include "bb/mcts.h"
#include "bb/roster.h"
#include "bb/rules_engine.h"
#include "bb/value_function.h"
#include "bb/policy_network.h"
#include "bb/turn_planner.h"
#include "bb/cage_advance.h"
#include "bb/dice.h"
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>

using namespace bb;

static constexpr uint32_t SEED_BASE = 35'000'000;  // disjoint from 30/31/34M

struct Matchup { const char* home; const char* away; };
static const Matchup MATCHUPS[] = {
    {"dwarf", "skaven"}, {"dwarf", "wood-elf"}, {"dwarf", "dwarf"}, {"orc", "skaven"},
};

struct ProbeAgg {
    long turns = 0;
    std::map<std::string, long> goals;
    // ADVANCE_BALL only:
    long advCarrierBusy = 0;
    std::map<int, long> advBuiltCorners;      // corners histogram at plan time
    std::map<std::string, long> verdicts;     // build() verdict histogram
    long tempoUsableLt1 = 0;                  // turnsLeft - reserve < 1
    long tempoPaceShort = 0;                  // achievable < required
    double reqPaceSum = 0, achPaceSum = 0;
    long paceSamples = 0;
    std::map<int, long> rawStepHist, resistanceHist;
};

static const char* goalName(TurnGoal g) {
    switch (g) {
        case TurnGoal::NONE: return "NONE";
        case TurnGoal::PICKUP_BALL: return "PICKUP";
        case TurnGoal::SCORE_BALL: return "SCORE";
        case TurnGoal::ADVANCE_BALL: return "ADVANCE";
    }
    return "?";
}
static const char* verdictName(CageAdvanceVerdict v) {
    switch (v) {
        case CageAdvanceVerdict::NOT_APPLICABLE: return "NOT_APPLICABLE";
        case CageAdvanceVerdict::TEMPO_INSUFFICIENT: return "TEMPO_INSUFFICIENT";
        case CageAdvanceVerdict::DICEY: return "DICEY";
        case CageAdvanceVerdict::PLAN_READY: return "PLAN_READY";
    }
    return "?";
}

static void probeTurn(const GameState& state, CageAdvancePlanner& planner,
                      ProbeAgg& agg) {
    agg.turns++;
    TurnGoal goal = classifyTurnGoal(state);
    agg.goals[goalName(goal)]++;
    if (goal != TurnGoal::ADVANCE_BALL) return;

    const Player& carrier = state.getPlayer(state.ball.carrierId);
    if (!carrier.canAct() || carrier.hasMoved || carrier.hasActed) {
        agg.advCarrierBusy++;
        return;
    }
    CageAdvancePlan plan = planner.build(state);
    agg.advBuiltCorners[plan.builtCorners]++;
    agg.verdicts[verdictName(plan.verdict)]++;
    if (plan.verdict == CageAdvanceVerdict::TEMPO_INSUFFICIENT) {
        const TeamState& my = state.getTeamState(carrier.teamSide);
        int turnsLeft = std::max(0, 9 - my.turnNumber);
        if (turnsLeft - CageAdvancePlanner::RESERVE_TURNS < 1) agg.tempoUsableLt1++;
        else agg.tempoPaceShort++;
        agg.reqPaceSum += plan.requiredPace;
        agg.achPaceSum += plan.achievablePace;
        agg.paceSamples++;
        agg.rawStepHist[plan.rawAchievableStep]++;
        agg.resistanceHist[plan.resistance]++;
    }
}

int main(int argc, char** argv) {
    std::string root = (argc > 1) ? argv[1] : ".";
    int nGames = (argc > 2) ? atoi(argv[2]) : 8;
    int mi = (argc > 3) ? atoi(argv[3]) : 0;
    setvbuf(stdout, nullptr, _IOLBF, 0);

    auto vf = loadValueFunction(root + "/weights_best.json");
    auto pol = loadPolicyNetworkFromFile(root + "/weights_best_policy.json");
    if (!vf || !pol) {
        fprintf(stderr, "weights_best.json + weights_best_policy.json required\n");
        return 1;
    }

    MCTSConfig cfg;
    cfg.maxIterations = 100;
    cfg.timeBudgetMs = 0;
    cfg.explorationC = 1.0;
    cfg.dirichletAlpha = 0.0f;
    cfg.vfBlend = 0.15f;
    cfg.nRollouts = 1;
    cfg.policy = pol.get();
    cfg.policyBlend = 0.0f;  // fairtest parity: floors active, no blend

    const Matchup& mu = MATCHUPS[mi];
    const TeamRoster* homeRoster = getDevelopedRoster(mu.home, 1200);
    const TeamRoster* awayRoster = getDevelopedRoster(mu.away, 1200);
    if (!homeRoster || !awayRoster) { fprintf(stderr, "roster load failed\n"); return 1; }
    printf("probe: %d games %s vs %s, production config (MCTS-100, vfBlend 0.15)\n",
           nGames, mu.home, mu.away);

    ProbeAgg agg;
    CageAdvancePlanner planner(vf.get(), cfg, 424242);

    for (int gi = 0; gi < nGames; ++gi) {
        uint32_t seed = SEED_BASE + static_cast<uint32_t>(mi) * 100'000u
                        + static_cast<uint32_t>(gi);
        MacroMCTSPolicy homePol(vf.get(), cfg, seed * 2654435761u + 11u);
        MacroMCTSPolicy awayPol(vf.get(), cfg, seed * 2654435761u + 47u);

        GameState state;
        DiceRoller dice(seed);
        constexpr int MAX_ACTIONS = 5000;
        const TeamSide openingKickingTeam = TeamSide::AWAY;
        state.half = 1;
        state.kickingTeam = openingKickingTeam;
        setupHalf(state, *homeRoster, *awayRoster, state.kickingTeam);
        simpleKickoff(state, dice);

        // team-turn transition tracking: probe at the FIRST action selection
        int lastHalf = -1, lastTurn = -1;
        TeamSide lastTeam = TeamSide::HOME;
        bool haveLast = false;

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
            const TeamState& ts = state.getTeamState(state.activeTeam);
            if (!haveLast || lastHalf != state.half || lastTurn != ts.turnNumber ||
                lastTeam != state.activeTeam) {
                probeTurn(state, planner, agg);
                haveLast = true;
                lastHalf = state.half;
                lastTurn = ts.turnNumber;
                lastTeam = state.activeTeam;
            }
            Action chosen = (state.activeTeam == TeamSide::HOME) ? homePol(state)
                                                                 : awayPol(state);
            executeAction(state, chosen, dice, nullptr);
            totalActions++;
        }
        printf("  game %d done (%d:%d, %d actions)\n", gi,
               state.homeTeam.score, state.awayTeam.score, totalActions);
    }

    printf("\n=== PROBE SUMMARY (%ld team-turns) ===\n", agg.turns);
    printf("goal histogram:\n");
    for (auto& [k, v] : agg.goals) printf("  %-8s %ld\n", k.c_str(), v);
    printf("ADVANCE turns with carrier busy/unable: %ld\n", agg.advCarrierBusy);
    printf("builtCorners at plan time (ADVANCE only):\n");
    for (auto& [k, v] : agg.advBuiltCorners) printf("  %d corners: %ld\n", k, v);
    printf("build() verdicts:\n");
    for (auto& [k, v] : agg.verdicts) printf("  %-18s %ld\n", k.c_str(), v);
    if (agg.paceSamples > 0) {
        printf("TEMPO_INSUFFICIENT detail: usable<1: %ld | pace short: %ld | "
               "mean requiredPace %.2f vs achievablePace %.2f (%ld samples)\n",
               agg.tempoUsableLt1, agg.tempoPaceShort,
               agg.reqPaceSum / agg.paceSamples, agg.achPaceSum / agg.paceSamples,
               agg.paceSamples);
        printf("rawAchievableStep hist:");
        for (auto& [k, v] : agg.rawStepHist) printf("  %d:%ld", k, v);
        printf("\nresistance hist:");
        for (auto& [k, v] : agg.resistanceHist) printf("  %d:%ld", k, v);
        printf("\n");
    }
    return 0;
}
