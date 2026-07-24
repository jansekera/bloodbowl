#pragma once

#include "bb/position.h"
#include <cstdint>

namespace bb {

struct GameEvent {
    enum class Type : uint8_t {
        PLAYER_MOVE, DODGE, GFI, BLOCK, PUSH, INJURY,
        TOUCHDOWN, TURNOVER, BALL_BOUNCE, PASS, CATCH,
        PICKUP, FOUL, KICKOFF, WEATHER_CHANGE, SKILL_USED,
        KNOCKED_DOWN, ARMOR_BREAK, CASUALTY, REGENERATION,
        EJECTED  // 2026-07-21: fouler sent off on doubles -- previously
                 // reused INJURY, ambiguous in raw event logs (see
                 // evidence/foul_success_field_bug_20260721.md)
    };

    Type type;
    int playerId = -1;
    int targetId = -1;
    Position from{};
    Position to{};
    int roll = 0;
    bool success = false;
    // 2026-07-24 (item 3.6): individual d6 faces for 2d6-composed rolls
    // (armour/injury) -- `roll` keeps carrying the final modified sum as
    // before (unchanged for all other event types), these are 0 when not
    // applicable (single-die rolls, or non-roll events). Added because
    // forensic replay analysis (e.g. reconstructing a FOUL by hand) could
    // not tell an unmodified 2d6 result apart from an assist/skill-modified
    // one using only the summed `roll` field.
    int die1 = 0;
    int die2 = 0;
};

} // namespace bb
