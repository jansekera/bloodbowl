#pragma once

#include "bb/game_state.h"
#include "bb/dice.h"
#include "bb/game_event.h"
#include <vector>

namespace bb {

struct InjuryContext {
    int armourModifier = 0;   // DirtyPlayer, foul assists
    int injuryModifier = 0;   // Stunty
    // Mighty Blow is a flag, not a modifier, because CRP spends it on ONE roll:
    // "you only modify one of the dice rolls, so if you decide to use Mighty
    // Blow to modify the Armour roll, you may not modify the Injury roll as
    // well." It used to be added to both at once. resolveArmourAndInjury picks
    // which roll it lands on -- see there.
    bool mightyBlow = false;
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
// outDoubles (nepovinné): true, když na hodu na ZRANĚNÍ padl dublet. BB2016
// l. 1878: "if the Armour AND/OR Injury roll is a doubles" -> faulující je
// vyloučen a tým má turnover. Foul handler dosud koukal jen na armour kostky,
// protože injury se dělá touhle sdílenou BLOKOVOU funkcí, která kostky
// nevracela -- klasický šev "faul přebírá blokovou mašinérii" (oprava 21.08.).
int resolveInjuryRoll(GameState& state, int playerId, DiceRollerBase& dice,
                      const InjuryContext& ctx, std::vector<GameEvent>* events,
                      bool* outDoubles = nullptr);

void resolveCrowdSurf(GameState& state, int playerId, DiceRollerBase& dice,
                      std::vector<GameEvent>* events);

} // namespace bb
