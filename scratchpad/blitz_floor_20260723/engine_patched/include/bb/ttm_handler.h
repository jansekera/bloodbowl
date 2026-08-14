#pragma once

#include "bb/game_state.h"
#include "bb/dice.h"
#include "bb/game_event.h"
#include "bb/action_result.h"
#include <vector>

namespace bb {

ActionResult resolveThrowTeamMate(GameState& state, int throwerId, int projectileId,
                                  Position target, DiceRollerBase& dice,
                                  std::vector<GameEvent>* events);

} // namespace bb
