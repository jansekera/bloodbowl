// diag_f1_adoption_probe (2026-08-04, corner-release groundwork edition):
// after the f1-cage-fix series, DICEY is the main remaining adoption brake.
// This build dumps EVERY DICEY state (board + which plan leg failed + why),
// categorizes it, and computes the user's four numbers per situation:
//   1. how many corners need a dodge to take their slot,
//   2. how many free bodies exist for substitution (and why they didn't serve),
//   3. how many marked failing corners a BLOCK by another player would free
//      (real dice: assists, 2d/1d, attacker Block skill),
//   4. how many a BLITZ would free (item14 path-aware approach estimate +
//      dice; flags collision with the single blitz per turn).
//
// Build:
//   g++ -O2 -std=c++20 -Iengine/include -Iengine/third_party \
//       diag_f1_adoption_probe.cpp \
//       -Lengine/build -lbb_engine -Wl,-rpath,$PWD/engine/build -o probe
// Run (measurement only, nice -n 19, max 2 concurrent):
//   ./probe . <nGames=6> <matchup:0=dw-sk,1=dw-we,2=dw-dw,3=orc-sk>
//
// Seeds: 36M base -- disjoint from 30/31/34/35M runs.

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
#include "bb/helpers.h"
#include "bb/dice.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>

using namespace bb;

static constexpr uint32_t SEED_BASE = 36'000'000;  // disjoint from 30/31/34/35M

struct Matchup { const char* home; const char* away; const char* tag; };
static const Matchup MATCHUPS[] = {
    {"dwarf", "skaven", "dw-sk"}, {"dwarf", "wood-elf", "dw-we"},
    {"dwarf", "dwarf", "dw-dw"}, {"orc", "skaven", "orc-sk"},
};

// ---------------------------------------------------------------------------
// Replicas of macro_actions.cpp statics (same math, kept in sync by hand).
static int blockDiceCount(const GameState& state, const Player& att,
                          const Player& def, bool isBlitz) {
    int attST = att.stats.strength;
    int defST = def.stats.strength;
    if (isBlitz && att.hasSkill(SkillName::Horns)) attST += 1;
    int attAssists = countAssists(state, def.position, att.teamSide,
                                  att.id, def.id, def.id);
    int defAssists = countAssists(state, att.position, def.teamSide,
                                  def.id, att.id, att.id);
    BlockDiceInfo info = getBlockDiceInfo(attST + attAssists, defST + defAssists);
    return info.attackerChooses ? info.count : -info.count;
}
static double blockFailChance(int diceCount, bool attackerHasBlock) {
    int n = std::abs(diceCount);
    if (n == 0) return 1.0;
    double bad = attackerHasBlock ? (1.0 / 6.0) : (2.0 / 6.0);
    if (diceCount > 0) return std::pow(bad, n);
    return 1.0 - std::pow(1.0 - bad, n);
}
static double approachFailChance(const GameState& state, const Player& mover,
                                 Position target) {
    if (mover.position.distanceTo(target) <= 1) return 0.0;
    Position cur = mover.position;
    int moveLeft = mover.movementRemaining - 1;  // blitz block costs 1 MP
    double failChance = 0.0;
    for (int guard = 0; guard < 20 && cur.distanceTo(target) > 1; ++guard) {
        Position next = pickApproachStep(state, mover, cur, target);
        if (next.x < 0) return 1.0;
        if (countTacklezones(state, cur, mover.teamSide) > 0) {
            int dt = calculateDodgeTarget(state, mover, next, cur);
            double df = std::clamp((dt - 1) / 6.0, 0.0, 5.0 / 6.0);
            failChance += df * (1.0 - failChance);
        }
        if (moveLeft <= 0) failChance += (1.0 / 6.0) * (1.0 - failChance);
        moveLeft -= 1;
        cur = next;
    }
    return failChance;
}

// ---------------------------------------------------------------------------
struct ReleaseOption {           // best way to remove one marker
    int markerId = -1;
    Position markerPos{-1, -1};
    int markerST = 0;
    int bestBlockDice = 0;       // 0 = no adjacent free blocker
    int bestBlockerId = -1;
    int bestBlitzDice = 0;
    int bestBlitzerId = -1;
    double bestBlitzFail = 1.0;  // combined approach+block fail
};

