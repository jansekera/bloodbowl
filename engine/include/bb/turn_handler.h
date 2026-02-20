#pragma once

#include "bb/game_state.h"
#include "bb/game_event.h"
#include <vector>

namespace bb {

void resolveEndTurn(GameState& state, std::vector<GameEvent>* events);
bool checkTouchdown(const GameState& state);
bool checkHalfOver(const GameState& state);

} // namespace bb
