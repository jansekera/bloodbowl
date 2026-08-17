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
    HAND_OFF_SCORE,
    PASS_SCORE,
    CHAIN_SCORE,
    MACRO_COUNT  // = 14
};

struct Macro {
    MacroType type = MacroType::END_TURN;
    int playerId = -1;      // primary player
    int targetId = -1;      // target (blitz/block/foul/pass/relay)
    Position targetPos{-1, -1}; // target position (reposition)
    int thirdId = -1;       // third player (CHAIN_SCORE scorer)
    // REPOSITION only: extra GFI squares the walk may roll for (0-2).
    // Default 0 keeps REPOSITION dice-free; the cage-advance planner sets
    // it for the ball carrier in tempo emergencies (user doctrine 2026-08-04:
    // "the carrier MUST arrive even at GFI dice cost near the end").
    int gfiAllowance = 0;
};

struct MacroExpansionResult {
    std::vector<Action> actions;
    bool turnover = false;
};

// Generate all available macros for the current game state
// dauntlessInOffer: price a block at the strength Dauntless would equalise to.
// ⚠️ This parameter default is NOT production. Production is
// MCTSConfig::dauntlessInOffer, which has been TRUE since 17.08.2026, and every
// engine call site passes config_.dauntlessInOffer explicitly. The false here
// exists only so the unit tests that call this directly keep asserting the raw
// offer; a new direct caller that omits the argument gets the non-production
// filter, which is the same "prices a different action than the one played"
// trap the arm was written to fix. Pass it explicitly.
void getAvailableMacros(const GameState& state, std::vector<Macro>& out,
                        bool dauntlessInOffer = false);

// How many times the block offer priced a block at the strength Dauntless would
// equalise to, since the last call -- and resets. Diagnostics for the A/B: an
// arm that changes nothing must be distinguishable from a change that does
// nothing. Only ever non-zero where dauntlessInOffer is set, so it needs no
// per-side bookkeeping.
long takeDauntlessOfferCount();

// How many times a HAND_OFF (a PASS_ACTION macro whose target is adjacent) was
// put on the menu, since the last call -- and resets. P21: the corpus logs zero
// hand-offs played across 3000 games, and without this there is no way to tell
// a gate that never offers from a search that never picks.
long takeHandOffOfferCount();

// Expand a macro into a sequence of low-level actions via greedy heuristics.
// Modifies state in-place as actions are executed.
MacroExpansionResult greedyExpandMacro(GameState& state, const Macro& macro,
                                       DiceRollerBase& dice);

// Stall-aware step budget for a ball carrier's own movement (shared by the
// ADVANCE expansion and the PICKUP advance continuation). Exported so the
// staged pickup planner can project the carrier's post-pickup position for
// its cage-fill stage with the SAME arithmetic the executor will use.
int carrierStallAwareSteps(const GameState& state, const Player& carrier,
                           const TeamState& myTeam);

// Extract NUM_ACTION_FEATURES features for a macro (shared count with action_features.h for policy reuse)
void extractMacroFeatures(const GameState& state, const Macro& macro, float* out);

} // namespace bb
