#include "bb/ttm_handler.h"
#include "bb/helpers.h"
#include "bb/injury.h"
#include "bb/ball_handler.h"
#include <algorithm>

namespace bb {

ActionResult resolveThrowTeamMate(GameState& state, int throwerId, int projectileId,
                                  Position target, DiceRollerBase& dice,
                                  std::vector<GameEvent>* events) {
    Player& thrower = state.getPlayer(throwerId);
    Player& projectile = state.getPlayer(projectileId);

    thrower.hasActed = true;
    state.getTeamState(thrower.teamSide).passUsedThisTurn = true;

    // 1. AlwaysHungry check
    if (thrower.hasSkill(SkillName::AlwaysHungry)) {
        int hungryRoll = dice.rollD6();
        if (hungryRoll == 1) {
            // Try team reroll
            bool rerolled = false;
            TeamState& team = state.getTeamState(thrower.teamSide);
            if (team.canUseReroll()) {
                team.rerolls--;
                team.rerollUsedThisTurn = true;

                bool canReroll = true;
                if (thrower.hasSkill(SkillName::Loner)) {
                    int lonerRoll = dice.rollD6();
                    if (lonerRoll < 4) canReroll = false;
                }

                if (canReroll) {
                    hungryRoll = dice.rollD6();
                    if (hungryRoll != 1) rerolled = true;
                }
            }

            if (!rerolled && hungryRoll == 1) {
                // Eaten! Projectile injured, removed from pitch
                emitEvent(events, {GameEvent::Type::SKILL_USED, throwerId, projectileId,
                                  thrower.position, {}, static_cast<int>(SkillName::AlwaysHungry), false});

                // Drop ball if projectile carried it
                handleBallOnPlayerDown(state, projectileId, dice, events);

                projectile.state = PlayerState::INJURED;
                projectile.position = {-1, -1};
                return ActionResult::ok(); // NOT turnover
            }
        }
    }

    // 2. Accuracy roll
    int dist = thrower.position.distanceTo(target);
    PassRange range = passRangeFromDistance(dist);

    if (thrower.hasSkill(SkillName::StrongArm) && range != PassRange::QUICK_PASS) {
        range = static_cast<PassRange>(static_cast<int>(range) - 1);
    }

    int passTarget = 7 - thrower.stats.agility;
    passTarget -= passModifier(range);

    if (!thrower.hasSkill(SkillName::NervesOfSteel)) {
        passTarget += countTacklezones(state, thrower.position, thrower.teamSide);
    }

    passTarget = std::clamp(passTarget, 2, 6);

    int roll = dice.rollD6();
    emitEvent(events, {GameEvent::Type::PASS, throwerId, projectileId, thrower.position,
                      target, roll, roll >= passTarget && roll != 1});

    // Determine landing position
    Position landPos = target;
    bool fumble = (roll == 1);
    bool accurate = (!fumble && roll >= passTarget);

    if (fumble) {
        // Scatter 1 from thrower
        int d8 = dice.rollD8();
        Position scatter = scatterDirection(d8);
        landPos = {static_cast<int8_t>(thrower.position.x + scatter.x),
                   static_cast<int8_t>(thrower.position.y + scatter.y)};
    } else if (!accurate) {
        // Inaccurate: scatter 1 from target
        int d8 = dice.rollD8();
        Position scatter = scatterDirection(d8);
        landPos = {static_cast<int8_t>(target.x + scatter.x),
                   static_cast<int8_t>(target.y + scatter.y)};
    }

    // 3. Landing
    // Off-pitch: crowd surf + injury + turnover
    if (!landPos.isOnPitch()) {
        handleBallOnPlayerDown(state, projectileId, dice, events);
        projectile.position = {-1, -1};
        resolveCrowdSurf(state, projectileId, dice, events);
        return ActionResult::turnovr();
    }

    // Occupied: scatter until empty or off-pitch
    while (state.getPlayerAtPosition(landPos) != nullptr) {
        int d8 = dice.rollD8();
        Position scatter = scatterDirection(d8);
        landPos = {static_cast<int8_t>(landPos.x + scatter.x),
                   static_cast<int8_t>(landPos.y + scatter.y)};
        if (!landPos.isOnPitch()) {
            handleBallOnPlayerDown(state, projectileId, dice, events);
            projectile.position = {-1, -1};
            resolveCrowdSurf(state, projectileId, dice, events);
            return ActionResult::turnovr();
        }
    }

    // Move projectile to landing position
    projectile.position = landPos;
    if (state.ball.isHeld && state.ball.carrierId == projectileId) {
        state.ball.position = landPos;
    }

    // Landing roll
    int landTarget = 7 - projectile.stats.agility;
    int tz = countTacklezones(state, landPos, projectile.teamSide);
    landTarget += tz;
    landTarget = std::clamp(landTarget, 2, 6);

    int landRoll = dice.rollD6();
    if (landRoll >= landTarget) {
        // Landed successfully — standing
        emitEvent(events, {GameEvent::Type::SKILL_USED, projectileId, -1, landPos, {},
                          static_cast<int>(SkillName::RightStuff), true});
        return ActionResult::ok();
    }

    // Failed landing: prone + armor roll
    projectile.state = PlayerState::PRONE;
    emitEvent(events, {GameEvent::Type::KNOCKED_DOWN, projectileId, -1, landPos, {}, 0, false});
    InjuryContext ctx;
    resolveArmourAndInjury(state, projectileId, dice, ctx, events);
    handleBallOnPlayerDown(state, projectileId, dice, events);

    return ActionResult::turnovr();
}

} // namespace bb
