#include "bb/turn_handler.h"
#include "bb/helpers.h"

namespace bb {

void resolveEndTurn(GameState& state, std::vector<GameEvent>* events) {
    TeamSide current = state.activeTeam;

    // Eject Secret Weapon players
    state.forEachOnPitch(current, [&](Player& p) {
        if (p.hasSkill(SkillName::SecretWeapon)) {
            p.state = PlayerState::EJECTED;
            p.position = {-1, -1};
        }
    });

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

    emitEvent(events, {GameEvent::Type::TURNOVER, -1, -1, {}, {},
                      newTeam.turnNumber, true});
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
