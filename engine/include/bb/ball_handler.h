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
// Hráč opouští hřiště (KO, zranění, lavička, vyloučení). Napřed zmizí on, pak se míč,
// který držel (nebo který pod ním ležel), odrazí z jeho pole -- viz ball_handler.cpp.
void removeFromPitch(GameState& state, int playerId, PlayerState newState,
                     DiceRollerBase& dice, std::vector<GameEvent>* events);

void resolveThrowIn(GameState& state, Position lastOnPitch, Position offPitchExit,
                    DiceRollerBase& dice, std::vector<GameEvent>* events);
void handleBallOnPlayerDown(GameState& state, int playerId, DiceRollerBase& dice,
                            std::vector<GameEvent>* events);

} // namespace bb
