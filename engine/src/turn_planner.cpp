#include "bb/turn_planner.h"
#include <algorithm>
#include <cmath>

namespace bb {

namespace {
// All-sixes roller for the picker-corridor check: with every die pinned to
// its best face the ONLY thing that can stop the pickup expansion is
// geometry -- the deterministic greedy walk hitting bodies. Exactly the
// walk the executor will take, so the check cannot disagree with execution.
struct MaxDiceRoller : DiceRollerBase {
    int rollD6() override { return 6; }
    int rollD8() override { return 8; }
};
}  // namespace

TurnGoal classifyTurnGoal(const GameState& state) {
    if (state.phase != GamePhase::PLAY) return TurnGoal::NONE;

    if (!state.ball.isHeld && state.ball.isOnPitch()) return TurnGoal::PICKUP_BALL;
    if (!state.ball.isHeld || state.ball.carrierId <= 0) return TurnGoal::NONE;

    const Player& carrier = state.getPlayer(state.ball.carrierId);
    if (carrier.teamSide != state.activeTeam || !carrier.isOnPitch()) {
        return TurnGoal::NONE;  // opponent's ball -- defensive goals are out of MVP scope
    }

    // Same reach/urgency arithmetic as simulate()'s offensive scoringBonus
    // block (macro_mcts.cpp): MA+2 GFI reach now, or last-2-turns urgency
    // with the endzone within one full activation.
    int ezX = (carrier.teamSide == TeamSide::HOME) ? 25 : 0;
    int dist = std::abs(carrier.position.x - ezX);
    if (dist <= static_cast<int>(carrier.movementRemaining) + 2) {
        return TurnGoal::SCORE_BALL;
    }
    const TeamState& my = state.getTeamState(state.activeTeam);
    int turnsLeft = std::max(0, 9 - my.turnNumber);
    if (turnsLeft <= 2 && dist <= carrier.stats.movement + 2) {
        return TurnGoal::SCORE_BALL;
    }
    return TurnGoal::ADVANCE_BALL;
}

bool stagedMacroStillValid(const GameState& state, const Macro& m,
                           bool requireHeldBall) {
    if (state.phase != GamePhase::PLAY) return false;
    if (m.playerId <= 0) return false;
    const Player& p = state.getPlayer(m.playerId);
    if (!p.isOnPitch() || p.state != PlayerState::STANDING) return false;
    if (p.hasMoved || p.hasActed) return false;
    if (requireHeldBall) {
        // Cage-fill stage: only meaningful while OUR side holds the ball.
        // A failed pickup (or any bounce to the opponent) invalidates the
        // whole stage and the turn falls back to search().
        if (!state.ball.isHeld || state.ball.carrierId <= 0) return false;
        if (state.getPlayer(state.ball.carrierId).teamSide != p.teamSide) {
            return false;
        }
    }

    switch (m.type) {
        case MacroType::REPOSITION: {
            if (!m.targetPos.isOnPitch()) return false;
            // Never step onto a loose ball's square, even as a plan target --
            // the auto-pickup in move_handler would turn this dice-free step
            // into a real gamble (item 11).
            if (!state.ball.isHeld && m.targetPos == state.ball.position) return false;
            const Player* occ = state.getPlayerAtPosition(m.targetPos);
            if (occ && occ->id != p.id) return false;
            return true;
        }
        case MacroType::PICKUP: {
            if (state.ball.isHeld || !state.ball.isOnPitch()) return false;
            if (!(state.ball.position == m.targetPos)) return false;  // ball moved
            return p.position.distanceTo(state.ball.position) <=
                   static_cast<int>(p.movementRemaining) + 2;
        }
        default:
            return false;  // MVP plans only ever contain REPOSITION + PICKUP
    }
}

StagedTurnPlanner::StagedTurnPlanner(const ValueFunction* vf, MCTSConfig config,
                                     uint32_t seed)
    : config_(config), evaler_(vf, config, seed), dice_(seed + 4242),
      cageHelper_(vf, config, seed + 999) {}

StagedTurnPlanner::ProbeStats StagedTurnPlanner::probeMacro(const GameState& state,
                                                            const Macro& m) {
    ProbeStats r;
    int nTO = 0;
    double acts = 0.0;
    for (int k = 0; k < PROBE_K; ++k) {
        GameState sim = state.clone();
        auto res = greedyExpandMacro(sim, m, dice_);
        if (res.turnover) nTO++;
        acts += static_cast<double>(res.actions.size());
    }
    r.pto = static_cast<double>(nTO) / PROBE_K;
    r.meanActions = acts / PROBE_K;
    return r;
}

StagedTurnPlanner::BranchStats StagedTurnPlanner::sampleBranch(
        const GameState& projected, const Macro& pickup, TeamSide perspective) {
    BranchStats bs;
    int nSucc = 0, nFail = 0;
    double sumSucc = 0.0, sumFail = 0.0;
    for (int k = 0; k < BRANCH_K; ++k) {
        GameState sim = projected.clone();
        auto res = greedyExpandMacro(sim, pickup, dice_);
        // Success = the whole PICKUP macro (approach + roll + the built-in
        // stall-aware ADVANCE continuation in expandPickup) ends with us
        // holding the ball. Folding approach risk (dodge/GFI on the way in)
        // into the single branch is a deliberate MVP simplification: it keeps
        // exactly one branch point while pricing the risk the executor will
        // actually take.
        bool success = !res.turnover && sim.ball.isHeld && sim.ball.carrierId > 0 &&
                       sim.getPlayer(sim.ball.carrierId).teamSide == perspective;
        double v = evaler_.evaluateLeaf(sim, perspective);
        if (success) {
            nSucc++;
            sumSucc += v;
        } else {
            nFail++;
            sumFail += v;
        }
    }
    bs.pSuccess = static_cast<double>(nSucc) / BRANCH_K;
    bs.valueSuccess = (nSucc > 0) ? sumSucc / nSucc : 0.0;
    // Fail branch: mean projected eval + fixed penalty for the unmodeled
    // bounce/recovery tree (greedyLookaheadBonus-style flat -0.10).
    bs.valueFail = ((nFail > 0) ? sumFail / nFail : 0.0) + FAIL_PENALTY;
    return bs;
}

StagedPlan StagedTurnPlanner::build(const GameState& state) {
    StagedPlan plan;
    plan.goal = classifyTurnGoal(state);
    if (plan.goal != TurnGoal::PICKUP_BALL) return plan;

    // Support gate: with plentiful help already near the loose ball the
    // production search outperforms the forced safe->pickup ordering
    // (see MAX_PICKUP_SUPPORT rationale in the header). Mirrors the state
    // miner's metric exactly: standing active-team players within
    // Chebyshev SUPPORT_RADIUS of the ball, minus one for the picker.
    {
        int nearBall = 0;
        state.forEachPlayer(state.activeTeam, [&](const Player& p) {
            if (p.state != PlayerState::STANDING) return;
            int dx = std::abs(p.position.x - state.ball.position.x);
            int dy = std::abs(p.position.y - state.ball.position.y);
            if (std::max(dx, dy) <= SUPPORT_RADIUS) nearBall++;
        });
        if (nearBall - 1 > MAX_PICKUP_SUPPORT) return plan;
    }

    std::vector<Macro> macros;
    getAvailableMacros(state, macros, config_.dauntlessInOffer);

    // PICKUP candidates arrive best-first (generation ordering contract,
    // macro_actions.cpp; item 7 emits at most two).
    std::vector<Macro> pickups;
    for (const auto& m : macros) {
        if (m.type == MacroType::PICKUP) pickups.push_back(m);
    }
    if (pickups.empty()) return plan;

    TeamSide mySide = state.activeTeam;
    int fdx = (mySide == TeamSide::HOME) ? 1 : -1;
    double bestValue = -1e9;
    GameState bestProjected;

    // Passive guard choreography (user 2026-08-05): a teammate ALREADY
    // standing next to the loose ball guards it for free -- spending their
    // activation as a safe-stage "backup" buys nothing (they physically are
    // one) and costs the cage-fill stage its cheapest corner. Excluded from
    // the safe stage, kept unmoved for cage-fill.
    std::vector<int> guardIds;
    state.forEachOnPitch(mySide, [&](const Player& p) {
        if (p.state != PlayerState::STANDING) return;
        if (p.hasMoved || p.hasActed) return;
        if (p.position.distanceTo(state.ball.position) == 1) {
            guardIds.push_back(p.id);
        }
    });

    // Deterministic projection of the carrier's post-pickup square, using
    // the SAME stall-aware arithmetic expandPickup's advance continuation
    // runs (carrierStallAwareSteps is shared, not replicated). Returns the
    // advance step count, or 0 when no continuation is projected.
    auto projectAdvanceSteps = [&](const GameState& s, int pickerId) {
        const Player& pk = s.getPlayer(pickerId);
        int approach = pk.position.distanceTo(s.ball.position);
        int mvAfter = static_cast<int>(pk.movementRemaining) - approach;
        if (mvAfter <= 0) return 0;
        Player tmp = pk;
        tmp.position = s.ball.position;
        tmp.movementRemaining = static_cast<int8_t>(mvAfter);
        return carrierStallAwareSteps(s, tmp, s.getTeamState(mySide));
    };

    for (const Macro& pickup : pickups) {
        // Projected corner slots for this pickup (step-2 smart targeting): a
        // safe backup parked on a FUTURE corner slot is both the pickup
        // insurance and a finished corner -- one figure, two jobs. Used as a
        // candidate-ordering tiebreak only; arrival keeps precedence (user
        // 2026-08-05: the bounce insurance comes first).
        Position futureSlots[4];
        int nFutureSlots = 0;
        {
            int s = projectAdvanceSteps(state, pickup.playerId);
            Position np{static_cast<int8_t>(state.ball.position.x + fdx * s),
                        state.ball.position.y};
            if (s >= 1 && np.isOnPitch()) {
                futureSlots[nFutureSlots++] = {static_cast<int8_t>(np.x + fdx),
                                               static_cast<int8_t>(np.y - 1)};
                futureSlots[nFutureSlots++] = {static_cast<int8_t>(np.x + fdx),
                                               static_cast<int8_t>(np.y + 1)};
                futureSlots[nFutureSlots++] = {static_cast<int8_t>(np.x - fdx),
                                               static_cast<int8_t>(np.y - 1)};
                futureSlots[nFutureSlots++] = {static_cast<int8_t>(np.x - fdx),
                                               static_cast<int8_t>(np.y + 1)};
            }
        }
        auto isFutureSlot = [&](Position p) {
            for (int i = 0; i < nFutureSlots; ++i) {
                if (futureSlots[i] == p) return true;
            }
            return false;
        };
        // --- Safe stage: dice-free backup positioning, ordered ball-first.
        // With a loose ball, generation targets a free ball-adjacent square
        // for every free player (item 11's REPOSITION fix). Candidates are
        // REGENERATED from the evolving projection after every accepted
        // macro rather than harvested once from the root: "nearest free
        // adjacent square" is computed against current occupancy, so a
        // one-shot harvest hands every teammate the SAME square (observed on
        // mined state g0000: 6 macros, 1 arrival) while regeneration
        // re-targets each subsequent backup to the next free square.
        // Termination: every accepted macro marks its player hasMoved (so
        // they leave the candidate pool) and every rejected player is
        // remembered -- the pool strictly shrinks.
        GameState projected = state.clone();
        std::vector<Macro> accepted;
        std::vector<int> rejected;
        for (int gid : guardIds) {
            if (gid != pickup.playerId) rejected.push_back(gid);
        }
        auto isRejected = [&](int id) {
            return std::find(rejected.begin(), rejected.end(), id) != rejected.end();
        };
        while (static_cast<int>(accepted.size()) < MAX_SAFE_BACKUPS) {
            std::vector<Macro> cur;
            getAvailableMacros(projected, cur, config_.dauntlessInOffer);
            std::vector<Macro> cands;
            for (const auto& m : cur) {
                if (m.type != MacroType::REPOSITION) continue;
                if (m.playerId == pickup.playerId) continue;  // picker acts once, at the branch
                if (isRejected(m.playerId)) continue;
                cands.push_back(m);
            }
            // Pick the RIGHT (at most) two backups, not merely the first two
            // generated: prefer players who actually ARRIVE at their target
            // square this turn (a backup that stops short denies nothing),
            // then the player nearest the ball (cheapest -- fewest squares
            // of positioning spent on this job).
            std::sort(cands.begin(), cands.end(),
                      [&](const Macro& a, const Macro& b) {
                          const Player& pa = projected.getPlayer(a.playerId);
                          const Player& pb = projected.getPlayer(b.playerId);
                          bool arriveA = pa.position.distanceTo(a.targetPos) <=
                                         static_cast<int>(pa.movementRemaining);
                          bool arriveB = pb.position.distanceTo(b.targetPos) <=
                                         static_cast<int>(pb.movementRemaining);
                          if (arriveA != arriveB) return arriveA;
                          // Step-2 smart targeting: among equally-arriving
                          // backups prefer the one whose target doubles as a
                          // projected corner slot.
                          bool slotA = isFutureSlot(a.targetPos);
                          bool slotB = isFutureSlot(b.targetPos);
                          if (slotA != slotB) return slotA;
                          int da = pa.position.distanceTo(state.ball.position);
                          int db = pb.position.distanceTo(state.ball.position);
                          if (da != db) return da < db;
                          return a.playerId < b.playerId;
                      });
            bool tookOne = false;
            for (const auto& m : cands) {
                // Probe on the EVOLVING projection: the safety of step k only
                // means anything given steps 1..k-1.
                auto pr = probeMacro(projected, m);
                if (pr.pto > SAFE_PTO || pr.meanActions < 0.5) {
                    rejected.push_back(m.playerId);  // dicey here, or a no-op
                    continue;
                }
                GameState next = projected.clone();
                auto res = greedyExpandMacro(next, m, dice_);
                if (res.turnover || next.phase != GamePhase::PLAY ||
                    next.activeTeam != mySide) {
                    rejected.push_back(m.playerId);  // <=2% tail materialized
                    continue;
                }
                // Picker-corridor check (user doctrine 2026-08-07, mined
                // state g0008): a backup must never wall off the picker's
                // physical route to the ball -- with only 2-3 free squares
                // around it, "nearest free adjacent" targets can consume the
                // picker's entire approach and the plan zeroes itself out.
                // Deterministic (all dice pinned to 6, greedy walk is the
                // executor's own): if the pickup no longer reaches the ball
                // on this projection, reject the backup; candidate
                // regeneration offers the next square/body. Backup cap
                // (MAX_SAFE_BACKUPS=2) is untouched -- this only vetoes
                // squares, never adds bodies.
                {
                    GameState reach = next.clone();
                    MaxDiceRoller sixes;
                    greedyExpandMacro(reach, pickup, sixes);
                    bool pickerReaches = reach.ball.isHeld &&
                                         reach.ball.carrierId == pickup.playerId;
                    if (!pickerReaches) {
                        rejected.push_back(m.playerId);
                        continue;
                    }
                }
                projected = std::move(next);
                accepted.push_back(m);
                tookOne = true;
                break;  // regenerate candidates against the new occupancy
            }
            if (!tookOne) break;
        }
        // Item 11 closure metric, measured on the projection the branch will
        // actually roll from: standing teammates adjacent to the ball.
        int backups = 0;
        projected.forEachOnPitch(mySide, [&](const Player& p) {
            if (p.id == pickup.playerId) return;
            if (p.state != PlayerState::STANDING) return;
            if (p.position.distanceTo(state.ball.position) == 1) backups++;
        });

        // --- The single branch point.
        BranchStats bs = sampleBranch(projected, pickup, mySide);
        // Adoption floor (see MIN_PICKUP_SUCCESS): a hopeless roll is not a
        // plan -- leave the turn to search(), which can blitz the marker off
        // the ball or refuse the pickup entirely.
        if (bs.pSuccess < MIN_PICKUP_SUCCESS) continue;
        double value = bs.pSuccess * bs.valueSuccess +
                       (1.0 - bs.pSuccess) * bs.valueFail;

        if (value > bestValue) {
            bestValue = value;
            plan.safeMacros = std::move(accepted);
            plan.pickupMacro = pickup;
            plan.pSuccess = bs.pSuccess;
            plan.valueSuccess = bs.valueSuccess;
            plan.valueFail = bs.valueFail;
            plan.planValue = value;
            plan.backupCount = backups;
            plan.valid = true;
            bestProjected = projected.clone();
        }
    }
    if (!plan.valid) return plan;

    // --- Step 2 (2026-08-07): cage-fill stage on the winning branch.
    // Still-unmoved teammates (the passive guard first among them) walk onto
    // the diagonal corner slots around the carrier's projected post-pickup
    // square. Slot assignment reuses CageAdvancePlanner::tryAssign; every
    // emitted macro passes the same dice-free probe regime as the safe
    // stage. The stage rides the SUCCESS branch only -- its macros carry the
    // requireHeldBall validity condition, so a failed pickup drops them and
    // the turn falls back to search().
    {
        GameState proj = bestProjected.clone();
        Player& picker = proj.getPlayer(plan.pickupMacro.playerId);
        int steps = projectAdvanceSteps(proj, picker.id);
        if (steps < 1) return plan;  // no advance continuation projected

        int approach = picker.position.distanceTo(state.ball.position);
        Position newPos{static_cast<int8_t>(state.ball.position.x + fdx * steps),
                        state.ball.position.y};
        if (!newPos.isOnPitch()) return plan;
        // The executor moves the carrier BEFORE the fills, so the vacate-
        // first choreography tryAssign supports for a blocked carrier square
        // cannot run here -- an occupied projection square means the carrier
        // will stop short and the whole slot geometry shifts. Skip the stage.
        if (proj.getPlayerAtPosition(newPos) != nullptr) return plan;

        picker.movementRemaining = static_cast<int8_t>(
            std::max(0, static_cast<int>(picker.movementRemaining) - approach));
        picker.position = state.ball.position;
        picker.hasMoved = true;
        proj.ball = BallState::carried(state.ball.position, picker.id);

        // First real use of the reservedPlayerIds role budget (design
        // 2026-08-06): the picker and the spent safe backups must not be
        // drafted as corners. (Both are also excluded structurally --
        // carrier param / hasMoved -- the reservation is the explicit
        // contract.)
        std::vector<int> reserved;
        reserved.push_back(picker.id);
        for (const auto& m : plan.safeMacros) reserved.push_back(m.playerId);

        auto ar = cageHelper_.tryAssign(proj, picker, steps, reserved);

        // Probe/execute the fills on the post-advance projection: the
        // carrier stands on the projected square while the corners walk.
        picker.position = newPos;
        proj.ball = BallState::carried(newPos, picker.id);
        for (const auto& sa : ar.slots) {
            if (sa.playerId <= 0 || sa.stayPut) continue;
            Macro m{MacroType::REPOSITION, sa.playerId, -1, sa.slot};
            auto pr = probeMacro(proj, m);
            if (pr.pto > SAFE_PTO || pr.meanActions < 0.5) continue;
            GameState next = proj.clone();
            auto res = greedyExpandMacro(next, m, dice_);
            if (res.turnover || next.phase != GamePhase::PLAY ||
                next.activeTeam != mySide) {
                continue;
            }
            proj = std::move(next);
            plan.cageFillMacros.push_back(m);
        }
    }

    return plan;
}

} // namespace bb