struct ProbeAgg {
    long turns = 0;
    std::map<std::string, long> goals;
    long advCarrierBusy = 0;
    std::map<std::string, long> verdicts;
    // DICEY aggregates
    long dicey = 0;
    std::map<char, long> cats;
    long carrierLegFails = 0;
    long sumNeedDodge = 0, sumFreeSubs = 0;
    long markedFail = 0, shadowFail = 0;
    std::map<char, long> unlockBlockByCat, unlockBlitzByCat, collisionByCat;
    long unlockBlock = 0, unlockBlitz = 0;   // situations release layer unlocks
    long blitzCollisions = 0;                // release would need >1 blitz
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

// Map window around the carrier, carrier's forward direction drawn RIGHT.
// C=carrier F=failing mover T/t=teammate standing/prone O/o=opponent
// standing/prone *=failing-leg target (if empty) b=loose ball .=empty #=off
static void printMap(const GameState& state, const Player& carrier,
                     int failPlayerId, Position failTarget) {
    int dx = (carrier.teamSide == TeamSide::HOME) ? 1 : -1;
    for (int ry = -3; ry <= 3; ++ry) {
        printf("    ");
        for (int rx = -3; rx <= 5; ++rx) {
            Position q{static_cast<int8_t>(carrier.position.x + dx * rx),
                       static_cast<int8_t>(carrier.position.y + ry)};
            char c;
            if (!q.isOnPitch()) c = '#';
            else {
                const Player* o = state.getPlayerAtPosition(q);
                if (!o) {
                    if (q == failTarget) c = '*';
                    else c = (!state.ball.isHeld && state.ball.position == q) ? 'b' : '.';
                } else if (o->id == carrier.id) c = 'C';
                else if (o->id == failPlayerId) c = 'F';
                else if (o->teamSide == carrier.teamSide)
                    c = (o->state == PlayerState::STANDING) ? 'T' : 't';
                else
                    c = (o->state == PlayerState::STANDING) ? 'O' : 'o';
            }
            putchar(c);
        }
        putchar('\n');
    }
}

static void analyzeDicey(const GameState& state, const CageAdvancePlan& plan,
                         const char* tag, int gi, ProbeAgg& agg) {
    const Player& carrier = state.getPlayer(state.ball.carrierId);
    TeamSide side = carrier.teamSide;
    const TeamState& my = state.getTeamState(side);
    agg.dicey++;

    const Macro& fail = plan.diagMacros[plan.diceyLegIdx];
    const Player& mover = state.getPlayer(fail.playerId);
    bool carrierLeg = (fail.playerId == carrier.id);
    if (carrierLeg) agg.carrierLegFails++;
    int tzStart = countTacklezones(state, mover.position, side, mover.id);

    // Planned slots = diagonals of the carrier's planned target square.
    Position carrierTarget{-1, -1};
    for (auto& m : plan.diagMacros)
        if (m.playerId == carrier.id) carrierTarget = m.targetPos;

    // (1) corners needing a dodge: planned non-carrier movers starting in TZ
    int needDodge = 0;
    for (auto& m : plan.diagMacros) {
        if (m.playerId == carrier.id) continue;
        const Player& p = state.getPlayer(m.playerId);
        if (countTacklezones(state, p.position, side, p.id) > 0) needDodge++;
    }

    // (2) free substitutes: eligible, unmoved, TZ-free teammates outside the
    // plan and not already holding a slot.
    auto inPlan = [&](int id) {
        for (auto& m : plan.diagMacros) if (m.playerId == id) return true;
        return false;
    };
    auto onSlot = [&](Position p) {
        if (carrierTarget.x < 0) return false;
        return std::abs(p.x - carrierTarget.x) == 1 &&
               std::abs(p.y - carrierTarget.y) == 1;
    };
    int freeSubs = 0, freeSubsReach = 0;
    state.forEachOnPitch(side, [&](const Player& p) {
        if (p.id == carrier.id || inPlan(p.id)) return;
        if (!p.canAct() || p.hasMoved || p.hasActed) return;
        if (!CageAdvancePlanner::eligibleCornerPlayer(p)) return;
        if (countTacklezones(state, p.position, side, p.id) > 0) return;
        if (onSlot(p.position)) return;
        freeSubs++;
        if (p.position.distanceTo(fail.targetPos)
                <= static_cast<int>(p.movementRemaining) + 1) freeSubsReach++;
    });

    // Reconstruct the evolving projection (legs before the failing one) so
    // pile/shadow reflect where teammates actually STAND when the leg walks.
    DiceRoller rdice(state.half * 7919u + my.turnNumber * 131u + 5);
    GameState projected = state.clone();
    bool projOk = true;
    for (int li = 0; li < plan.diceyLegIdx && projOk; ++li) {
        const Macro& m = plan.diagMacros[li];
        bool ok = false;
        for (int a = 0; a < 8 && !ok; ++a) {
            GameState next = projected.clone();
            auto r = greedyExpandMacro(next, m, rdice);
            if (r.turnover || next.phase != GamePhase::PLAY) continue;
            const Player& moved = next.getPlayer(m.playerId);
            if (moved.position.distanceTo(m.targetPos) > 1) continue;
            projected = std::move(next);
            ok = true;
        }
        if (!ok) projOk = false;
    }
    // Sample run of the failing leg on the projection: where does it end?
    Position sampleEnd{-1, -1};
    int sampleMiss = -1;
    bool sampleTO = false;
    if (projOk) {
        GameState s2 = projected.clone();
        auto r = greedyExpandMacro(s2, fail, rdice);
        sampleTO = r.turnover;
        sampleEnd = s2.getPlayer(fail.playerId).position;
        sampleMiss = sampleEnd.distanceTo(fail.targetPos);
    }

    // Straight-line corridor start->target: enemy TZ casters + teammate pile.
    int budget = static_cast<int>(mover.movementRemaining) +
                 (carrierLeg ? fail.gfiAllowance : 0);
    int distLine = mover.position.distanceTo(fail.targetPos);
    bool tight = (distLine >= budget);
    std::vector<int> casterIds;
    int pileOnLine = 0;
    {
        Position cur = mover.position;
        const GameState& pileState = projOk ? projected : state;
        for (int guard = 0; guard < 20 && cur != fail.targetPos; ++guard) {
            int sx = (fail.targetPos.x > cur.x) - (fail.targetPos.x < cur.x);
            int sy = (fail.targetPos.y > cur.y) - (fail.targetPos.y < cur.y);
            cur = Position{static_cast<int8_t>(cur.x + sx),
                           static_cast<int8_t>(cur.y + sy)};
            if (!cur.isOnPitch()) break;
            // TZ casters on this line square (original state; opponents static)
            state.forEachOnPitch(opponent(side), [&](const Player& en) {
                if (en.state != PlayerState::STANDING || en.lostTacklezones) return;
                if (en.position.distanceTo(cur) != 1 && en.position != cur) return;
                if (std::find(casterIds.begin(), casterIds.end(), en.id)
                        == casterIds.end()) casterIds.push_back(en.id);
            });
            if (cur != fail.targetPos) {
                const Player* occ = pileState.getPlayerAtPosition(cur);
                if (occ && occ->teamSide == side && occ->id != fail.playerId)
                    pileOnLine++;
            }
        }
    }

    // Category: a=marked corner w/o substitute, b=TZ shadow at tight budget,
    // c=too few bodies at all, d=other (pile deadlock etc.).
    char cat;
    std::string reason;
    bool marked = (tzStart > 0);
    if (marked && !carrierLeg) {
        cat = (freeSubs == 0) ? 'c' : 'a';
        reason = (freeSubs == 0) ? "marked mover, zero free bodies"
                                 : "marked mover, free bodies out of reach";
    } else if (marked && carrierLeg) {
        cat = 'a';
        reason = "carrier himself marked (dodge to advance)";
    } else if (!casterIds.empty()) {
        cat = 'b';
        reason = tight ? "TZ shadow on line, budget exact"
                       : "TZ shadow on line (dodge risk on walk)";
    } else if (pileOnLine > 0) {
        cat = 'd';
        reason = "teammate pile on line (untangle gap)";
    } else {
        cat = 'd';
        reason = plan.diceyExecFail ? "exec stall, no shadow/pile on line"
                                    : "probe risk, no shadow on line";
    }

    // (3)+(4) release options: markers of a marked mover, or the TZ-shadow
    // casters for category b -- the set a block/blitz layer would remove.
    std::vector<ReleaseOption> rel;
    std::vector<int> targetsIds;
    if (marked) {
        agg.markedFail++;
        state.forEachOnPitch(opponent(side), [&](const Player& en) {
            if (en.state != PlayerState::STANDING || en.lostTacklezones) return;
            if (en.position.distanceTo(mover.position) != 1) return;
            targetsIds.push_back(en.id);
        });
    } else if (cat == 'b') {
        agg.shadowFail++;
        targetsIds = casterIds;
    }
    for (int tid : targetsIds) {
        const Player& en = state.getPlayer(tid);
        ReleaseOption ro;
        ro.markerId = en.id;
        ro.markerPos = en.position;
        ro.markerST = en.stats.strength;
        state.forEachOnPitch(side, [&](const Player& b) {
            if (b.id == carrier.id || b.id == mover.id) return;
            if (!b.canAct() || b.hasMoved || b.hasActed) return;
            if (b.position.distanceTo(en.position) == 1) {
                int d = blockDiceCount(state, b, en, false);
                if (d > ro.bestBlockDice) { ro.bestBlockDice = d; ro.bestBlockerId = b.id; }
            }
            if (b.movementRemaining < 1) return;
            int d = blockDiceCount(state, b, en, true);
            if (d < 1) return;
            double af = approachFailChance(state, b, en.position);
            double bf = blockFailChance(d, b.hasSkill(SkillName::Block));
            double tot = 1.0 - (1.0 - af) * (1.0 - bf);
            if (tot < ro.bestBlitzFail) {
                ro.bestBlitzFail = tot; ro.bestBlitzDice = d; ro.bestBlitzerId = b.id;
            }
        });
        rel.push_back(ro);
    }
    // Coverage: distinct adjacent blockers per marker (greedy, small n).
    int nMarkers = static_cast<int>(rel.size());
    std::vector<int> usedBlockers;
    int blockCovered = 0;
    for (auto& ro : rel) {
        if (ro.bestBlockDice >= 1 &&
            std::find(usedBlockers.begin(), usedBlockers.end(), ro.bestBlockerId)
                == usedBlockers.end()) {
            usedBlockers.push_back(ro.bestBlockerId);
            blockCovered++;
        }
    }
    bool blockRelease = (nMarkers > 0 && blockCovered == nMarkers);
    int needBlitz = nMarkers - blockCovered;  // markers left to the blitz
    int blitzOk = 0;
    for (auto& ro : rel)
        if (ro.bestBlitzDice >= 1 && ro.bestBlitzFail <= 0.35) blitzOk++;
    bool blitzRelease = blockRelease ||
        (nMarkers > 0 && needBlitz == 1 && blitzOk >= 1);
    bool collision = (needBlitz > 1);

    agg.cats[cat]++;
    agg.sumNeedDodge += needDodge;
    agg.sumFreeSubs += freeSubs;
    if (nMarkers > 0) {
        if (blockRelease) { agg.unlockBlock++; agg.unlockBlockByCat[cat]++; }
        if (blitzRelease) { agg.unlockBlitz++; agg.unlockBlitzByCat[cat]++; }
        if (collision) { agg.blitzCollisions++; agg.collisionByCat[cat]++; }
    }

    // ---- dump ----
    int ezX = (side == TeamSide::HOME) ? 25 : 0;
    printf("[DICEY] %s-g%d H%d T%d %s | carrier P%d@(%d,%d) dist=%d step=%d "
           "req=%.2f ach=%.2f resist=%d built=%d filled=%d open=%d cgfi=%d\n",
           tag, gi, state.half, my.turnNumber,
           side == TeamSide::HOME ? "HOME" : "AWAY",
           carrier.id, carrier.position.x, carrier.position.y,
           std::abs(carrier.position.x - ezX), plan.step,
           plan.requiredPace, plan.achievablePace, plan.resistance,
           plan.builtCorners, plan.filledCorners, plan.openCorners,
           plan.carrierGfi);
    printf("  legs(%zu):", plan.diagMacros.size());
    for (size_t i = 0; i < plan.diagMacros.size(); ++i) {
        const Macro& m = plan.diagMacros[i];
        const Player& p = state.getPlayer(m.playerId);
        printf(" %s%zu:P%d(%d,%d)->(%d,%d)%s%s",
               (static_cast<int>(i) == plan.diceyLegIdx) ? "!" : "",
               i, m.playerId, p.position.x, p.position.y,
               m.targetPos.x, m.targetPos.y,
               m.playerId == carrier.id ? "[CAR]" : "",
               (i < plan.diagMacroCornerGfi.size() && plan.diagMacroCornerGfi[i])
                   ? "[gfi]" : "");
    }
    printf("\n  FAIL leg#%d P%d MA%d tzAtStart=%d pto=%.3f ceil=%.2f "
           "meanActs=%.2f execFail=%d dist=%d budget=%d%s %s sampleEnd=(%d,%d)"
           " miss=%d to=%d\n",
           plan.diceyLegIdx, fail.playerId, mover.stats.movement, tzStart,
           plan.diceyPto, plan.diceyCeil, plan.diceyMeanActs,
           plan.diceyExecFail ? 1 : 0, distLine, budget,
           tight ? " TIGHT" : "", carrierLeg ? "CARRIER-LEG" : "",
           sampleEnd.x, sampleEnd.y, sampleMiss, sampleTO ? 1 : 0);
    printf("  CAT=%c (%s) casters=%zu pileOnLine=%d\n", cat, reason.c_str(),
           casterIds.size(), pileOnLine);
    printf("  QUAD: needDodge=%d freeSubs=%d(reach=%d) markers=%d "
           "blockCov=%d blockRel=%d blitzRel=%d collision=%d\n",
           needDodge, freeSubs, freeSubsReach, nMarkers, blockCovered,
           blockRelease ? 1 : 0, blitzRelease ? 1 : 0, collision ? 1 : 0);
    for (auto& ro : rel) {
        printf("  marker P%d@(%d,%d) ST%d bestBlock=%dd(P%d) "
               "bestBlitz=%dd(P%d fail=%.2f)\n",
               ro.markerId, ro.markerPos.x, ro.markerPos.y, ro.markerST,
               ro.bestBlockDice, ro.bestBlockerId,
               ro.bestBlitzDice, ro.bestBlitzerId, ro.bestBlitzFail);
    }
    printMap(state, carrier, fail.playerId, fail.targetPos);
}

static void probeTurn(const GameState& state, CageAdvancePlanner& planner,
                      const char* tag, int gi, ProbeAgg& agg) {
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
    agg.verdicts[verdictName(plan.verdict)]++;
    if (plan.verdict == CageAdvanceVerdict::DICEY && plan.diceyLegIdx >= 0 &&
        plan.diceyLegIdx < static_cast<int>(plan.diagMacros.size())) {
        analyzeDicey(state, plan, tag, gi, agg);
    }
}

int main(int argc, char** argv) {
    std::string root = (argc > 1) ? argv[1] : ".";
    int nGames = (argc > 2) ? atoi(argv[2]) : 6;
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
    cfg.policyBlend = 0.0f;

    const Matchup& mu = MATCHUPS[mi];
    const TeamRoster* homeRoster = getDevelopedRoster(mu.home, 1200);
    const TeamRoster* awayRoster = getDevelopedRoster(mu.away, 1200);
    if (!homeRoster || !awayRoster) { fprintf(stderr, "roster load failed\n"); return 1; }
    printf("probe[%s]: %d games, seeds %uM+, production config\n",
           mu.tag, nGames, SEED_BASE / 1'000'000);

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
                probeTurn(state, planner, mu.tag, gi, agg);
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

    printf("\n=== SUMMARY [%s] (%ld team-turns) ===\n", mu.tag, agg.turns);
    for (auto& [k, v] : agg.goals) printf("  goal %-8s %ld\n", k.c_str(), v);
    printf("  ADVANCE carrier busy: %ld\n", agg.advCarrierBusy);
    for (auto& [k, v] : agg.verdicts) printf("  verdict %-18s %ld\n", k.c_str(), v);
    printf("  DICEY=%ld cats:", agg.dicey);
    for (auto& [k, v] : agg.cats) printf(" %c=%ld", k, v);
    printf(" carrierLegFails=%ld\n", agg.carrierLegFails);
    if (agg.dicey > 0) {
        printf("  QUAD sums: needDodge=%ld freeSubs=%ld markedFail=%ld "
               "shadowFail=%ld unlockBlock=%ld unlockBlitz=%ld collisions=%ld\n",
               agg.sumNeedDodge, agg.sumFreeSubs, agg.markedFail,
               agg.shadowFail, agg.unlockBlock, agg.unlockBlitz,
               agg.blitzCollisions);
        printf("  unlockBlockByCat:");
        for (auto& [k, v] : agg.unlockBlockByCat) printf(" %c=%ld", k, v);
        printf(" | unlockBlitzByCat:");
        for (auto& [k, v] : agg.unlockBlitzByCat) printf(" %c=%ld", k, v);
        printf(" | collisionByCat:");
        for (auto& [k, v] : agg.collisionByCat) printf(" %c=%ld", k, v);
        printf("\n");
    }
    return 0;
}
