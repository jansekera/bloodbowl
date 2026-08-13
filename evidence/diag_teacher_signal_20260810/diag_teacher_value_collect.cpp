// diag_teacher_value_collect.cpp  (2026-08-10, teacher-signal test 1)
//
// STANDALONE READ-ONLY harness: replays the exact fairtest games of the
// feature-A/B corpus (same config MCTS-100, C=1.0, dirichlet 0, vfBlend 0.15,
// policy loaded blend 0; same seeds) and, at every fresh macro decision with
// >=3 REPOSITION candidates, DIRECTLY evaluates the resulting position of
// each REPOS candidate with the search's own leaf eval:
//   v015  = MacroMCTSSearch::evaluateLeaf, vfBlend 0.15 (the teacher signal)
//   vheur = same with vfBlend 0.0 (pure heuristic + scoringBonus)
//   vnn   = raw NN value function on extracted features
// Resulting position = clone + teleport actor to targetPos (ball follows if
// carried), movementRemaining -= chebyshev (min 0), hasMoved/hasActed = true.
// Pre-registered protocol: evidence/fable_teacher_signal_report_20260810.md §1.1.
//
// Build:
//   g++ -O2 -std=c++20 -I$ROOT/engine/include -I$ROOT/engine/third_party \
//       diag_teacher_value_collect.cpp -L$ROOT/engine/build -lbb_engine \
//       -Wl,-rpath,$ROOT/engine/build -o diag_teacher_value_collect
//
// argv: repoRoot raceHome raceAway nGames seedBase outJsonl

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
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

using namespace bb;

static float progressOf(TeamSide side, int x) {
    int p = (side == TeamSide::HOME) ? x : (25 - x);
    if (p < 0) p = 0;
    if (p > 25) p = 25;
    return p / 25.0f;
}

struct Evaluators {
    MacroMCTSSearch eval015;
    MacroMCTSSearch evalHeur;
    const ValueFunction* vf;

    Evaluators(const ValueFunction* v, MCTSConfig cfgBase, uint32_t seed)
        : eval015(v, cfgBase, seed ^ 0x51ed270bu),
          evalHeur(v, [&] {
              MCTSConfig c = cfgBase;
              c.vfBlend = 0.0f;
              return c;
          }(), seed ^ 0x2545f491u),
          vf(v) {}

    void evalState(const GameState& st, TeamSide persp, double* out3) {
        out3[0] = eval015.evaluateLeaf(st, persp);
        out3[1] = evalHeur.evaluateLeaf(st, persp);
        float feats[NUM_FEATURES];
        extractFeatures(st, persp, feats);
        out3[2] = vf->evaluate(feats, NUM_FEATURES);
    }
};

// Deterministic "success" application of a REPOSITION: teleport.
// Returns false if the candidate cannot be applied cleanly.
static bool applyReposition(GameState& st, int pid, Position tgt) {
    if (pid <= 0 || pid > 22) return false;
    Player& p = st.getPlayer(pid);
    if (!p.isOnPitch() || !tgt.isOnPitch()) return false;
    if (st.getPlayerAtPosition(tgt) != nullptr) return false;
    int cheb = p.position.distanceTo(tgt);
    if (st.ball.isHeld && st.ball.carrierId == pid) st.ball.position = tgt;
    p.position = tgt;
    p.movementRemaining =
        static_cast<int8_t>(std::max(0, p.movementRemaining - cheb));
    p.hasMoved = true;
    p.hasActed = true;
    return true;
}

struct LoggingMacroPolicy {
    MacroMCTSSearch search;
    DiceRoller expansionDice;
    Evaluators evals;
    std::vector<Action> plan;
    size_t planIdx = 0;

    FILE* out;
    const char* raceH;
    const char* raceA;
    uint32_t seed;
    int* decisionCounter;

    LoggingMacroPolicy(const ValueFunction* vf, MCTSConfig cfg, uint32_t s,
                       FILE* o, const char* rh, const char* ra,
                       uint32_t gameSeed, int* ctr)
        : search(vf, cfg, s), expansionDice(s ^ 0x9e3779b9u),
          evals(vf, cfg, s), out(o), raceH(rh), raceA(ra), seed(gameSeed),
          decisionCounter(ctr) {}

    void logDecision(const GameState& state) {
        int di = (*decisionCounter)++;
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

        int nRepos = 0;
        for (int i = 0; i < k; ++i) {
            if (sorted[i].macro.type == MacroType::REPOSITION) nRepos++;
        }
        if (nRepos < 3) return;

        TeamSide persp = state.activeTeam;
        double base[3];
        evals.evalState(state, persp, base);
        // determinism check: same state evaluated twice must match exactly
        double base2[3];
        evals.evalState(state, persp, base2);
        if (base[0] != base2[0]) {
            fprintf(stderr, "NONDETERMINISTIC leaf eval at di=%d\n", di);
        }

        fprintf(out,
                "{\"seed\":%u,\"race_h\":\"%s\",\"race_a\":\"%s\","
                "\"persp\":\"%s\",\"di\":%d,\"n_cands\":%d,\"n_repos\":%d,"
                "\"v015_base\":%.6g,\"vheur_base\":%.6g,\"vnn_base\":%.6g,"
                "\"cands\":[",
                seed, raceH, raceA,
                persp == TeamSide::HOME ? "home" : "away", di, k, nRepos,
                base[0], base[1], base[2]);
        bool first = true;
        for (int i = 0; i < k; ++i) {
            const Macro& mc = sorted[i].macro;
            if (mc.type != MacroType::REPOSITION) continue;
            GameState st = state.clone();
            if (!applyReposition(st, mc.playerId, mc.targetPos)) continue;
            double v[3];
            evals.evalState(st, persp, v);
            const Player& actor = state.getPlayer(mc.playerId);
            int tzTgt = countTacklezones(state, mc.targetPos, actor.teamSide,
                                         actor.id);
            float prog = progressOf(actor.teamSide, mc.targetPos.x);
            fprintf(out,
                    "%s{\"pid\":%d,\"tx\":%d,\"ty\":%d,\"n\":%d,\"v\":%.5g,"
                    "\"pr\":%.5g,\"v015\":%.6g,\"vheur\":%.6g,\"vnn\":%.6g,"
                    "\"tz\":%d,\"prog\":%.4g}",
                    first ? "" : ",", mc.playerId,
                    static_cast<int>(mc.targetPos.x),
                    static_cast<int>(mc.targetPos.y), sorted[i].visits,
                    static_cast<double>(sorted[i].visits) / total,
                    sorted[i].prior, v[0], v[1], v[2], tzTgt, prog);
            first = false;
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
    printf("config: iters=100 C=1.0 dirichlet=0 vfBlend=0.15 policyBlend=0, "
           "%s vs %s, %d games, seeds %u+ (teacher-value replay)\n",
           raceH, raceA, nGames, seedBase);

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
    cfg.maxIterations = 100;
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
