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

} // namespace bb
