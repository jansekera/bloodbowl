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
    bool success = attemptRollLogged(state, playerId, dice, target, SkillName::SureHands, false, true,
                                     events, GameEvent::Type::PICKUP, player.position, {});

    if (success) {
        state.ball = BallState::carried(player.position, playerId);
    } else {
        resolveBounce(state, player.position, dice, 0, events);
    }
    return success;
}

bool resolveCatch(GameState& state, int catcherId, DiceRollerBase& dice,
                  int modifier, std::vector<GameEvent>* events, bool accuratePass) {
    Player& catcher = state.getPlayer(catcherId);

    // P80 (30.09.2026), port PHP 2f930279. r. 857-858: "Prone and Stunned
    // players may never attempt to catch the ball." Ostatni volajici to hlidaji
    // sami; predani ne (prijemce muze mezi nabidkou a provedenim ulehnout).
    //
    // OPRAVENO 09.10.2026 (audit parity 08.10., nález 19) — hlídal se jen stav. Pravidla
    // ř. 7983-7986 (Bone-head; stejně Really Stupid ř. 8401-8405 a Hypnotic Gaze
    // ř. 8185-8188): „The player loses his tackle zones and MAY NOT CATCH, intercept or
    // pass“. Hráč bez zón tedy nechytá odraz, vhazování, přihrávku ani předání — míč se
    // od něj odráží jako od ležícího. Zachycení hlídá pass_handler (kandidát musí mít zóny).
    if (catcher.state != PlayerState::STANDING || catcher.lostTacklezones) {
        emitEvent(events, {GameEvent::Type::CATCH, catcherId, -1, catcher.position, {},
                          0, false});
        return false;
    }

    if (catcher.hasSkill(SkillName::NoHands)) {
        emitEvent(events, {GameEvent::Type::CATCH, catcherId, -1, catcher.position, {},
                          0, false});
        return false;
    }

    int target = calculateCatchTarget(state, catcher, modifier, accuratePass);
    // Týmový přehoz hlídá attemptRoll (jen tým na tahu, ne při výkopu; P64).
    // Catch, l. 7992-7995: "allowed to re-roll the D6 if he fails a catch roll. It also allows
    // the player to re-roll the D6 if he drops a hand-off or fails to make an interception."
    bool success = attemptRollLogged(state, catcherId, dice, target, SkillName::Catch, false, true,
                                     events, GameEvent::Type::CATCH, catcher.position, {});

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

// P223 (20.10.2026) — Diving Catch u míče dopadajícího VEDLE hráče v C++ chyběl úplně (byl jen +1
// k přesné přihrávce, helpers.cpp). Pravidla ř. 8065-8071: "the player can attempt to catch any
// pass, kick off or crowd throw-in, but not bouncing ball, that would land in an EMPTY square in
// one of his tackle zones as if it had landed in his own square WITHOUT LEAVING HIS CURRENT SQUARE.
// A failed catch will bounce from the Diving Catch player's square. If there are two or more
// players attempting to use this skill then they get in each other's way and neither can use it."
// Bez zón "may not catch" (ř. 7983-7986). Smí kterýkoli tým, i sám přihrávající (uživatel 20.10.).
int divingCatcherFor(const GameState& state, Position at) {
    if (state.getPlayerAtPosition(at)) return -1;
    int found = -1;
    int count = 0;
    for (const auto& p : state.players) {
        if (!p.isOnPitch() || p.state != PlayerState::STANDING || p.lostTacklezones) continue;
        if (!p.hasSkill(SkillName::DivingCatch) || p.position.distanceTo(at) != 1) continue;
        found = p.id;
        ++count;
    }
    return count == 1 ? found : -1;
}

void resolveLanding(GameState& state, Position at, DiceRollerBase& dice,
                    std::vector<GameEvent>* events, bool accuratePass) {
    state.ball = BallState::onGround(at);
    const Player* p = state.getPlayerAtPosition(at);
    int catcherId = -1;
    bool diving = false;
    if (p) {
        if (p->state == PlayerState::STANDING && !p->hasSkill(SkillName::NoHands)) catcherId = p->id;
    } else {
        catcherId = divingCatcherFor(state, at);
        diving = catcherId >= 0;
    }
    if (catcherId < 0) {
        resolveBounce(state, at, dice, 0, events);
        return;
    }
    const Position from = state.getPlayer(catcherId).position;
    if (diving) {
        emitEvent(events, {GameEvent::Type::SKILL_USED, catcherId, -1, at, from,
                          static_cast<int>(SkillName::DivingCatch), true});
        state.ball = BallState::onGround(from);
    }
    // Skokem chycená přesná přihrávka má +1 "Catching an accurate pass" (ř. 891), ale ne +1 z Diving
    // Catch — ten je jen "from an accurate pass TARGETED TO HIS SQUARE" (ř. 8063-8064).
    if (resolveCatch(state, catcherId, dice, accuratePass ? 1 : 0, events, accuratePass && !diving)) return;
    resolveBounce(state, from, dice, 0, events);
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
// `templateDie` = hozená D6 (P145: jde do `die1` události vhazování).
Position throwInDirection(ExitEdge edge, DiceRollerBase& dice, int& templateDie) {
    const int d6 = templateDie = dice.rollD6();
    switch (edge) {
        case ExitEdge::TOP: {
            if (d6 <= 2) return {-1, 1};   // SW
            if (d6 <= 4) return {0, 1};    // S (straight back in)
            return {1, 1};                  // SE
        }
        case ExitEdge::BOTTOM: {
            if (d6 <= 2) return {-1, -1};  // NW
            if (d6 <= 4) return {0, -1};   // N
            return {1, -1};                 // NE
        }
        case ExitEdge::LEFT: {
            if (d6 <= 2) return {1, -1};   // NE
            if (d6 <= 4) return {1, 0};    // E
            return {1, 1};                  // SE
        }
        case ExitEdge::RIGHT: {
            if (d6 <= 2) return {-1, -1};  // NW
            if (d6 <= 4) return {-1, 0};   // W
            return {-1, 1};                 // SW
        }
        case ExitEdge::TOP_LEFT: {
            int d3 = (d6 + 1) / 2;
            if (d3 == 1) return {1, 0};    // E, along the top edge
            if (d3 == 2) return {1, 1};    // SE, pure diagonal
            return {0, 1};                   // S, along the left edge
        }
        case ExitEdge::TOP_RIGHT: {
            int d3 = (d6 + 1) / 2;
            if (d3 == 1) return {-1, 0};   // W, along the top edge
            if (d3 == 2) return {-1, 1};   // SW, pure diagonal
            return {0, 1};                   // S, along the right edge
        }
        case ExitEdge::BOTTOM_LEFT: {
            int d3 = (d6 + 1) / 2;
            if (d3 == 1) return {1, 0};    // E, along the bottom edge
            if (d3 == 2) return {1, -1};   // NE, pure diagonal
            return {0, -1};                  // N, along the left edge
        }
        default: {  // BOTTOM_RIGHT
            int d3 = (d6 + 1) / 2;
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
        int templateDie = 0;
        Position offset = throwInDirection(edge, dice, templateDie);
        int distance = dice.roll2D6();

        // Mic se posouva po JEDNOM poli, aby se dalo najit "the LAST SQUARE it
        // was IN before it left the pitch" (l. 875-877) -- skok rovnou na
        // cilove pole tuhle informaci zahodi a druhe vhazeni by se pak pocitalo
        // z puvodniho mista, ne z toho, kde mic hriste opustil.
        Position dest = origin;
        Position lastInside = origin;
        Position firstOutside = origin;
        bool leftPitch = false;
        // OPRAVENO 13.10.2026 (T5.24 / P145) -- tady bylo `step < distance`, mic
        // letel o pole dal. FAQ l. 9307-9314: "The square with the Blood Bowl
        // logo over it counts as the FIRST square of the ball's movement. So, if
        // the result of the 2D6 roll was a 2, the ball would be ... placed in the
        // square with the Blood Bowl logo over it, then move ONE square".
        for (int step = 0; step < distance - 1; ++step) {
            dest.x = static_cast<int8_t>(dest.x + offset.x);
            dest.y = static_cast<int8_t>(dest.y + offset.y);
            if (!dest.isOnPitch()) {
                firstOutside = dest;
                leftPitch = true;
                break;
            }
            lastInside = dest;
        }

        // P145: `die1` = kostka šablony -- podle ní se vhazování (2D6 v `roll`)
        // pozná od odrazu (D8 v `roll`, `die1` = 0).
        emitEvent(events, {GameEvent::Type::BALL_BOUNCE, -1, -1, origin, dest,
                          distance, !leftPitch, templateDie});

        if (leftPitch) {
            // l. 875-877: letel ven znovu => vhazuje se ZNOVU, ale z POSLEDNIHO
            // POLE V HRISTI, ne z puvodniho mista.
            origin = lastInside;
            exitAt = firstOutside;
            continue;
        }

        // l. 871-874: stojici hráč chytat MUSI; prazdne pole, nebo lezici/omraceny => odraz
        // (P223: na prazdne pole smi skocit hrac s Diving Catch). l. 878: "Throw-ins cannot be
        // intercepted" -- chytani vhozeneho mice neni intercept a plati pro obe strany stejne.
        resolveLanding(state, dest, dice, events);
        return;
    }

    // Strop: mic polozime na posledni pole v hristi a odrazime.
    resolveBounce(state, origin, dice, 0, events);
}

// JEDNO místo pro pořadí „hráč pryč -> míč se odráží“.
// 09.10.2026 (T5.26 a týž princip u KO / zranění): pozice se nastavila na (−1,−1) dřív,
// než se míč pustil, takže se odrážel z pole mimo hřiště.
// OPRAVENO 13.10.2026 (P192 review, nález 1) -- po opravě z 09.10. se míč pouštěl, dokud
// hráč na poli ještě byl: odraz zpět ho „viděl“ (ležící = další odraz; stojící vyloučený
// nebo upír míč chytil a odnesl ze hřiště). Pravidla ř. 678-681: míč se odráží „AFTER the
// player's armour and injury rolls (if any) are FULLY RESOLVED“ -- hráč už je v tu chvíli
// pryč; ř. 1882-1884: „bounces from the square he was standing in when sent off“.
void removeFromPitch(GameState& state, int playerId, PlayerState newState,
                     DiceRollerBase& dice, std::vector<GameEvent>* events) {
    Player& player = state.getPlayer(playerId);
    const Position pos = player.position;
    const bool carried = state.ball.isHeld && state.ball.carrierId == playerId;
    const bool lyingOnIt = !state.ball.isHeld && pos.isOnPitch() && state.ball.position == pos;
    player.setState(newState);
    player.position = {-1, -1};
    if (carried) state.ball = BallState::onGround(pos);
    if (carried || lyingOnIt) resolveBounce(state, pos, dice, 0, events);
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
