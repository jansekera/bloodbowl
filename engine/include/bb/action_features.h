#pragma once

#include "bb/game_state.h"
#include "bb/rules_engine.h"

namespace bb {

constexpr int NUM_ACTION_FEATURES = 15;

// Extract 15 features describing an action in the context of a game state.
// out must point to at least NUM_ACTION_FEATURES floats.
void extractActionFeatures(const GameState& state, const Action& action, float* out);

} // namespace bb
