#pragma once

#include "bb/enums.h"
#include "bb/player.h"
#include "bb/team_state.h"
#include "bb/ball_state.h"
#include <array>
#include <functional>

namespace bb {

class GameState {
public:
    int half = 1;
    GamePhase phase = GamePhase::COIN_TOSS;
    TeamSide activeTeam = TeamSide::HOME;
    TeamState homeTeam;
    TeamState awayTeam;
    std::array<Player, 22> players{};  // 0-10 = home (IDs 1-11), 11-21 = away (IDs 12-22)
    BallState ball;
    bool turnoverPending = false;
    TeamSide kickingTeam = TeamSide::AWAY;
    Weather weather = Weather::NICE;

    GameState();

    // Player lookup by ID (1-22)
    Player& getPlayer(int id);
    const Player& getPlayer(int id) const;

    // Find player at position, or nullptr
    Player* getPlayerAtPosition(Position pos);
    const Player* getPlayerAtPosition(Position pos) const;

    // Team state lookup
    TeamState& getTeamState(TeamSide side);
    const TeamState& getTeamState(TeamSide side) const;

    // Iteration over players of a team
    template<typename F>
    void forEachPlayer(TeamSide side, F&& func) {
        int start = (side == TeamSide::HOME) ? 0 : 11;
        for (int i = start; i < start + 11; ++i) {
            func(players[i]);
        }
    }

    template<typename F>
    void forEachPlayer(TeamSide side, F&& func) const {
        int start = (side == TeamSide::HOME) ? 0 : 11;
        for (int i = start; i < start + 11; ++i) {
            func(players[i]);
        }
    }

    template<typename F>
    void forEachOnPitch(TeamSide side, F&& func) {
        forEachPlayer(side, [&](Player& p) {
            if (p.isOnPitch()) func(p);
        });
    }

    template<typename F>
    void forEachOnPitch(TeamSide side, F&& func) const {
        forEachPlayer(side, [&](const Player& p) {
            if (p.isOnPitch()) func(p);
        });
    }

    void resetPlayersForNewTurn(TeamSide side);

    // Trivial copy for MCTS branching
    GameState clone() const { return *this; }
};

} // namespace bb
