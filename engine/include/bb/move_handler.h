#pragma once

#include "bb/game_state.h"
#include "bb/dice.h"
#include "bb/game_event.h"
#include "bb/action_result.h"
#include <vector>

namespace bb {

ActionResult resolveMoveStep(GameState& state, int playerId, Position to,
                             DiceRollerBase& dice, std::vector<GameEvent>* events);
ActionResult resolveLeap(GameState& state, int playerId, Position to,
                         DiceRollerBase& dice, std::vector<GameEvent>* events);
// Q3 (30.08.2026): kolik vstání se v hledání skutečně provedlo, a kolik
// z toho vedle STOJÍCÍHO soupeře -- tedy v drahé větvi.
long takeStoodUpInSearch();
long takeStoodUpNextToEnemyInSearch();

ActionResult resolveStandUp(GameState& state, int playerId, DiceRollerBase& dice,
                            std::vector<GameEvent>* events);

} // namespace bb
