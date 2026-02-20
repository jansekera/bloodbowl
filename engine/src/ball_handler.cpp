#include "bb/ball_handler.h"
#include "bb/helpers.h"

namespace bb {

bool resolvePickup(GameState& state, int playerId, DiceRollerBase& dice,
                   std::vector<GameEvent>* events) {
    Player& player = state.getPlayer(playerId);

    if (player.hasSkill(SkillName::NoHands)) {
        emitEvent(events, {GameEvent::Type::PICKUP, playerId, -1, player.position, {},
                          0, false});
        return false;
    }

    int target = calculatePickupTarget(state, player);
    bool success = attemptRoll(state, playerId, dice, target,
                               SkillName::SureHands, false, true, events);

    emitEvent(events, {GameEvent::Type::PICKUP, playerId, -1, player.position, {},
                      target, success});

    if (success) {
        state.ball = BallState::carried(player.position, playerId);
    }
    return success;
}

bool resolveCatch(GameState& state, int catcherId, DiceRollerBase& dice,
                  int modifier, std::vector<GameEvent>* events) {
    Player& catcher = state.getPlayer(catcherId);

    if (catcher.hasSkill(SkillName::NoHands)) {
        emitEvent(events, {GameEvent::Type::CATCH, catcherId, -1, catcher.position, {},
                          0, false});
        return false;
    }

    int target = calculateCatchTarget(state, catcher, modifier);
    bool success = attemptRoll(state, catcherId, dice, target,
                               SkillName::Catch, false, true, events);

    emitEvent(events, {GameEvent::Type::CATCH, catcherId, -1, catcher.position, {},
                      target, success});

    if (success) {
        state.ball = BallState::carried(catcher.position, catcherId);
    }
    return success;
}

void resolveBounce(GameState& state, Position from, DiceRollerBase& dice,
                   int depth, std::vector<GameEvent>* events) {
    if (depth > 5) {
        // Ball stays on ground at from
        state.ball = BallState::onGround(from);
        return;
    }

    int d8 = dice.rollD8();
    Position offset = scatterDirection(d8);
    Position dest{static_cast<int8_t>(from.x + offset.x),
                  static_cast<int8_t>(from.y + offset.y)};

    emitEvent(events, {GameEvent::Type::BALL_BOUNCE, -1, -1, from, dest, d8, true});

    if (!dest.isOnPitch()) {
        // Ball goes off pitch — throw-in from last on-pitch position
        resolveThrowIn(state, from, dice, events);
        return;
    }

    // Check if a standing player is at dest
    const Player* p = state.getPlayerAtPosition(dest);
    if (p && canAct(p->state)) {
        // Attempt catch (no modifier for bounced ball)
        state.ball = BallState::onGround(dest);
        bool caught = resolveCatch(state, p->id, dice, 0, events);
        if (!caught) {
            resolveBounce(state, dest, dice, depth + 1, events);
        }
    } else {
        state.ball = BallState::onGround(dest);
    }
}

void resolveThrowIn(GameState& state, Position lastOnPitch, DiceRollerBase& dice,
                    std::vector<GameEvent>* events) {
    int d8 = dice.rollD8();
    int distance = dice.roll2D6();
    Position offset = scatterDirection(d8);

    Position dest{
        static_cast<int8_t>(lastOnPitch.x + offset.x * distance),
        static_cast<int8_t>(lastOnPitch.y + offset.y * distance)
    };

    // Clamp to pitch if off-pitch
    if (!dest.isOnPitch()) {
        dest.x = std::clamp(dest.x, static_cast<int8_t>(0),
                            static_cast<int8_t>(Position::PITCH_WIDTH - 1));
        dest.y = std::clamp(dest.y, static_cast<int8_t>(0),
                            static_cast<int8_t>(Position::PITCH_HEIGHT - 1));
    }

    emitEvent(events, {GameEvent::Type::BALL_BOUNCE, -1, -1, lastOnPitch, dest,
                      distance, true});

    // Check if a standing player is at dest
    const Player* p = state.getPlayerAtPosition(dest);
    if (p && canAct(p->state)) {
        state.ball = BallState::onGround(dest);
        bool caught = resolveCatch(state, p->id, dice, 0, events);
        if (!caught) {
            resolveBounce(state, dest, dice, 0, events);
        }
    } else {
        state.ball = BallState::onGround(dest);
    }
}

void handleBallOnPlayerDown(GameState& state, int playerId, DiceRollerBase& dice,
                            std::vector<GameEvent>* events) {
    if (!state.ball.isHeld || state.ball.carrierId != playerId) return;

    // Ball drops at player's position and bounces
    Position pos = state.getPlayer(playerId).position;
    state.ball = BallState::onGround(pos);
    resolveBounce(state, pos, dice, 0, events);
}

} // namespace bb
