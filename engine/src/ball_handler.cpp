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
        resolveThrowIn(state, from, dest, dice, events);
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

namespace {

enum class ExitEdge { LEFT, RIGHT, TOP, BOTTOM,
                      TOP_LEFT, TOP_RIGHT, BOTTOM_LEFT, BOTTOM_RIGHT };

ExitEdge classifyExit(Position offPitchExit) {
    bool left = offPitchExit.x < 0;
    bool right = offPitchExit.x >= Position::PITCH_WIDTH;
    bool top = offPitchExit.y < 0;
    bool bottom = offPitchExit.y >= Position::PITCH_HEIGHT;
    if (top && left) return ExitEdge::TOP_LEFT;
    if (top && right) return ExitEdge::TOP_RIGHT;
    if (bottom && left) return ExitEdge::BOTTOM_LEFT;
    if (bottom && right) return ExitEdge::BOTTOM_RIGHT;
    if (top) return ExitEdge::TOP;
    if (bottom) return ExitEdge::BOTTOM;
    if (left) return ExitEdge::LEFT;
    return ExitEdge::RIGHT;
}

// LRB6 throw-in template: for a side exit, a D6 picks one of 3 directions --
// 1-2 diagonal one way, 3-4 straight back onto the pitch, 5-6 diagonal the
// other way (verified against LRB6/CRP reference material, distinct from the
// uniform 8-way Bounce scatter template). For a corner exit, a D3 picks one
// of 3 directions -- straight along one edge, the pure diagonal into the
// corner, or straight along the other edge.
Position throwInDirection(ExitEdge edge, DiceRollerBase& dice) {
    switch (edge) {
        case ExitEdge::TOP: {
            int d6 = dice.rollD6();
            if (d6 <= 2) return {-1, 1};   // SW
            if (d6 <= 4) return {0, 1};    // S (straight back in)
            return {1, 1};                  // SE
        }
        case ExitEdge::BOTTOM: {
            int d6 = dice.rollD6();
            if (d6 <= 2) return {-1, -1};  // NW
            if (d6 <= 4) return {0, -1};   // N
            return {1, -1};                 // NE
        }
        case ExitEdge::LEFT: {
            int d6 = dice.rollD6();
            if (d6 <= 2) return {1, -1};   // NE
            if (d6 <= 4) return {1, 0};    // E
            return {1, 1};                  // SE
        }
        case ExitEdge::RIGHT: {
            int d6 = dice.rollD6();
            if (d6 <= 2) return {-1, -1};  // NW
            if (d6 <= 4) return {-1, 0};   // W
            return {-1, 1};                 // SW
        }
        case ExitEdge::TOP_LEFT: {
            int d3 = (dice.rollD6() + 1) / 2;
            if (d3 == 1) return {1, 0};    // E, along the top edge
            if (d3 == 2) return {1, 1};    // SE, pure diagonal
            return {0, 1};                   // S, along the left edge
        }
        case ExitEdge::TOP_RIGHT: {
            int d3 = (dice.rollD6() + 1) / 2;
            if (d3 == 1) return {-1, 0};   // W, along the top edge
            if (d3 == 2) return {-1, 1};   // SW, pure diagonal
            return {0, 1};                   // S, along the right edge
        }
        case ExitEdge::BOTTOM_LEFT: {
            int d3 = (dice.rollD6() + 1) / 2;
            if (d3 == 1) return {1, 0};    // E, along the bottom edge
            if (d3 == 2) return {1, -1};   // NE, pure diagonal
            return {0, -1};                  // N, along the left edge
        }
        default: {  // BOTTOM_RIGHT
            int d3 = (dice.rollD6() + 1) / 2;
            if (d3 == 1) return {-1, 0};   // W, along the bottom edge
            if (d3 == 2) return {-1, -1};  // NW, pure diagonal
            return {0, -1};                  // N, along the right edge
        }
    }
}

} // namespace

void resolveThrowIn(GameState& state, Position lastOnPitch, Position offPitchExit,
                    DiceRollerBase& dice, std::vector<GameEvent>* events) {
    ExitEdge edge = classifyExit(offPitchExit);
    Position offset = throwInDirection(edge, dice);
    int distance = dice.roll2D6();

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

    // A throw-in always ends with one standard bounce from the landing
    // square, regardless of whether that square is occupied.
    resolveBounce(state, dest, dice, 0, events);
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
