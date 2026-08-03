// diag_f1_cage_advance_harness.cpp  (2026-08-03, F1 CAGE_ADVANCE validation)
//
// Standalone READ-ONLY paired A/B harness for the cage-advance staged plan
// (bb/cage_advance.h, MCTSConfig::cageAdvance). Head-to-head fairtest
// pattern (diag_policy_confirm_20260731.py): candidate = production config
// + cageAdvance=true, baseline = production config; each seed is played as
// a side-swapped pair (candidate home, then candidate away) with the same
// base seed, on a FIXED matchup list targeted at the dwarf playstyle gap
// (evidence/fable_dwarf_playstyle_gap_20260803.md):
//
//     0  dwarf  - skaven     (primary: baseline dwarf 26.0% decisive)
//     1  dwarf  - wood-elf   (primary: baseline dwarf  9.7% decisive)
//     2  dwarf  - dwarf      (mirror -- never played by the period-5 cycle)
//     3  orc    - skaven     (generalization control: no regression)
//
// PRE-REGISTERED thresholds (evidence/fable_f0f1_cage_advance_20260803.md,
// do not move after launch):
//   - per dwarf matchup: paired per-race delta chess (cand-as-dwarf minus
//     base-as-dwarf within seed pair, section 3b reading) >= +3 pp;
//   - orc-skaven control: |paired delta| within 2 SE of 0 (no regression).
//
// NEW METRICS -- attrition/survival: end-of-game KO / INJURED / DEAD /
// EJECTED counts per team, read from the final GameState (the GameResult
// object does not expose them, so the harness runs its own copy of the
// simulateGame loop and keeps the state). Aggregated per race by whether
// the OPPONENT had the cage-advance gate on -- this directly answers "how
// many skaven survive the match into the next tournament round" with and
// without the dwarf grind.
//
// Build (worktree root, after engine/build/libbb_engine.so exists):
//   g++ -O2 -std=c++20 -Iengine/include -Iengine/third_party \
//       diag_f1_cage_advance_harness.cpp \
//       -Lengine/build -lbb_engine -Wl,-rpath,$PWD/engine/build \
//       -o diag_f1_cage_advance
//
// FULL PLAN (do NOT run while training is live): 400 pairs x 4 matchups =
// 3200 full games at MCTS-100 both sides -- rough order 10-20 h per matchup
// on one core. Run one matchup per process, niced:
//   for m in 0 1 2 3; do
//     nice -n 19 ./diag_f1_cage_advance . 400 $m \
//         > diag_f1_m${m}_$(date +%Y%m%d).log 2>&1 &
//   done
//
// MINI-SMOKE (the only mode run before validation; 1 pair x 1 matchup =
// 2 games x 2 arms... no: head-to-head means 1 pair = 2 games total):
//   nice -n 19 ./diag_f1_cage_advance . 2 0     # 4 games, one matchup
//
// argv: [repoRoot=.] [nPairs=400] [matchupFilter=-1 (all)]
//
// Per-game JSONL rows go to diag_f1_cage_advance_rows.jsonl (append) in the
// F0 row format {seed_idx, cand_home, race_h, race_a, cand, base, ...} so
// python/blood_bowl/fairtest_schedule.per_race_paired can re-analyze them.

#include "bb/game_state.h"
#include "bb/game_simulator.h"
#include "bb/action_resolver.h"
#include "bb/macro_mcts.h"
#include "bb/mcts.h"
#include "bb/roster.h"
#include "bb/rules_engine.h"
#include "bb/value_function.h"
#include "bb/policy_network.h"
#include "bb/dice.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>

using namespace bb;

static constexpr uint32_t SEED_BASE = 34'000'000;  // disjoint from 30M/31M runs

struct Matchup {
    const char* home;
    const char* away;
};
static const Matchup MATCHUPS[] = {
    {"dwarf", "skaven"},
    {"dwarf", "wood-elf"},
    {"dwarf", "dwarf"},
    {"orc", "skaven"},
};
static constexpr int N_MATCHUPS = 4;

