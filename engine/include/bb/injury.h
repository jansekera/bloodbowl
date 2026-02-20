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

void resolveCrowdSurf(GameState& state, int playerId, DiceRollerBase& dice,
                      std::vector<GameEvent>* events);

} // namespace bb
