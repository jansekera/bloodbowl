#pragma once

#include "bb/game_state.h"
#include "bb/enums.h"
#include "bb/position.h"
#include <vector>

namespace bb {

struct Action {
    ActionType type = ActionType::END_TURN;
    int playerId = -1;
    int targetId = -1;
    Position target{-1, -1};
};

void getAvailableActions(const GameState& state, std::vector<Action>& out);

} // namespace bb
