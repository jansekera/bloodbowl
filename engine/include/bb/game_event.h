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
        EJECTED, // 2026-07-21: fouler sent off on doubles -- previously
                 // reused INJURY, ambiguous in raw event logs (see
                 // evidence/foul_success_field_bug_20260721.md)
        HAND_OFF, // 2026-08-14: a hand-off used to leave no trace of its own --
                 // resolveHandOff calls resolveCatch and the log showed a bare
                 // CATCH, indistinguishable from catching a bounce or a kick.
                 // The verification run for the hand-off pricing fix therefore
                 // reported zero hand-offs in 3000 games while the carrier
                 // distribution had visibly moved, i.e. the instrument was
                 // measuring a string the engine never emits.
                 // positionally mapped in bb_module.cpp -- APPEND ONLY, or
                 // every event in already-collected corpora gets renamed.
        STAND_UP // 2026-08-21: standing up left NO trace at all -- resolveStandUp
                 // emitted nothing and action_resolver returned before
                 // resolveMoveStep, so a stand-up was invisible in the logs.
                 // The corpus therefore could not distinguish "nobody stands
                 // up" from "standing up is not logged"; it turned out to be
                 // the former (0.4 % of 280 719 prone player-turns), but only
                 // an added event can keep that honest.
                 // MUST STAY LAST: bb_module.cpp maps this enum to names
                 // positionally, so append only.
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
