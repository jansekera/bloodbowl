#pragma once

#include "bb/game_state.h"
#include "bb/position.h"

namespace bb {

struct MoveTarget {
    Position pos{};
    bool requiresDodge = false;
    bool isGfi = false;
};

// Can the player reach any square adjacent to target?
// If yes, returns true and sets outAdjacent to the best adjacent square.
bool canReachAdjacentTo(const GameState& state, const Player& player,
                        Position target, Position& outAdjacent);

// Get all valid single-step move targets for a player.
// Returns the number of targets written to out (up to maxOut).
int getValidMoveTargets(const GameState& state, const Player& player,
                        MoveTarget* out, int maxOut);

} // namespace bb