struct SideAttrition {
    int ko = 0, injured = 0, dead = 0, ejected = 0;
    int survivors(int fielded = 11) const { return fielded - injured - dead; }
};

struct FullGameOutcome {
    int homeScore = 0, awayScore = 0;
    SideAttrition home, away;
    int totalActions = 0;
};

// Copy of simulateGame's loop (game_simulator.cpp) that KEEPS the final
// GameState: the public API only returns scores, and the end-of-game
// PlayerStates are exactly what the attrition metric needs.
static FullGameOutcome playGame(const TeamRoster& home, const TeamRoster& away,
                                ActionSelector homePolicy, ActionSelector awayPolicy,
                                uint32_t seed) {
    GameState state;
    FullGameOutcome out;
    DiceRoller dice(seed);
    constexpr int MAX_ACTIONS = 5000;

    const TeamSide openingKickingTeam = TeamSide::AWAY;
    state.half = 1;
    state.kickingTeam = openingKickingTeam;
    setupHalf(state, home, away, state.kickingTeam);
    simpleKickoff(state, dice);

    std::vector<Action> actions;
    int totalActions = 0;
    while (state.phase != GamePhase::GAME_OVER && totalActions < MAX_ACTIONS) {
        if (state.phase == GamePhase::TOUCHDOWN) {
            state.kickingTeam = state.getPlayer(state.ball.carrierId).teamSide;
            setupDrive(state, home, away, state.kickingTeam);
            simpleKickoff(state, dice);
            continue;
        }
        if (state.phase == GamePhase::HALF_TIME) {
            state.half = 2;
            state.kickingTeam = opponent(openingKickingTeam);
            setupHalf(state, home, away, state.kickingTeam);
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
        ActionSelector& policy = (state.activeTeam == TeamSide::HOME)
                                     ? homePolicy : awayPolicy;
        Action chosen = policy(state);
        executeAction(state, chosen, dice, nullptr);
        totalActions++;
    }

    out.homeScore = state.homeTeam.score;
    out.awayScore = state.awayTeam.score;
    out.totalActions = totalActions;
    for (const Player& p : state.players) {
        if (p.id <= 0) continue;
        SideAttrition& side = (p.teamSide == TeamSide::HOME) ? out.home : out.away;
        switch (p.state) {
            case PlayerState::KO: side.ko++; break;
            case PlayerState::INJURED: side.injured++; break;
            case PlayerState::DEAD: side.dead++; break;
            case PlayerState::EJECTED: side.ejected++; break;
            default: break;
        }
    }
    return out;
}

static MCTSConfig makeConfig(const ValueFunction* /*vf*/, const PolicyNetwork* pol,
                             bool cageAdvanceOn) {
    // Champion fairtest config (diag_policy_confirm_20260731.py): MCTS-100,
    // vf_blend 0.15, policy net loaded with blend 0 => heuristic prior
    // floors ACTIVE (expand() only computes floors when a policy is set).
    MCTSConfig cfg;
    cfg.maxIterations = 100;
    cfg.timeBudgetMs = 0;
    cfg.explorationC = 1.0;
    cfg.dirichletAlpha = 0.0f;
    cfg.vfBlend = 0.15f;
    cfg.nRollouts = 1;
    cfg.policy = pol;
    cfg.policyBlend = 0.0f;
    cfg.cageAdvance = cageAdvanceOn;
    return cfg;
}

struct PairResult {
    // chess scores of the CANDIDATE side in each orientation
    double chessCandHome = 0.0, chessCandAway = 0.0;
    // section-3b paired per-race delta for the HOME race of the matchup:
    // cand-as-homeRace chess (orientation 1) minus base-as-homeRace chess
    // (orientation 2) = chessCandHome + chessCandAway - 1.
    double deltaHomeRace() const { return chessCandHome + chessCandAway - 1.0; }
};

int main(int argc, char** argv) {
    std::string root = (argc > 1) ? argv[1] : ".";
    int nPairs = (argc > 2) ? atoi(argv[2]) : 400;
    int matchupFilter = (argc > 3) ? atoi(argv[3]) : -1;
    setvbuf(stdout, nullptr, _IOLBF, 0);

    auto vf = loadValueFunction(root + "/weights_best.json");
    auto pol = loadPolicyNetworkFromFile(root + "/weights_policy.json");
    printf("vf=%s policy=%s (fairtest config: iters=100, vfBlend=0.15, "
           "policyBlend=0 w/ policy loaded => prior floors ACTIVE)\n",
           vf ? "loaded" : "NULL", pol ? "loaded" : "NULL");
    if (!vf || !pol) {
        fprintf(stderr, "weights_best.json + weights_policy.json required "
                        "(pass the repo root as argv[1])\n");
        return 1;
    }

    FILE* rows = fopen("diag_f1_cage_advance_rows.jsonl", "a");

    // attrition aggregation: race -> opponent-gate-on? -> sums
    struct AttrAgg {
        long n = 0, ko = 0, injured = 0, dead = 0, ejected = 0;
    };
    std::map<std::string, std::map<bool, AttrAgg>> attrition;

    for (int mi = 0; mi < N_MATCHUPS; ++mi) {
        if (matchupFilter >= 0 && mi != matchupFilter) continue;
        const Matchup& mu = MATCHUPS[mi];
        const TeamRoster* homeRoster = getDevelopedRoster(mu.home, 1200);
        const TeamRoster* awayRoster = getDevelopedRoster(mu.away, 1200);
        if (!homeRoster || !awayRoster) {
            fprintf(stderr, "roster load failed %s/%s\n", mu.home, mu.away);
            return 1;
        }
        printf("\n=== matchup %d: %s (home race) vs %s ===\n", mi, mu.home, mu.away);

        std::vector<PairResult> pairs;
        int candW = 0, candD = 0, candL = 0;
        long candPlansTotal = 0;  // "did the gate even fire" diagnostics
        for (int i = 0; i < nPairs; ++i) {
            uint32_t seed = SEED_BASE + static_cast<uint32_t>(mi) * 1'000'000u
                            + static_cast<uint32_t>(i);
            PairResult pr;
            for (int orient = 0; orient < 2; ++orient) {
                bool candHome = (orient == 0);
                MCTSConfig candCfg = makeConfig(vf.get(), pol.get(), true);
                MCTSConfig baseCfg = makeConfig(vf.get(), pol.get(), false);
                MacroMCTSPolicy homePol(vf.get(),
                                        candHome ? candCfg : baseCfg,
                                        seed * 2654435761u + 11u + orient);
                MacroMCTSPolicy awayPol(vf.get(),
                                        candHome ? baseCfg : candCfg,
                                        seed * 2654435761u + 47u + orient);
                FullGameOutcome g = playGame(
                    *homeRoster, *awayRoster,
                    [&](const GameState& s) { return homePol(s); },
                    [&](const GameState& s) { return awayPol(s); },
                    seed * 2u + static_cast<uint32_t>(orient));

                int candPlans = (candHome ? homePol : awayPol).stagedPlansAdopted();
                candPlansTotal += candPlans;
                int cs = candHome ? g.homeScore : g.awayScore;
                int bs = candHome ? g.awayScore : g.homeScore;
                double chess = cs > bs ? 1.0 : (cs < bs ? 0.0 : 0.5);
                if (candHome) pr.chessCandHome = chess;
                else pr.chessCandAway = chess;
                if (cs > bs) candW++;
                else if (cs < bs) candL++;
                else candD++;

                // attrition rows: each side keyed by its race and whether
                // its OPPONENT ran the cage-advance gate
                struct SideRow {
                    const char* race;
                    const SideAttrition* at;
                    bool oppGateOn;
                } sides[2] = {
                    {mu.home, &g.home, !candHome},  // home faces away side
                    {mu.away, &g.away, candHome},
                };
                for (auto& sr : sides) {
                    AttrAgg& a = attrition[sr.race][sr.oppGateOn];
                    a.n++;
                    a.ko += sr.at->ko;
                    a.injured += sr.at->injured;
                    a.dead += sr.at->dead;
                    a.ejected += sr.at->ejected;
                }
                if (rows) {
                    fprintf(rows,
                            "{\"matchup\":%d,\"seed_idx\":%d,\"cand_home\":%s,"
                            "\"race_h\":\"%s\",\"race_a\":\"%s\",\"cand\":%d,"
                            "\"base\":%d,\"home_attr\":[%d,%d,%d,%d],"
                            "\"away_attr\":[%d,%d,%d,%d],\"actions\":%d,"
                            "\"cand_plans\":%d}\n",
                            mi, i, candHome ? "true" : "false", mu.home, mu.away,
                            cs, bs, g.home.ko, g.home.injured, g.home.dead,
                            g.home.ejected, g.away.ko, g.away.injured,
                            g.away.dead, g.away.ejected, g.totalActions, candPlans);
                    fflush(rows);
                }
            }
            pairs.push_back(pr);
            if ((i + 1) % 25 == 0 || i + 1 == nPairs) {
                double sum = 0;
                for (auto& p : pairs) sum += p.deltaHomeRace();
                printf("  [%d/%d pairs] cand W%d D%d L%d | paired delta(%s) "
                       "so far %+.4f\n", i + 1, nPairs, candW, candD, candL,
                       mu.home, sum / pairs.size());
            }
        }

        // paired per-race (home race) summary -- section 3b reading
        double n = static_cast<double>(pairs.size());
        double sum = 0, sum2 = 0;
        for (auto& p : pairs) {
            double d = p.deltaHomeRace();
            sum += d;
            sum2 += d * d;
        }
        double mean = sum / n;
        double sd = std::sqrt(std::max(0.0, sum2 / n - mean * mean));
        double se = sd / std::sqrt(n);
        int dec = candW + candL;
        double wr = dec > 0 ? static_cast<double>(candW) / dec : 0.0;
        printf("SUMMARY matchup %d (%s vs %s), %d pairs (%d games):\n",
               mi, mu.home, mu.away, static_cast<int>(n), 2 * static_cast<int>(n));
        printf("  cand overall: W%d D%d L%d decisiveWR=%.3f chess=%.4f | "
               "cage plans adopted %.2f/game\n",
               candW, candD, candL, wr,
               (candW + 0.5 * candD) / (2.0 * n),
               candPlansTotal / (2.0 * n));
        printf("  PAIRED delta chess as %s: %+.4f +- %.4f SE (~%.1f SE) "
               "[pre-reg: >= +0.03 on dwarf matchups; control within 2 SE]\n",
               mu.home, mean, se, se > 0 ? mean / se : 0.0);
    }

    printf("\n=== ATTRITION / SURVIVAL (end-of-game states per race, split by "
           "whether the OPPONENT ran cageAdvance) ===\n");
    printf("%-10s %-14s %6s %8s %8s %8s %8s %10s\n", "race", "opp gate", "games",
           "KO/g", "INJ/g", "DEAD/g", "EJ/g", "surv/11");
    for (auto& [race, byGate] : attrition) {
        for (auto& [gate, a] : byGate) {
            if (a.n == 0) continue;
            double inj = static_cast<double>(a.injured) / a.n;
            double dead = static_cast<double>(a.dead) / a.n;
            printf("%-10s %-14s %6ld %8.2f %8.2f %8.2f %8.2f %10.2f\n",
                   race.c_str(), gate ? "cageAdvance ON" : "off", a.n,
                   static_cast<double>(a.ko) / a.n, inj, dead,
                   static_cast<double>(a.ejected) / a.n, 11.0 - inj - dead);
        }
    }
    if (rows) fclose(rows);
    printf("\nDONE\n");
    return 0;
}
