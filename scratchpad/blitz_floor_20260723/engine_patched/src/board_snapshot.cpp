#include "bb/board_snapshot.h"

namespace bb {

BoardSnapshot captureBoardSnapshot(const GameState& state) {
    BoardSnapshot snap;

    if (state.ball.isOnPitch()) {
        snap.ballX = state.ball.position.x;
        snap.ballY = state.ball.position.y;
    }
    snap.ballHeld = state.ball.isHeld;
    snap.ballCarrierId = state.ball.carrierId;

    auto snapshotTeam = [&](TeamSide side, std::vector<PlayerSnapshot>& out) {
        state.forEachOnPitch(side, [&](const Player& p) {
            PlayerSnapshot ps;
            ps.id = p.id;
            ps.x = p.position.x;
            ps.y = p.position.y;
            if (p.state == PlayerState::STANDING) ps.state = 0;
            else if (p.state == PlayerState::PRONE) ps.state = 1;
            else if (p.state == PlayerState::STUNNED) ps.state = 2;
            else ps.state = 3;
            ps.hasBall = (state.ball.isHeld && state.ball.carrierId == p.id);
            out.push_back(ps);
        });
    };
    snapshotTeam(TeamSide::HOME, snap.homePlayers);
    snapshotTeam(TeamSide::AWAY, snap.awayPlayers);

    return snap;
}

} // namespace bb
