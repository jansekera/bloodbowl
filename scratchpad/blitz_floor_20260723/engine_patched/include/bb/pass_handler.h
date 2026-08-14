#pragma once

#include "bb/game_state.h"
#include "bb/dice.h"
#include "bb/game_event.h"
#include "bb/action_result.h"
#include <vector>

namespace bb {

ActionResult resolvePass(GameState& state, int passerId, Position target,
                         DiceRollerBase& dice, std::vector<GameEvent>* events);

ActionResult resolveHandOff(GameState& state, int giverId, int receiverId,
                            DiceRollerBase& dice, std::vector<GameEvent>* events);

} // namespace bb
