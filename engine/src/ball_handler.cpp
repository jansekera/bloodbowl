#include "bb/ball_handler.h"
#include "bb/helpers.h"

namespace bb {

bool resolvePickup(GameState& state, int playerId, DiceRollerBase& dice,
                   std::vector<GameEvent>* events) {
    Player& player = state.getPlayer(playerId);

    if (player.hasSkill(SkillName::NoHands)) {
        emitEvent(events, {GameEvent::Type::PICKUP, playerId, -1, player.position, {},
                          0, false});
        resolveBounce(state, player.position, dice, 0, events);
        return false;
    }

    int target = calculatePickupTarget(state, player);
    bool success = attemptRoll(state, playerId, dice, target,
                               SkillName::SureHands, false, true, events);

    emitEvent(events, {GameEvent::Type::PICKUP, playerId, -1, player.position, {},
                      target, success});

    if (success) {
        state.ball = BallState::carried(player.position, playerId);
    } else {
        resolveBounce(state, player.position, dice, 0, events);
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
    if (depth > 200) {
        // Pathological chain (e.g. a dense ring of prone/failed-catch
        // players) -- this is purely a recursion-safety valve, not a
        // realistic gameplay limit; real dice make a chain anywhere near
        // this long vanishingly unlikely.
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

    // Check if a player is at dest
    const Player* p = state.getPlayerAtPosition(dest);
    if (p) {
        state.ball = BallState::onGround(dest);
        if (canAct(p->state)) {
            // Attempt catch (no modifier for bounced ball)
            bool caught = resolveCatch(state, p->id, dice, 0, events);
            if (!caught) {
                resolveBounce(state, dest, dice, depth + 1, events);
            }
        } else {
            // Prone/stunned players can't attempt a catch -- automatic
            // fail, ball keeps bouncing rather than resting on them.
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

// F9 (24.08.2026) -- BB2016 l. 868-877. Dve vady v jedne funkci:
//
// (1) "If the ball is thrown into a square occupied by a STANDING PLAYER, that
//     player MUST ATTEMPT TO CATCH the ball. If the ball lands in an empty
//     square or a square occupied by a Prone or Stunned player, then it will
//     bounce." -- u nas se odrazelo VZDY ("regardless of whether that square is
//     occupied"), takze vhozeny mic nikdo nikdy nechytil.
// (2) "If a throw-in results in the ball going off the pitch AGAIN, it will be
//     THROWN IN AGAIN, centred on the last square it was in before it left the
//     pitch." -- u nas se misto toho oriznul na kraj hriste (`clamp`), cimz
//     vzniklo pole, kam se mic podle pravidel nikdy nedostane.
//
// Opakovane vhazovani se pocita ve smycce s tvrdym stropem: hriste je 26x15,
// takze 2D6 nemuze cyklit donekonecna, ale strop chrani proti degenerovanemu
// stavu (mic mimo, zadny hrac). Po strope se mic polozi na posledni pole
// v hristi a odrazi se -- to uz je nase volba, ne pravidlo.
void resolveThrowIn(GameState& state, Position lastOnPitch, Position offPitchExit,
                    DiceRollerBase& dice, std::vector<GameEvent>* events) {
    Position origin = lastOnPitch;
    Position exitAt = offPitchExit;

    for (int attempt = 0; attempt < 8; ++attempt) {
        ExitEdge edge = classifyExit(exitAt);
        Position offset = throwInDirection(edge, dice);
        int distance = dice.roll2D6();

        // Mic se posouva po JEDNOM poli, aby se dalo najit "the LAST SQUARE it
        // was IN before it left the pitch" (l. 875-877) -- skok rovnou na
        // cilove pole tuhle informaci zahodi a druhe vhazeni by se pak pocitalo
        // z puvodniho mista, ne z toho, kde mic hriste opustil.
        Position dest = origin;
        Position lastInside = origin;
        Position firstOutside = origin;
        bool leftPitch = false;
        for (int step = 0; step < distance; ++step) {
            dest.x = static_cast<int8_t>(dest.x + offset.x);
            dest.y = static_cast<int8_t>(dest.y + offset.y);
            if (!dest.isOnPitch()) {
                firstOutside = dest;
                leftPitch = true;
                break;
            }
            lastInside = dest;
        }

        emitEvent(events, {GameEvent::Type::BALL_BOUNCE, -1, -1, origin, dest,
                          distance, !leftPitch});

        if (leftPitch) {
            // l. 875-877: letel ven znovu => vhazuje se ZNOVU, ale z POSLEDNIHO
            // POLE V HRISTI, ne z puvodniho mista.
            origin = lastInside;
            exitAt = firstOutside;
            continue;
        }

        Player* catcher = state.getPlayerAtPosition(dest);
        if (catcher && catcher->state == PlayerState::STANDING &&
            !catcher->hasSkill(SkillName::NoHands)) {
            // l. 871-872: stojici hráč chytat MUSI (neni to volba).
            // l. 878: "Throw-ins cannot be intercepted" -- chytani vhozeneho
            // mice tedy neni intercept a plati pro obe strany stejne.
            state.ball = BallState::onGround(dest);
            if (resolveCatch(state, catcher->id, dice, 0, events)) return;
            // nechytil => odrazi se z jeho pole
            resolveBounce(state, dest, dice, 0, events);
            return;
        }

        // l. 872-874: prazdne pole, nebo lezici/omraceny => odraz
        resolveBounce(state, dest, dice, 0, events);
        return;
    }

    // Strop: mic polozime na posledni pole v hristi a odrazime.
    resolveBounce(state, origin, dice, 0, events);
}

void handleBallOnPlayerDown(GameState& state, int playerId, DiceRollerBase& dice,
                            std::vector<GameEvent>* events) {
    Position pos = state.getPlayer(playerId).position;

    if (state.ball.isHeld && state.ball.carrierId == playerId) {
        // Ball drops at player's position and bounces
        state.ball = BallState::onGround(pos);
        resolveBounce(state, pos, dice, 0, events);
        return;
    }

    if (!state.ball.isHeld && pos.isOnPitch() && state.ball.position == pos) {
        // Player fell onto a loose ball -- it scatters from under them
        resolveBounce(state, pos, dice, 0, events);
    }
}

} // namespace bb
