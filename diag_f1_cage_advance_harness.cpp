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
// argv: [repoRoot=.] [nPairs=400] [matchupFilter=-1 (all)] [mode=0]
//
// mode 1 (2026-08-06 tempo doctrine, option (a) GRIND A/B): candidate =
// cageAdvance + cageGrind (on TEMPO_INSUFFICIENT push at max dice-free
// step), baseline = cageAdvance WITHOUT grind = today's fallback-to-search
// behavior. Seeds move to 37M (disjoint), rows go to
// diag_f1_grind_rows.jsonl.
//
// Per-game JSONL rows go to diag_f1_cage_advance_rows.jsonl (append) in the
// F0 row format {seed_idx, cand_home, race_h, race_a, cand, base, ...} so
// python/blood_bowl/fairtest_schedule.per_race_paired can re-analyze them.

#include "bb/game_state.h"
#include "bb/game_simulator.h"
#include "bb/action_resolver.h"
#include "bb/macro_mcts.h"
#include "bb/macro_actions.h"
#include "bb/block_handler.h"
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
    // 2026-08-14: APPENDED, never inserted -- the index is the argv matchup
    // filter and is written into every row already on disk, so reordering this
    // table would silently relabel past runs.
    //
    // dwarf-orc was missing, and it is the matchup that decides several of the
    // questions we keep asking: we score 86 touchdowns against orcs over 750
    // games against 451 against Skaven, and skills that key on strength (P13
    // Dauntless equalising onto ST4) or on Block (P17 Wrestle) only ever fire
    // here -- Skaven cap at ST3, so a Dauntless arm run on dw-sk measures
    // nothing at all.
    {"dwarf", "orc"},
    // 2026-08-14: a human too, so that a dwarf A/B finally covers all four
    // opponents. Every A/B this project has ever run -- the cage gate, bundle G,
    // everything -- used dw-sk and dw-we only, i.e. the two FAST teams, which
    // are exactly the matchups where our doctrine flatters itself. Orcs and
    // humans, our two worst results, were never in an arm at all.
    {"dwarf", "human"},
};
static constexpr int N_MATCHUPS = 6;

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
                             bool cageAdvanceOn, bool cageGrindOn = false,
                             float policyBlend = 0.0f, bool dauntlessOn = false) {
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
    cfg.policyBlend = policyBlend;
    cfg.dauntlessInOffer = dauntlessOn;
    cfg.cageAdvance = cageAdvanceOn;
    cfg.cageGrind = cageGrindOn;
    return cfg;
}

struct PairResult {
    // chess scores of the CANDIDATE side in each orientation
    double chessCandHome = 0.0, chessCandAway = 0.0;
    // section-3b paired per-race delta for the HOME race of the matchup:
    // cand-as-homeRace chess (orientation 1) minus base-as-homeRace chess
    // (orientation 2) = chessCandHome + chessCandAway - 1.
    double deltaHomeRace() const { return chessCandHome + chessCandAway - 1.0; }

    // 2026-08-17: how many times the ARM ITSELF did something in this pair --
    // cage plans adopted for modes 0/1, Dauntless offers for mode 4. Under CRN
    // both orientations play the identical game, so a pair where the arm never
    // acted MUST come back at delta 0. One that does not is a leak: the flag
    // changed the run through some path other than the feature. That check is
    // per pair and inside the real run, which makes it far stronger than any
    // separate control leg -- see the note on mode 2 below.
    long armEvents = 0;
};

