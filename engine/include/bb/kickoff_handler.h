#pragma once

#include "bb/game_state.h"
#include "bb/dice.h"
#include "bb/game_event.h"
#include <vector>

namespace bb {

// Full kickoff: scatter ball, resolve kickoff event (2D6 table), handle catch/bounce
void resolveKickoff(GameState& state, DiceRollerBase& dice, std::vector<GameEvent>* events);

} // namespace bb
