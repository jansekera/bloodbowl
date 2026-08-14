#pragma once

#include "bb/game_state.h"
#include "bb/dice.h"
#include "bb/game_event.h"
#include "bb/enums.h"
#include <vector>

namespace bb {

struct BigGuyResult {
    bool actionBlocked = false;  // true = action cannot proceed
    bool proceed = true;         // for Bloodlust bite: action still proceeds
};

BigGuyResult resolveBigGuyCheck(GameState& state, int playerId, ActionType actionType,
                                DiceRollerBase& dice, std::vector<GameEvent>* events);

} // namespace bb
