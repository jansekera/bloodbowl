#include "bb/turn_planner.h"
#include <algorithm>
#include <cmath>

namespace bb {

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

bool stagedMacroStillValid(const GameState& state, const Macro& m) {
    if (state.phase != GamePhase::PLAY) return false;
    if (m.playerId <= 0) return false;
    const Player& p = state.getPlayer(m.playerId);
    if (!p.isOnPitch() || p.state != PlayerState::STANDING) return false;
    if (p.hasMoved || p.hasActed) return false;

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
    : config_(config), evaler_(vf, config, seed), dice_(seed + 4242) {}

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

    std::vector<Macro> macros;
    getAvailableMacros(state, macros);

    // PICKUP candidates arrive best-first (generation ordering contract,
    // macro_actions.cpp; item 7 emits at most two).
    std::vector<Macro> pickups;
    for (const auto& m : macros) {
        if (m.type == MacroType::PICKUP) pickups.push_back(m);
    }
    if (pickups.empty()) return plan;

    TeamSide mySide = state.activeTeam;
    double bestValue = -1e9;

    for (const Macro& pickup : pickups) {
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
        auto isRejected = [&](int id) {
            return std::find(rejected.begin(), rejected.end(), id) != rejected.end();
        };
        while (static_cast<int>(accepted.size()) < MAX_SAFE_BACKUPS) {
            std::vector<Macro> cur;
            getAvailableMacros(projected, cur);
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
        }
    }

    return plan;
}

} // namespace bb
