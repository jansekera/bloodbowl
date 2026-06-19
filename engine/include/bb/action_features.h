#pragma once

#include "bb/game_state.h"
#include "bb/rules_engine.h"

namespace bb {

constexpr int NUM_ACTION_FEATURES = 23;

// Extract features describing an action in the context of a game state.
// [0-14]  tactical category (action type, player stats, block dice, ...)
// [15-22] move identity (source/target coords, deltas, player index) — added 2026-06-19
//         to break feature collisions: ~50% of decisions had the best action
//         feature-identical to a worse one, capping policy top1. See
//         memory project-neural-policy-rootcause.
// out must point to at least NUM_ACTION_FEATURES floats.
void extractActionFeatures(const GameState& state, const Action& action, float* out);

} // namespace bb
