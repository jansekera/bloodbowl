#include "bb/bomb_handler.h"
#include "bb/helpers.h"
#include "bb/injury.h"
#include "bb/ball_handler.h"
#include <algorithm>

namespace bb {

ActionResult resolveBombThrow(GameState& state, int throwerId, Position target,
                              DiceRollerBase& dice, std::vector<GameEvent>* events) {
    Player& thrower = state.getPlayer(throwerId);

    thrower.hasActed = true;
    state.getTeamState(thrower.teamSide).passUsedThisTurn = true;

    // 1. Accuracy roll (same as pass)
    int dist = thrower.position.distanceTo(target);
    PassRange range = passRangeFromDistance(dist);

    int passTarget = 7 - thrower.stats.agility;
    passTarget -= passModifier(range);

    if (!thrower.hasSkill(SkillName::NervesOfSteel)) {
        passTarget += countTacklezones(state, thrower.position, thrower.teamSide);
    }

    passTarget += countDisturbingPresence(state, thrower.position, thrower.teamSide);

    if (state.weather == Weather::POURING_RAIN || state.weather == Weather::BLIZZARD ||
        state.weather == Weather::VERY_SUNNY) {
        passTarget += 1;
    }

    passTarget = std::clamp(passTarget, 2, 6);

    int roll = dice.rollD6();
    emitEvent(events, {GameEvent::Type::PASS, throwerId, -1, thrower.position, target,
                      roll, roll >= passTarget && roll != 1});

    // Determine explosion position
    Position explosionPos = target;

    if (roll == 1) {
        // Fumble: scatter 1 from thrower
        int d8 = dice.rollD8();
        Position scatter = scatterDirection(d8);
        explosionPos = {static_cast<int8_t>(thrower.position.x + scatter.x),
                        static_cast<int8_t>(thrower.position.y + scatter.y)};
    } else if (roll < passTarget) {
        // Inaccurate: 3x scatter from target
        for (int i = 0; i < 3; i++) {
            int d8 = dice.rollD8();
            Position scatter = scatterDirection(d8);
            explosionPos = {static_cast<int8_t>(explosionPos.x + scatter.x),
                            static_cast<int8_t>(explosionPos.y + scatter.y)};
        }
    }

    // Off-pitch: fizzle, no effect
    if (!explosionPos.isOnPitch()) {
        return ActionResult::ok(); // Never turnover
    }

    // 2. Explosion: all standing players in 3x3 area around explosion
    // Thrower is immune
    for (int dx = -1; dx <= 1; dx++) {
        for (int dy = -1; dy <= 1; dy++) {
            int px = explosionPos.x + dx;
            int py = explosionPos.y + dy;
            if (px < 0 || px > 25 || py < 0 || py > 14) continue;

            Position checkPos{static_cast<int8_t>(px), static_cast<int8_t>(py)};
            Player* victim = state.getPlayerAtPosition(checkPos);
            if (!victim) continue;
            if (victim->id == throwerId) continue; // thrower immune
            if (victim->state != PlayerState::STANDING) continue;

            // Knocked down + armor roll
            victim->state = PlayerState::PRONE;
            emitEvent(events, {GameEvent::Type::KNOCKED_DOWN, victim->id, throwerId,
                              victim->position, {}, 0, false});
            InjuryContext ctx;
            resolveArmourAndInjury(state, victim->id, dice, ctx, events);
            handleBallOnPlayerDown(state, victim->id, dice, events);
        }
    }

    return ActionResult::ok(); // Never turnover
}

} // namespace bb
