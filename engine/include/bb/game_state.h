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
    // Package G, layer 2 (2026-08-10): a squad is larger than the eleven on
    // the pitch. Slots 0..10 are the starting eleven and keep exactly the
    // identities they had before this change; the rest are SUBSTITUTES, who
    // start in Reserves and come on when a team-mate is KO'd or hurt. The
    // engine does not model Team Value at all (it appears only in roster
    // comments), so a bench costs nothing mechanically -- but both sides get
    // the same one, and pricing it is package E's job.
    // Layout keeps every pre-existing ID intact -- the two starting elevens
    // stay 1-11 and 12-22 -- and appends the benches after them. Renumbering
    // instead would have rewritten 41 tests for no gain.
    //   index 0..10   HOME starters   ids  1..11
    //   index 11..21  AWAY starters   ids 12..22
    //   index 22..23  HOME bench      ids 23..24
    //   index 24..25  AWAY bench      ids 25..26
    static constexpr int STARTERS = 11;
    static constexpr int BENCH = 2;                    // substitutes per side
    static constexpr int SQUAD_SIZE = STARTERS + BENCH;
    static constexpr int PLAYERS_TOTAL = 2 * SQUAD_SIZE;
    static constexpr int homeBaseId() { return 1; }
    static constexpr int awayBaseId() { return 1 + STARTERS; }
    static constexpr int baseIdFor(TeamSide s) {
        return s == TeamSide::HOME ? homeBaseId() : awayBaseId();
    }
    // First bench ID for a side (the starters are baseIdFor(s) .. +STARTERS-1)
    static constexpr int benchBaseId(TeamSide s) {
        return 1 + 2 * STARTERS + (s == TeamSide::HOME ? 0 : BENCH);
    }
    // Squad slot n (0..SQUAD_SIZE-1) -> player ID
    static constexpr int squadId(TeamSide s, int n) {
        return n < STARTERS ? baseIdFor(s) + n : benchBaseId(s) + (n - STARTERS);
    }

    int half = 1;
    GamePhase phase = GamePhase::COIN_TOSS;
    TeamSide activeTeam = TeamSide::HOME;
    TeamState homeTeam;
    TeamState awayTeam;
    std::array<Player, PLAYERS_TOTAL> players{};  // home first, then away; ID = index+1
    BallState ball;
    bool turnoverPending = false;
    // One-activation-per-player tracking: id of the player whose activation is
    // currently open (-1 = none). Used by executeAction to close out a mover's
    // activation (hasActed = true) when a different player starts acting, so a
    // player who moved cannot be reactivated later in the same team-turn.
    int currentActivationId = -1;
    TeamSide kickingTeam = TeamSide::AWAY;
    Weather weather = Weather::NICE;
    RosterSpeed receiverSpeed = RosterSpeed::MIXED;

    GameState();

    // Player lookup by ID (1..PLAYERS_TOTAL)
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
        for (int n = 0; n < SQUAD_SIZE; ++n) {
            func(players[squadId(side, n) - 1]);
        }
    }

    template<typename F>
    void forEachPlayer(TeamSide side, F&& func) const {
        for (int n = 0; n < SQUAD_SIZE; ++n) {
            func(players[squadId(side, n) - 1]);
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
