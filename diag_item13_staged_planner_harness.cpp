// diag_item13_staged_planner_harness.cpp  (2026-07-31, queue item 13 MVP)
//
// Standalone READ-ONLY diagnostic for the staged safe-then-PICKUP whole-turn
// planner (bb/turn_planner.h). Loads real mined PICKUP-goal turn-start
// states (diag_item13_states.json, produced by diag_item13_mine_states.py
// from the diag_replay_mine_20260730_data corpus) and answers, per state:
//
//   STAGE P (prediction): build the staged plan with the production replay
//     config. Report goal, safe-stage size, backupCount (safe REPOSITIONs
//     ending adjacent to the ball -- the item 11 closure metric), pSuccess,
//     V(success), V(fail) and the 2-branch planValue.
//
//   STAGE AB (whole-turn A/B, paired seeds): play the entire team-turn
//     macro-by-macro, nPairs paired seeds.
//       A = production: execute whatever per-macro search() picks (exactly
//           the item 10 harness's mode-0 loop).
//       B = staged: execute the plan's safe macros first (each re-validated
//           by stagedMacroStillValid, skipped on deviation), then the PICKUP
//           branch macro, then hand the rest of the turn back to per-macro
//           search() -- mirroring the MacroMCTSPolicy integration.
//     Report mean end-of-turn value (engine leaf eval, own perspective; the
//     SAME eval the plan's prediction uses -> directly comparable), paired
//     delta +- SE, turnover rate, ball-secured rate, TD rate, activations
//     banked, and the item 11 metric: mean # of standing teammates adjacent
//     to the ball at the moment the pickup roll happens.
//
//   CALIBRATION: predicted planValue vs mean realized end-of-turn value of
//     the B arm (does the 2-branch model predict what actually happens?).
//
// Build (worktree root, after engine/build/libbb_engine.so exists):
//   g++ -O2 -std=c++20 -Iengine/include -Iengine/third_party \
//       diag_item13_staged_planner_harness.cpp \
//       -Lengine/build -lbb_engine -Wl,-rpath,$PWD/engine/build \
//       -o diag_item13_staged_planner
//
// Run (FULL validation -- ~12 states x 2 x nPairs=200 whole turns with
// search iters=100 each; budget a few hours of ONE core, run niced):
//   nice -n 19 ./diag_item13_staged_planner . diag_item13_states.json 200 \
//       > diag_item13_full_$(date +%Y%m%d).log 2>&1
//
// Mini-smoke (1-2 states, seconds -- the only mode run before validation):
//   nice -n 19 ./diag_item13_staged_planner . diag_item13_states.json 4 0
//   nice -n 19 ./diag_item13_staged_planner . diag_item13_states.json 4 1
//
// argv: [repoRoot=.] [statesJson=diag_item13_states.json] [nPairs=200]
//       [stateFilter=-1 (all)]

#include "bb/game_state.h"
#include "bb/macro_actions.h"
#include "bb/macro_mcts.h"
#include "bb/turn_planner.h"
#include "bb/cage_advance.h"
#include "bb/helpers.h"
#include "bb/mcts.h"
#include "bb/value_function.h"
#include "bb/policy_network.h"
#include "bb/dice.h"
#include "bb/game_simulator.h"
#include "bb/roster.h"
#include "nlohmann/json.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

using namespace bb;
using nlohmann::json;

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

static Weather parseWeather(const std::string& w) {
    if (w == "sweltering_heat") return Weather::SWELTERING_HEAT;
    if (w == "very_sunny") return Weather::VERY_SUNNY;
    if (w == "pouring_rain") return Weather::POURING_RAIN;
    if (w == "blizzard") return Weather::BLIZZARD;
    return Weather::NICE;
}

