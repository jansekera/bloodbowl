#pragma once

#include "bb/game_state.h"
#include "bb/enums.h"

namespace bb {

constexpr int NUM_FEATURES = 70;

// Extract 70 features from game state, from the given team's perspective.
// out must point to at least NUM_FEATURES floats.
void extractFeatures(const GameState& state, TeamSide perspective, float* out);

} // namespace bb
