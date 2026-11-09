#pragma once

#include "bb/game_state.h"
#include "bb/dice.h"
#include "bb/game_event.h"
#include "bb/action_result.h"
#include <vector>

namespace bb {

ActionResult resolvePass(GameState& state, int passerId, Position target,
                         DiceRollerBase& dice, std::vector<GameEvent>* events);

ActionResult resolveHandOff(GameState& state, int giverId, int receiverId,
                            DiceRollerBase& dice, std::vector<GameEvent>* events);

// Zachycení hodu (ř. 1761-1785) — sdílí přihrávka a bomba (P264, 9.11.2026).
// Cíl hodu na zachycení pro hráče `p` (7 − AG + 2 a modifikátory, ořez 2..6).
int interceptionTarget(const GameState& state, const Player& p);
// Nejpravděpodobnější způsobilý soupeř házeče v koridoru pravítka (jen jeden smí zkusit); -1 = nikdo.
int pickInterceptor(const GameState& state, const Player& passer, Position target);
// Hod na zachycení (Catch přehodí neúspěch, Safe Throw házeče může úspěch zrušit). Stav míče nemění.
bool rollInterception(GameState& state, const Player& passer, int interceptorId, DiceRollerBase& dice,
                      std::vector<GameEvent>* events);

} // namespace bb