static GameState makeState(const json& st) {
    GameState s;
    std::string hr = st["home_race"], ar = st["away_race"];
    const TeamRoster* home = getDevelopedRoster(hr, 1200);
    const TeamRoster* away = getDevelopedRoster(ar, 1200);
    if (!home || !away) {
        fprintf(stderr, "roster load failed (%s/%s)\n", hr.c_str(), ar.c_str());
        exit(1);
    }
    setupHalf(s, *home, *away, TeamSide::AWAY);

    s.half = st["half"];
    s.phase = GamePhase::PLAY;
    s.activeTeam = (st["active"] == "home") ? TeamSide::HOME : TeamSide::AWAY;
    s.weather = parseWeather(st["weather"]);
    s.kickingTeam = TeamSide::AWAY;
    s.turnoverPending = false;
    s.currentActivationId = -1;

    s.homeTeam.score = st["home_score"];
    s.awayTeam.score = st["away_score"];
    int turn = st["turn"];
    s.homeTeam.turnNumber = turn;
    s.awayTeam.turnNumber = turn;
    // Rerolls are not in the replay log; 2 is the same mid-game convention
    // the item 10 harness used.
    s.homeTeam.rerolls = 2;
    s.awayTeam.rerolls = 2;
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
    for (const auto& jp : st["players"]) {
        Player& p = s.getPlayer(jp["id"]);
        int stt = jp["st"];
        p.state = (stt == 0) ? PlayerState::STANDING
                : (stt == 1) ? PlayerState::PRONE : PlayerState::STUNNED;
        p.position = {static_cast<int8_t>(int(jp["x"])), static_cast<int8_t>(int(jp["y"]))};
        p.movementRemaining = p.stats.movement;
    }
    for (const auto& id : st["ko"]) s.getPlayer(int(id)).state = PlayerState::KO;

    s.ball.position = {static_cast<int8_t>(int(st["ball"][0])),
                       static_cast<int8_t>(int(st["ball"][1]))};
    s.ball.isHeld = false;
    s.ball.carrierId = -1;
    return s;
}

static MCTSConfig makeConfig(const PolicyNetwork* pol) {
    // Production replay config: iters=100, C=1.0, vfBlend=0, nRollouts=1,
    // policy loaded with policyBlend=0 => heuristic prior floors ACTIVE.
    MCTSConfig cfg;
    cfg.maxIterations = 100;
    cfg.timeBudgetMs = 0;
    cfg.explorationC = 1.0;
    cfg.dirichletAlpha = 0.0f;
    cfg.vfBlend = 0.0f;
    cfg.nRollouts = 1;
    cfg.policy = pol;
    cfg.policyBlend = 0.0f;
    return cfg;
}

static int countActed(const GameState& s, TeamSide side) {
    int n = 0;
    s.forEachOnPitch(side, [&](const Player& p) {
        if (p.hasActed || p.hasMoved) n++;
    });
    return n;
}

static int adjacentTeammates(const GameState& s, Position ball, TeamSide side,
                             int excludeId) {
    int n = 0;
    s.forEachOnPitch(side, [&](const Player& p) {
        if (p.id == excludeId) return;
        if (p.state != PlayerState::STANDING) return;
        if (p.position.distanceTo(ball) == 1) n++;
    });
    return n;
}

struct TurnOutcome {
    double value = 0;       // end-of-turn leaf eval, own perspective
    bool turnover = false;
    bool td = false;
    bool ballSecured = false;  // we hold the ball at end of turn
    int activations = 0;
    bool pickupAttempted = false;
    int backupsAtPickup = -1;  // adjacent standing teammates when pickup ran
    int cornersAtEnd = -1;     // step 2 metric: standing teammates on the 4
                               // diagonal slots around the carrier at end of
                               // turn (only when ballSecured)
};

static void finishOutcome(TurnOutcome& out, GameState& state, TeamSide mySide,
                          int myScore0, MacroMCTSSearch& evaler) {
    out.activations = countActed(state, mySide);
    if (state.getTeamState(mySide).score > myScore0) out.td = true;
    out.ballSecured = state.ball.isHeld && state.ball.carrierId > 0 &&
                      state.getPlayer(state.ball.carrierId).teamSide == mySide;
    if (out.ballSecured) {
        const Player& c = state.getPlayer(state.ball.carrierId);
        out.cornersAtEnd = 0;
        for (int sx : {-1, 1}) {
            for (int sy : {-1, 1}) {
                Position d{static_cast<int8_t>(c.position.x + sx),
                           static_cast<int8_t>(c.position.y + sy)};
                const Player* occ = state.getPlayerAtPosition(d);
                if (occ && occ->teamSide == mySide &&
                    occ->state == PlayerState::STANDING) {
                    out.cornersAtEnd++;
                }
            }
        }
    }
    out.value = evaler.evaluateLeaf(state, mySide);
}