int main(int argc, char** argv) {
    std::string root = (argc > 1) ? argv[1] : ".";
    int nPairs = (argc > 2) ? atoi(argv[2]) : 400;
    int matchupFilter = (argc > 3) ? atoi(argv[3]) : -1;
    int mode = (argc > 4) ? atoi(argv[4]) : 0;  // 1 = grind A/B, 2 = era, 3 = M1
    // Shard offset (2026-08-13): one matchup runs in one process, so a run
    // long enough to see past the +-5.3pp noise floor took a whole day on one
    // core while eleven sat idle. The offset shifts the seed index, so the
    // same matchup can be split across processes -- shard k covers pairs
    // [k*nPairs, (k+1)*nPairs). Offset 0 reproduces every earlier run exactly.
    int seedOffset = (argc > 5) ? atoi(argv[5]) : 0;
    // mode 2 (2026-08-10, rules-parity era comparison): BOTH sides run the
    // production config, so there is no candidate arm at all. A rules change
    // alters the game for both teams, which means it cannot be A/B'd inside
    // one binary -- you run the SAME seeds through two builds and compare
    // per seed. Seeds are disjoint from the other modes so runs never mix.
    uint32_t seedBase = (mode == 1) ? 37'000'000u
                      : (mode == 2) ? 51'000'000u
                      : (mode == 3) ? 63'000'000u
                      : (mode == 4) ? 79'000'000u
                      : (mode == 5) ? 91'000'000u : SEED_BASE;
    setvbuf(stdout, nullptr, _IOLBF, 0);
    printf("mode=%d (%s)\n", mode,
           mode == 1 ? "GRIND A/B: cage+grind vs cage (fallback)"
         : mode == 2 ? "ERA: single arm, production config, both sides"
         : mode == 4 ? "DAUNTLESS: block offer prices the equalised strength vs raw"
         : mode == 3 ? "M1: learned policy blend 0.2 vs 0.0, DWARF SIDE ONLY"
         : mode == 5 ? "P9/P9c: cilove pole odsunu se VYBIRA (geometrie) vs 'rovne dozadu'"
                     : "cage vs off");

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

    // 2026-08-17: was "a". A run that gets killed and started again -- which is
    // exactly what happened on 14.08. -- then APPENDED a second set of rows on
    // top of the first, and nothing said so: the pair count still looked right
    // to anything that de-duplicates by seed. One process owns one shard
    // directory, so truncating is the behaviour that makes a re-launch correct
    // by construction rather than by remembering to rm first.
    const char* rowsName = mode == 5 ? "diag_pushgeom_rows.jsonl"
                         : mode == 4 ? "diag_dauntless_rows.jsonl"
                         : mode == 1 ? "diag_f1_grind_rows.jsonl"
                         : mode == 2 ? "diag_era_rows.jsonl"
                         : mode == 3 ? "diag_m1_rows.jsonl"
                                     : "diag_f1_cage_advance_rows.jsonl";
    if (FILE* old = fopen(rowsName, "r")) {
        fclose(old);
        printf("note: %s existed and is being overwritten (re-launch)\n", rowsName);
    }
    FILE* rows = fopen(rowsName, "w");

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
        // 2026-08-17: the same question for the Dauntless arm, printed in the
        // SUMMARY instead of having to be recovered from the rows afterwards.
        // A matchup where the arm never fires is a TRUE NULL and its delta is
        // the harness's own noise -- that is the single most useful line on
        // Monday, and until now nothing printed it.
        long dauntTotal = 0, rollTotal = 0;
        long handoffOfferTotal = 0;   // P21: offered vs chosen
        for (int i = 0; i < nPairs; ++i) {
            uint32_t seed = seedBase + static_cast<uint32_t>(mi) * 1'000'000u
                            + static_cast<uint32_t>(seedOffset + i);
            PairResult pr;
            for (int orient = 0; orient < 2; ++orient) {
                bool candHome = (orient == 0);
                // mode 0: cand=cage on,  base=cage off
                // mode 1: cand=cage+grind, base=cage without grind
                // mode 2: BOTH production (cage off, grind off) -- era run
                // mode 3 (M1, Fable §M1): both arms keep the policy LOADED so
                // the hand-coded prior floors stay active in both; only the
                // LEARNED content differs (blend 0.2 vs 0.0). That isolates
                // "does the learned policy help or hurt the dwarf" from "do
                // the floors help". Cage/grind off, as in production.
                // mode 4 (2026-08-14): both arms are production -- cage off,
                // grind off -- and the ONLY difference is whether the block
                // offer prices a Dauntless block at the strength it would
                // equalise to. base is the engine as it stood on 14.08.
                //
                // 2026-08-17: Dauntless-in-offer became production, so "leave
                // it at the struct default" no longer means "off". Both arms
                // now say what they want out loud. Mode 4 is kept reproducible
                // -- its base arm is still the pre-17.08. engine -- while every
                // other mode gets the arm ON in BOTH arms, because an A/B whose
                // two sides are both non-production measures an engine we do
                // not ship. That is the same defect the arm itself was written
                // to fix, one level up.
                const bool candDauntless = true;
                const bool baseDauntless = (mode != 4);
                // ⛔ 18.08.: bylo `mode != 2 && mode != 3 && mode != 4`, tedy
                // seznam VÝJIMEK -- každý nový režim by bránu klece dostal
                // zapnutou, aniž by o to kdokoli požádal. Mode 5 (P9) by tak
                // měřil odsun DOHROMADY s bránou, kterou jsme týž den zamítli
                // (−0,0248 ± 0,0068). Přepsáno na seznam těch, kdo ji CHTĚJÍ.
                MCTSConfig candCfg = makeConfig(vf.get(), pol.get(),
                                                mode == 0 || mode == 1,
                                                mode == 1,
                                                mode == 3 ? 0.2f : 0.0f,
                                                candDauntless);
                MCTSConfig baseCfg = makeConfig(vf.get(), pol.get(),
                                                mode == 1, false, 0.0f,
                                                baseDauntless);
                // ⭐ COMMON RANDOM NUMBERS (2026-08-17). All three seeds used to
                // carry `+ orient`, so the two halves of a "pair" were two
                // DIFFERENT games on related seeds. Measured correlation
                // between the halves: -0.017. The pairing bought us nothing,
                // and the cost was not academic -- on 15.08. a matchup where
                // the arm provably never fired came out at +2.28 pp (+2.3 SE)
                // and formally cleared the pre-registered "passed" threshold.
                //
                // Without `+ orient` both orientations play the SAME game with
                // the SAME per-side search seeds, and only the arm assignment
                // swaps. An arm that changes nothing now yields a delta of
                // exactly zero, per pair, not on average. The seed is bound to
                // the SIDE (11 home, 47 away) rather than to the arm, which is
                // what makes the swap a swap.
                //
                // No new dependence between pairs: seeds stay disjoint across
                // pairs. The one thing this changes about reading the output is
                // that most pairs now return exactly 0 -- so SUMMARY reports
                // n_nonzero, and the mean is still taken over ALL pairs,
                // including the zeros. Dropping them would inflate the effect.
                MacroMCTSPolicy homePol(vf.get(),
                                        candHome ? candCfg : baseCfg,
                                        seed * 2654435761u + 11u);
                MacroMCTSPolicy awayPol(vf.get(),
                                        candHome ? baseCfg : candCfg,
                                        seed * 2654435761u + 47u);
                // mode 5 (P9/P9c, 18.08.): rameno nesedí v MCTSConfig, ale
                // v resolveru bloku, protože odsun řeší resolver, ne search.
                // Nastavuje se PER STRANU, ať A/B umí jen jednu -- a shazuje se
                // hned po hře, aby nepřeteklo do dalšího páru.
                bb::setPushGeometryArm(bb::TeamSide::HOME,
                                       mode == 5 && candHome);
                bb::setPushGeometryArm(bb::TeamSide::AWAY,
                                       mode == 5 && !candHome);
                bb::takePushGeometryEvalsInSearch();   // vynuluj čítač na pár

                FullGameOutcome g = playGame(
                    *homeRoster, *awayRoster,
                    [&](const GameState& s) { return homePol(s); },
                    [&](const GameState& s) { return awayPol(s); },
                    seed * 2u);

                // mode 4: kolikrát nabídka ocenila blok silou, na kterou by
                // Dauntless srovnal. Nula = rameno nic nezměnilo, a to je něco
                // JINÉHO než „změna nemá efekt".
                long candPush = bb::takePushGeometryEvalsInSearch();
                bb::setPushGeometryArm(bb::TeamSide::HOME, false);
                bb::setPushGeometryArm(bb::TeamSide::AWAY, false);
                long candDaunt = bb::takeDauntlessOfferEvalsInSearch();
                long candRoll  = bb::takeDauntlessRollEvalsInSearch();
                handoffOfferTotal += bb::takeHandOffOfferEvalsInSearch();
                int candPlans = (candHome ? homePol : awayPol).stagedPlansAdopted();
                int basePlans = (candHome ? awayPol : homePol).stagedPlansAdopted();
                candPlansTotal += candPlans;
                dauntTotal += candDaunt;
                rollTotal += candRoll;
                int cs = candHome ? g.homeScore : g.awayScore;
                int bs = candHome ? g.awayScore : g.homeScore;
                double chess = cs > bs ? 1.0 : (cs < bs ? 0.0 : 0.5);
                if (candHome) pr.chessCandHome = chess;
                else pr.chessCandAway = chess;
                // Co znamená "rameno jednalo" závisí na rameni. Mode 0/1 =
                // brána adoptovala plán; mode 4 = nabídka ocenila Dauntlessem.
                // Mode 3 (policy blend) NEMÁ takový signál -- působí na každé
                // rozhodnutí a nic ho nepočítá -- a mode 2 nemá rameno vůbec.
                // Kdyby se pro ně armEvents počítalo z candPlans, vyšla by nula
                // vždy a leak test by křičel na KAŽDÉM pohnutém páru. Proto se
                // pro ně netiskne; falešný poplach je horší než žádný test,
                // protože se ho lidi naučí ignorovat.
                pr.armEvents += (mode == 4) ? candDaunt
                              : (mode == 5) ? candPush : candPlans;
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
                            "\"cand_plans\":%d,\"base_plans\":%d,"
                            "\"cand_daunt\":%ld,\"cand_roll\":%ld,\"mode\":%d}\n",
                            mi, seedOffset + i, candHome ? "true" : "false",
                            mu.home, mu.away,
                            cs, bs, g.home.ko, g.home.injured, g.home.dead,
                            g.home.ejected, g.away.ko, g.away.injured,
                            g.away.dead, g.away.ejected, g.totalActions, candPlans,
                            basePlans, candDaunt, candRoll, mode);
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
        // With common random numbers most pairs come back exactly 0, because
        // the two arms played the identical game. n_nonzero is therefore the
        // honest sample size of the comparison -- and a run where it is 0 has
        // measured nothing at all, however tight its SE looks. The mean stays
        // over ALL pairs; dividing by n_nonzero instead would inflate it.
        int nNonzero = 0, nArmFired = 0, nLeak = 0;
        for (auto& p2 : pairs) {
            bool moved = p2.deltaHomeRace() != 0.0;
            bool fired = p2.armEvents > 0;
            if (moved) ++nNonzero;
            if (fired) ++nArmFired;
            if (moved && !fired) ++nLeak;   // must be 0
        }
        int dec = candW + candL;
        double wr = dec > 0 ? static_cast<double>(candW) / dec : 0.0;
        printf("SUMMARY matchup %d (%s vs %s), %d pairs (%d games), "
               "n_nonzero %d (%.1f%%)%s:\n",
               mi, mu.home, mu.away, static_cast<int>(n), 2 * static_cast<int>(n),
               nNonzero, 100.0 * nNonzero / n,
               nNonzero == 0 ? "  <= ARMS ARE IDENTICAL, NOTHING MEASURED" : "");
        // ⭐ THE REAL NULL TEST, and it lives inside the run rather than beside
        // it. A separate mode-2 leg (same config in both arms) is a TAUTOLOGY
        // under CRN: the two orientations are literally the same game, so the
        // delta is forced to 0 algebraically and the leg can only catch a gross
        // seeding mistake. This cannot be waved through: it compares pairs where
        // the arm ACTED against pairs where it did not, in the arm's own run.
        const bool armSignalAvailable = (mode == 0 || mode == 1 || mode == 4 || mode == 5);
        if (armSignalAvailable) {
            printf("  arm acted in %d/%d pairs; pairs that moved: %d; "
                   "MOVED WITHOUT THE ARM ACTING: %d%s\n",
                   nArmFired, static_cast<int>(n), nNonzero, nLeak,
                   nLeak > 0
                       ? "  <= LEAK. The flag changed the run through something"
                         " other than the feature. DO NOT read the delta."
                       : "  (0 = clean)");
        } else {
            printf("  arm-acted signal: NONE for mode %d -- this arm touches "
                   "every decision and nothing counts it, so the per-pair leak "
                   "test cannot run. The delta here has NO null control.\n",
                   mode);
        }
        printf("  cand overall: W%d D%d L%d decisiveWR=%.3f chess=%.4f | "
               "cage plans adopted %.2f/game%s\n",
               candW, candD, candL, wr,
               (candW + 0.5 * candD) / (2.0 * n),
               candPlansTotal / (2.0 * n),
               mode == 2 ? " (cage is OFF in both arms -- 0.00 says nothing"
                           " about the gate)"
             : mode == 4 ? " (cage is OFF in both arms -- read ARM below,"
                           " not this)"
                         : "");
        // Did the arm fire at all? Zero is NOT "no effect" -- it is "the two
        // arms ran the same code", which makes the matchup a true null and its
        // delta a direct read of this harness's noise floor.
        // P21: printed in every mode -- the hand-off swap is production, not an
        // arm, and "was it ever on the menu" is the question the corpus could
        // not answer.
        printf("  HAND_OFF offer evals in search: %ld (%.1f/game) -- %s\n",
               handoffOfferTotal, handoffOfferTotal / (2.0 * n),
               handoffOfferTotal == 0
                   ? "NEVER OFFERED => the gate is what kills it"
                   : "offered; if none were played, the SEARCH is what kills it");
        if (mode == 4) {
            printf("  ARM Dauntless: %ld offers (%.0f/game), %ld rolls "
                   "(%.0f/game) -- %s\n",
                   dauntTotal, dauntTotal / (2.0 * n),
                   rollTotal, rollTotal / (2.0 * n),
                   dauntTotal == 0
                       ? "NEVER FIRED => TRUE NULL: any delta below is this "
                         "harness's noise, not an effect"
                       : "arm active. ⚠️ SEARCH EVALUATIONS, NOT BLOCKS PLAYED "
                         "-- measured 186x the played count. Played rolls come "
                         "from SKILL_USED/Dauntless in the event log (1.88/game "
                         "on the 14.08. corpus).");
        }
        // In mode 2 the two arms are the SAME configuration; only the MCTS RNG
        // seed differs. The delta is therefore a null test, not a measurement,
        // and printing it next to the pre-registered threshold invites reading
        // a gate effect that is not there. Say what it is instead.
        if (mode == 2) {
            printf("  NULL-TEST delta chess as %s: %+.4f +- %.4f SE (~%.1f SE)"
                   " [same config both arms, RNG seed only -- expected 0;"
                   " this IS the noise floor]\n",
                   mu.home, mean, se, se > 0 ? mean / se : 0.0);
        } else {
            // The threshold used to be hardcoded at +0.03 while the weekend
            // pre-registration said +-0.02 -- the binary and the document
            // disagreed about what "passed" means. Say which document owns it.
            printf("  PAIRED delta chess as %s: %+.4f +- %.4f SE (~%.1f SE)\n"
                   "    [%s]\n",
                   mu.home, mean, se, se > 0 ? mean / se : 0.0,
                   mode == 4
                       ? "pre-reg evidence/weekend_prereg_20260814.md: PASS if a "
                         "dwarf arm >= +0.02 and neither < 0; REJECT if any <= "
                         "-0.02; null matchups must stay within 2 SE"
                       : "pre-reg: >= +0.03 on dwarf matchups; control within 2 SE");
        }
    }

    // The split is by which arm the OPPONENT ran. Only outside mode 2 does that
    // arm correspond to the cageAdvance gate; in mode 2 both arms are the same
    // configuration, so the two rows are one race measured twice and their
    // spread is the per-race noise floor.
    printf("\n=== ATTRITION / SURVIVAL (end-of-game states per race, split by "
           "which arm the OPPONENT ran%s) ===\n",
           mode == 2 ? ": SAME config, RNG seed only -- rows are a noise floor"
         : mode == 4 ? ": Dauntless in the block offer on/off"
                     : ": cageAdvance on/off");
    printf("%-10s %-14s %6s %8s %8s %8s %8s %10s\n", "race",
           mode == 2 ? "opp arm" : mode == 4 ? "opp arm" : "opp gate", "games",
           "KO/g", "INJ/g", "DEAD/g", "EJ/g", "surv/11");
    for (auto& [race, byGate] : attrition) {
        for (auto& [gate, a] : byGate) {
            if (a.n == 0) continue;
            double inj = static_cast<double>(a.injured) / a.n;
            double dead = static_cast<double>(a.dead) / a.n;
            const char* label = mode == 2 ? (gate ? "RNG-A" : "RNG-B")
                              : mode == 4 ? (gate ? "Dauntless ON" : "off")
                                          : (gate ? "cageAdvance ON" : "off");
            printf("%-10s %-14s %6ld %8.2f %8.2f %8.2f %8.2f %10.2f\n",
                   race.c_str(), label, a.n,
                   static_cast<double>(a.ko) / a.n, inj, dead,
                   static_cast<double>(a.ejected) / a.n, 11.0 - inj - dead);
        }
    }
    if (rows) fclose(rows);
    printf("\nDONE\n");
    return 0;
}
