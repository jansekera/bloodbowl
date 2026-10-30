#pragma once

#include "bb/game_state.h"
#include "bb/dice.h"
#include "bb/game_event.h"
#include <vector>

namespace bb {

bool resolvePickup(GameState& state, int playerId, DiceRollerBase& dice,
                   std::vector<GameEvent>* events);
// `accuratePass` = chytá se PŘESNÁ PŘIHRÁVKA mířená na hráčovo pole (jen tehdy platí +1 z Diving Catch).
bool resolveCatch(GameState& state, int catcherId, DiceRollerBase& dice,
                  int modifier, std::vector<GameEvent>* events, bool accuratePass = false);
void resolveBounce(GameState& state, Position from, DiceRollerBase& dice,
                   int depth, std::vector<GameEvent>* events);
// P223: hráč s Diving Catch, který smí chytit míč dopadající ze vzduchu na PRÁZDNÉ pole `at`
// (stojí, má tackle zóny, sousedí; jediný takový — dva a víc se ruší, ř. 8065-8071); -1 = nikdo.
int divingCatcherFor(const GameState& state, Position at);
// Uživatel 30.10.2026: skočí hráč `diver` pro míč dopadající na `at`? Skill je volba (ř. 8065 "can
// attempt"): v živé partii odpoví kouč hráče (USE_DIVING_CATCH, i v tahu soupeře), jinak `aiJumps`.
bool divingCatchChosen(const GameState& state, int diver, Position at, bool aiJumps);
// P223: JEDINÉ místo „míč dopadá ze vzduchu na pole `at` na hřišti“ — přihrávka (přesná
// i nepřesná), Hail Mary, vhazování (výkop má vlastní `landBall` kvůli touchbacku, hráče s Diving
// Catch bere z `divingCatcherFor`). Ne odraz. Stojící hráč chytá; na prázdné pole hráč s Diving
// Catch ze SVÉHO pole; neúspěch i prázdné pole ⇒ odraz. `accuratePass` = přesná přihrávka mířená sem.
// `depth` = hloubka odrazů (pojistka v `resolveBounce`) — u vhazování ji nese řetěz dál.
void resolveLanding(GameState& state, Position at, DiceRollerBase& dice,
                    std::vector<GameEvent>* events, bool accuratePass = false, int depth = 0);
// Hráč opouští hřiště (KO, zranění, lavička, vyloučení). Napřed zmizí on, pak se míč,
// který držel (nebo který pod ním ležel), odrazí z jeho pole -- viz ball_handler.cpp.
void removeFromPitch(GameState& state, int playerId, PlayerState newState,
                     DiceRollerBase& dice, std::vector<GameEvent>* events);

void resolveThrowIn(GameState& state, Position lastOnPitch, Position offPitchExit,
                    DiceRollerBase& dice, std::vector<GameEvent>* events, int depth = 0);
void handleBallOnPlayerDown(GameState& state, int playerId, DiceRollerBase& dice,
                            std::vector<GameEvent>* events);

} // namespace bb