// Common tail: per-macro production search until the turn ends. Shared by
// arm A (whole turn) and arm B (after the staged plan resolves).
static void runSearchTail(GameState& state, TurnOutcome& out, TeamSide mySide,
                          int myScore0, const ValueFunction* vf,
                          const PolicyNetwork* pol, DiceRoller& turnDice,
                          uint32_t seed, int stepBase) {
    MCTSConfig cfg = makeConfig(pol);
    Macro lastNoop{};
    bool hadNoop = false;
    for (int step = stepBase; step < stepBase + 30; ++step) {
        if (state.phase != GamePhase::PLAY || state.activeTeam != mySide) break;
        MacroMCTSSearch search(vf, cfg, seed * 131u + static_cast<uint32_t>(step));
        Macro pick = search.search(state);
        if (pick.type == MacroType::END_TURN) break;
        if (pick.type == MacroType::PICKUP && !out.pickupAttempted) {
            out.pickupAttempted = true;
            out.backupsAtPickup = adjacentTeammates(state, state.ball.position,
                                                    mySide, pick.playerId);
        }
        auto res = greedyExpandMacro(state, pick, turnDice);
        if (res.turnover) { out.turnover = true; break; }
        if (state.phase == GamePhase::TOUCHDOWN ||
            state.getTeamState(mySide).score > myScore0) break;
        if (state.phase != GamePhase::PLAY) break;
        if (res.actions.empty()) {
            bool same = lastNoop.type == pick.type && lastNoop.playerId == pick.playerId &&
                        lastNoop.targetId == pick.targetId && lastNoop.targetPos == pick.targetPos;
            if (hadNoop && same) break;  // no-op livelock bail
            hadNoop = true; lastNoop = pick;
        } else {
            hadNoop = false;
        }
    }
}

// Arm A: production (item 10 harness mode-0 loop).
static TurnOutcome playTurnProduction(const GameState& start, const ValueFunction* vf,
                                      const PolicyNetwork* pol, uint32_t seed) {
    TurnOutcome out;
    GameState state = start.clone();
    TeamSide mySide = start.activeTeam;
    int myScore0 = start.getTeamState(mySide).score;
    DiceRoller turnDice(seed * 7919u + 13);
    MacroMCTSSearch evaler(vf, makeConfig(pol), 1);
    runSearchTail(state, out, mySide, myScore0, vf, pol, turnDice, seed, 0);
    finishOutcome(out, state, mySide, myScore0, evaler);
    return out;
}

// Arm B: staged plan (safe macros -> PICKUP branch), then search tail --
// mirrors the MacroMCTSPolicy integration (deviation -> skip/fallback).
static TurnOutcome playTurnStaged(const GameState& start, const StagedPlan& plan,
                                  const ValueFunction* vf, const PolicyNetwork* pol,
                                  uint32_t seed) {
    TurnOutcome out;
    GameState state = start.clone();
    TeamSide mySide = start.activeTeam;
    int myScore0 = start.getTeamState(mySide).score;
    DiceRoller turnDice(seed * 7919u + 13);
    MacroMCTSSearch evaler(vf, makeConfig(pol), 1);

    bool deviated = false;
    for (const auto& m : plan.safeMacros) {
        if (state.phase != GamePhase::PLAY || state.activeTeam != mySide) break;
        if (!stagedMacroStillValid(state, m)) { deviated = true; break; }
        auto res = greedyExpandMacro(state, m, turnDice);
        if (res.turnover) { out.turnover = true; break; }
    }
    if (!out.turnover && state.phase == GamePhase::PLAY && state.activeTeam == mySide &&
        !deviated && stagedMacroStillValid(state, plan.pickupMacro)) {
        out.pickupAttempted = true;
        out.backupsAtPickup = adjacentTeammates(state, state.ball.position, mySide,
                                                plan.pickupMacro.playerId);
        auto res = greedyExpandMacro(state, plan.pickupMacro, turnDice);
        if (res.turnover) out.turnover = true;
    }
    // Step 2: cage-fill stage -- only while our side holds the ball
    // (requireHeldBall), each macro re-validated; deviation skips the rest,
    // mirroring the MacroMCTSPolicy integration.
    if (!out.turnover) {
        for (const auto& m : plan.cageFillMacros) {
            if (state.phase != GamePhase::PLAY || state.activeTeam != mySide) break;
            if (!stagedMacroStillValid(state, m, true)) break;
            auto res = greedyExpandMacro(state, m, turnDice);
            if (res.turnover) { out.turnover = true; break; }
        }
    }
    if (!out.turnover) {
        runSearchTail(state, out, mySide, myScore0, vf, pol, turnDice, seed, 100);
    }
    finishOutcome(out, state, mySide, myScore0, evaler);
    return out;
}

