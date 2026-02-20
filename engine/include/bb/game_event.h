#pragma once

#include "bb/position.h"
#include <cstdint>

namespace bb {

struct GameEvent {
    enum class Type : uint8_t {
        PLAYER_MOVE, DODGE, GFI, BLOCK, PUSH, INJURY,
        TOUCHDOWN, TURNOVER, BALL_BOUNCE, PASS, CATCH,
        PICKUP, FOUL, KICKOFF, WEATHER_CHANGE, SKILL_USED,
        KNOCKED_DOWN, ARMOR_BREAK, CASUALTY, REGENERATION
    };

    Type type;
    int playerId = -1;
    int targetId = -1;
    Position from{};
    Position to{};
    int roll = 0;
    bool success = false;
};

} // namespace bb
