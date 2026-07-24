#pragma once

#include "bb/game_state.h"
#include "bb/dice.h"
#include "bb/game_event.h"
#include <vector>

namespace bb {

struct InjuryContext {
    int armourModifier = 0;   // MightyBlow, DirtyPlayer, foul assists
    int injuryModifier = 0;   // MightyBlow, Stunty
    bool hasClaw = false;     // armor broken on 8+
    bool hasStakes = false;   // blocks Regeneration
    bool hasDecay = false;    // roll injury twice, take worse
    bool hasNurglesRot = false;
};

// Returns true if armor was broken
bool resolveArmourAndInjury(GameState& state, int playerId, DiceRollerBase& dice,
                            const InjuryContext& ctx, std::vector<GameEvent>* events);

// Rolls 2d6 injury (+ modifiers), applies Stunty/ThickSkull/Regeneration/Decay,
// sets player state (STUNNED/KO/INJURED) and emits the matching INJURY/
// CASUALTY/SKILL_USED/REGENERATION event(s). Exposed (not just used via
// resolveArmourAndInjury above) for callers that resolve their own armour
// roll separately -- e.g. FOUL, whose armour roll includes assist modifiers
// not expressible via InjuryContext::armourModifier -- so they still get
// event-emission and skill-handling parity with the shared BLOCK/bomb/BC path.
// Returns the injury roll.
int resolveInjuryRoll(GameState& state, int playerId, DiceRollerBase& dice,
                      const InjuryContext& ctx, std::vector<GameEvent>* events);

void resolveCrowdSurf(GameState& state, int playerId, DiceRollerBase& dice,
                      std::vector<GameEvent>* events);

} // namespace bb
