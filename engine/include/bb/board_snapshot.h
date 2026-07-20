#pragma once

#include "bb/game_state.h"
#include <string>
#include <vector>

namespace bb {

// Snapshot of a single player for replay
struct PlayerSnapshot {
    int id = -1;
    int8_t x = -1, y = -1;
    uint8_t state = 0;  // 0=standing, 1=prone, 2=stunned, 3=off
    bool hasBall = false;
    std::string name;    // positional name (e.g., "Blitzer")
};

// Board-only snapshot (players + ball), shared by TurnLog and PolicyDecision
// so both replay logging and policy-training decision logging capture the
// same raw per-player state from one implementation.
struct BoardSnapshot {
    std::vector<PlayerSnapshot> homePlayers;
    std::vector<PlayerSnapshot> awayPlayers;
    int8_t ballX = -1, ballY = -1;
    bool ballHeld = false;
    int ballCarrierId = -1;
};

BoardSnapshot captureBoardSnapshot(const GameState& state);

} // namespace bb
