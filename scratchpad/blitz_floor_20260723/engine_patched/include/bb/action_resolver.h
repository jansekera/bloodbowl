#pragma once

#include "bb/game_state.h"
#include "bb/dice.h"
#include "bb/game_event.h"
#include "bb/action_result.h"
#include "bb/rules_engine.h"
#include <vector>

namespace bb {

ActionResult resolveAction(GameState& state, const Action& action,
                           DiceRollerBase& dice, std::vector<GameEvent>* events);

// resolveAction + auto-end-turn on turnover + check touchdown/half
ActionResult executeAction(GameState& state, const Action& action,
                           DiceRollerBase& dice, std::vector<GameEvent>* events);

} // namespace bb