int main(int argc, char** argv) {
    std::string root = (argc > 1) ? argv[1] : ".";
    std::string statesPath = (argc > 2) ? argv[2] : "diag_item13_states.json";
    int nPairs = (argc > 3) ? atoi(argv[3]) : 200;
    int stateFilter = (argc > 4) ? atoi(argv[4]) : -1;
    setvbuf(stdout, nullptr, _IOLBF, 0);

    auto vf = loadValueFunction(root + "/weights_best.json");
    auto pol = loadPolicyNetworkFromFile(root + "/weights_policy.json");
    printf("vf=%s policy=%s (production replay config: iters=100, vfBlend=0, "
           "policyBlend=0 w/ policy loaded => prior floors ACTIVE)\n",
           vf ? "loaded" : "NULL", pol ? "loaded" : "NULL");
    if (!pol) { fprintf(stderr, "policy required for floors\n"); return 1; }

    std::ifstream in(statesPath);
    if (!in) { fprintf(stderr, "cannot open %s\n", statesPath.c_str()); return 1; }
    json doc = json::parse(in);
    const auto& states = doc["states"];
    printf("loaded %zu states from %s\n", states.size(), statesPath.c_str());

    for (size_t ci = 0; ci < states.size(); ++ci) {
        if (stateFilter >= 0 && static_cast<int>(ci) != stateFilter) continue;
        const json& st = states[ci];
        GameState state = makeState(st);
        printf("\n================================================================\n");
        printf("[%zu] %s  real: TO=%d TD=%d recovered=%s\n", ci,
               std::string(st["label"]).c_str(),
               bool(st["real_turnover"]) ? 1 : 0,
               bool(st["real_touchdown"]) ? 1 : 0,
               st["real_recovered_by_next_own_turn"].is_null() ? "?"
                   : (bool(st["real_recovered_by_next_own_turn"]) ? "yes" : "no"));

        // ---- STAGE D (2026-08-07, user doctrine "go for the ball vs deny
        // it"): both decision members, priced per PICKUP candidate and
        // printed for EVERY state -- including ones where no plan is
        // adopted, which is exactly where the numbers matter.
        //   (1) chance  = computed pickup success at the ball square
        //                 (engine's own target + Sure Hands personal reroll)
        //   (2) corners = cage support AFTER a successful pickup: slots our
        //                 still-unmoved bodies can fill around the projected
        //                 post-pickup carrier square (CageAdvancePlanner::
        //                 tryAssign on the projection). A pickup that lands
        //                 a lone carrier in enemy territory is not a win.
        {
            MCTSConfig dcfg = makeConfig(pol.get());
            CageAdvancePlanner cageHelper(vf.get(), dcfg, 4242);
            std::vector<Macro> dmacros;
            getAvailableMacros(state, dmacros);
            TeamSide side = state.activeTeam;
            int fdx = (side == TeamSide::HOME) ? 1 : -1;
            for (const auto& m : dmacros) {
                if (m.type != MacroType::PICKUP) continue;
                const Player& pk = state.getPlayer(m.playerId);
                int dist = pk.position.distanceTo(state.ball.position);
                int target = calculatePickupTargetAt(state, pk, state.ball.position);
                double chance = (7.0 - target) / 6.0;
                if (pk.hasSkill(SkillName::SureHands)) {
                    chance += (1.0 - chance) * chance;
                }
                GameState proj = state.clone();
                Player& pp = proj.getPlayer(m.playerId);
                int mvAfter = static_cast<int>(pp.movementRemaining) - dist;
                pp.position = state.ball.position;
                pp.movementRemaining = static_cast<int8_t>(std::max(0, mvAfter));
                pp.hasMoved = true;
                proj.ball = BallState::carried(state.ball.position, pp.id);
                int steps = (mvAfter > 0)
                    ? carrierStallAwareSteps(proj, pp, proj.getTeamState(side)) : 0;
                int filled = -1, open = -1;
                if (steps >= 1) {
                    auto ar = cageHelper.tryAssign(proj, pp, steps, {pp.id});
                    filled = ar.filled;
                    open = ar.open;
                }
                printf("STAGE D: picker p%d AG%d%s dist=%d target=%d+ "
                       "chance=%.0f%% | advance=%d corners filled=%d open=%d\n",
                       m.playerId, pk.stats.agility,
                       pk.hasSkill(SkillName::SureHands) ? "+SH" : "",
                       dist, target, 100.0 * chance, steps, filled, open);
            }
        }

        // ---- STAGE P: build & report the staged plan
        MCTSConfig cfg = makeConfig(pol.get());
        StagedTurnPlanner planner(vf.get(), cfg, 12345);
        StagedPlan plan = planner.build(state);
        printf("STAGE P: goal=%d valid=%d safe=%zu backupCount=%d\n",
               static_cast<int>(plan.goal), plan.valid ? 1 : 0,
               plan.safeMacros.size(), plan.backupCount);
        if (!plan.valid) { printf("  (no plan -> state skipped)\n"); continue; }
        printf("  pickup: %s p%d at (%d,%d)\n", macroName(plan.pickupMacro.type),
               plan.pickupMacro.playerId,
               static_cast<int>(plan.pickupMacro.targetPos.x),
               static_cast<int>(plan.pickupMacro.targetPos.y));
        for (const auto& m : plan.safeMacros) {
            printf("  safe: %s p%d -> (%d,%d)\n", macroName(m.type), m.playerId,
                   static_cast<int>(m.targetPos.x), static_cast<int>(m.targetPos.y));
        }
        printf("  pSuccess=%.3f V(success)=%.4f V(fail)=%.4f planValue=%.4f\n",
               plan.pSuccess, plan.valueSuccess, plan.valueFail, plan.planValue);

        if (nPairs <= 0) continue;  // prediction-only run

        // ---- STAGE AB: paired-seed whole-turn A/B
        double sumA = 0, sumB = 0, sumD = 0, sumD2 = 0;
        int toA = 0, toB = 0, tdA = 0, tdB = 0, secA = 0, secB = 0;
        double actA = 0, actB = 0;
        int attA = 0, attB = 0;
        double bkA = 0, bkB = 0;
        int bkAn = 0, bkBn = 0;
        double crA = 0, crB = 0;
        int crAn = 0, crBn = 0;
        for (int i = 0; i < nPairs; ++i) {
            TurnOutcome a = playTurnProduction(state, vf.get(), pol.get(), 20000 + i);
            TurnOutcome b = playTurnStaged(state, plan, vf.get(), pol.get(), 20000 + i);
            sumA += a.value; sumB += b.value;
            double d = b.value - a.value;
            sumD += d; sumD2 += d * d;
            toA += a.turnover; toB += b.turnover;
            tdA += a.td; tdB += b.td;
            secA += a.ballSecured; secB += b.ballSecured;
            actA += a.activations; actB += b.activations;
            attA += a.pickupAttempted; attB += b.pickupAttempted;
            if (a.backupsAtPickup >= 0) { bkA += a.backupsAtPickup; bkAn++; }
            if (b.backupsAtPickup >= 0) { bkB += b.backupsAtPickup; bkBn++; }
            if (a.cornersAtEnd >= 0) { crA += a.cornersAtEnd; crAn++; }
            if (b.cornersAtEnd >= 0) { crB += b.cornersAtEnd; crBn++; }
        }
        double n = nPairs;
        double mD = sumD / n;
        double sdD = std::sqrt(std::max(0.0, sumD2 / n - mD * mD));
        double seD = sdD / std::sqrt(n);
        printf("STAGE AB: %d paired seeds\n", nPairs);
        printf("  %-30s %10s %10s\n", "", "A(prod)", "B(staged)");
        printf("  %-30s %10.4f %10.4f   (paired delta %+.4f +- %.4f SE, ~%.1f SE)\n",
               "mean end-of-turn value", sumA / n, sumB / n, mD, seD,
               seD > 0 ? mD / seD : 0.0);
        printf("  %-30s %9.1f%% %9.1f%%\n", "turnover rate", 100.0 * toA / n, 100.0 * toB / n);
        printf("  %-30s %9.1f%% %9.1f%%\n", "ball secured at end", 100.0 * secA / n, 100.0 * secB / n);
        printf("  %-30s %9.1f%% %9.1f%%\n", "TD rate", 100.0 * tdA / n, 100.0 * tdB / n);
        printf("  %-30s %10.2f %10.2f\n", "mean activations banked", actA / n, actB / n);
        printf("  %-30s %9.1f%% %9.1f%%\n", "pickup attempted", 100.0 * attA / n, 100.0 * attB / n);
        printf("  %-30s %10.2f %10.2f   (item 11 closure metric)\n",
               "backups adj at pickup",
               bkAn > 0 ? bkA / bkAn : -1.0, bkBn > 0 ? bkB / bkBn : -1.0);
        printf("  %-30s %10.2f %10.2f   (step 2 metric, secured turns only; plan fills=%zu)\n",
               "corners at end (secured)",
               crAn > 0 ? crA / crAn : -1.0, crBn > 0 ? crB / crBn : -1.0,
               plan.cageFillMacros.size());
        printf("  CALIBRATION: planValue=%.4f vs realized B mean=%.4f (delta %+.4f)\n",
               plan.planValue, sumB / n, plan.planValue - sumB / n);
    }
    printf("\nDONE\n");
    return 0;
}
