#pragma once

#include "bb/game_state.h"
#include "bb/rules_engine.h"
#include "bb/dice.h"
#include "bb/action_features.h"
#include <vector>
#include <cstdint>

namespace bb {

enum class MacroType : uint8_t {
    SCORE = 0,
    ADVANCE,
    CAGE,
    BLITZ,
    BLOCK,
    PICKUP,
    PASS_ACTION,
    FOUL,
    REPOSITION,
    END_TURN,
    BLITZ_AND_SCORE,
    MACRO_COUNT  // = 11
};

struct Macro {
    MacroType type = MacroType::END_TURN;
    int playerId = -1;      // primary player
    int targetId = -1;      // target (blitz/block/foul/pass)
    Position targetPos{-1, -1}; // target position (reposition)
};

struct MacroExpansionResult {
    std::vector<Action> actions;
    bool turnover = false;
};

// Generate all available macros for the current game state
void getAvailableMacros(const GameState& state, std::vector<Macro>& out);

// Expand a macro into a sequence of low-level actions via greedy heuristics.
// Modifies state in-place as actions are executed.
MacroExpansionResult greedyExpandMacro(GameState& state, const Macro& macro,
                                       DiceRollerBase& dice);

// Extract 15 features for a macro (same count as NUM_ACTION_FEATURES for policy reuse)
void extractMacroFeatures(const GameState& state, const Macro& macro, float* out);

} // namespace bb
