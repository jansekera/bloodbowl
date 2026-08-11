#include "bb/turn_handler.h"
#include "bb/helpers.h"

namespace bb {

void resolveEndTurn(GameState& state, std::vector<GameEvent>* events, bool wasTurnover) {
    TeamSide current = state.activeTeam;

    // Secret Weapon players are NOT sent off here. CRP: "Once a DRIVE ends
    // that this player has played in at any point, the referee orders the
    // player to be sent off." This used to fire at the end of every team
    // turn, so a Deathroller left the pitch after his own first turn instead
    // of playing the drive out. The ejection now happens where the drive
    // actually ends, in setupHalfOrDrive (game_simulator.cpp).

    // Switch active team
    state.activeTeam = opponent(current);

    // Increment turn number for the new active team
    TeamState& newTeam = state.getTeamState(state.activeTeam);
    newTeam.turnNumber++;

    // Reset players for new turn
    state.resetPlayersForNewTurn(state.activeTeam);
    newTeam.resetForNewTurn();

    // Clear turnover flag
    state.turnoverPending = false;

    if (wasTurnover) {
        emitEvent(events, {GameEvent::Type::TURNOVER, -1, -1, {}, {},
                          newTeam.turnNumber, true});
    }
}

bool checkTouchdown(const GameState& state) {
    if (!state.ball.isHeld) return false;

    const Player& carrier = state.getPlayer(state.ball.carrierId);
    if (carrier.state != PlayerState::STANDING) return false;

    // Home team scores in Away endzone (x=25), Away team scores in Home endzone (x=0)
    if (carrier.teamSide == TeamSide::HOME) {
        return carrier.position.isInEndZone(false); // away endzone
    } else {
        return carrier.position.isInEndZone(true); // home endzone
    }
}

bool checkHalfOver(const GameState& state) {
    // Each team gets 8 turns per half
    return state.getTeamState(state.activeTeam).turnNumber > 8;
}

} // namespace bb
