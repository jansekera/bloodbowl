#pragma once

#include "bb/game_state.h"
#include "bb/macro_actions.h"
#include <cstdint>

namespace bb {

// Cíl tahu a kontrola, jestli makro z plánu ještě sedí na desku. Používá je
// jedna klec (bb/one_cage.h, P126). Celotahový plánovač zvednutí míče
// (item 13, StagedTurnPlanner) tu býval do 02.10.2026 -- nahradila ho fáze 1
// klece (zvednout, co nejdál dopředu, rohy kolem).

// Turn goal, staged by drive phase -- mirrors what simulate()'s
// turnsLeft/idealDist pacing encodes implicitly (macro_mcts.cpp, offensive
// scoringBonus block), made explicit.
enum class TurnGoal : uint8_t {
    NONE = 0,     // opponent holds the ball / not in PLAY -- no offensive plan
    PICKUP_BALL,  // ball loose on the pitch -> recover it
    ADVANCE_BALL, // we hold it, endzone out of reach this turn
    SCORE_BALL,   // we hold it, in scoring range (or last-turns urgency)
};

TurnGoal classifyTurnGoal(const GameState& state);

// Semantic re-validation of a planned macro against the CURRENT state --
// deliberately not "regenerate getAvailableMacros and compare": REPOSITION
// targets are recomputed each generation pass (nearest free ball-adjacent
// square shifts as teammates arrive), so exact-match validation would flag a
// healthy plan as deviated after its own first step. Only the macro types a
// staged plan can contain (REPOSITION, PICKUP) get real checks; anything
// else is conservatively invalid. `requireHeldBall` is set for cage-fill
// stage macros: they additionally require the ball to be held by the acting
// player's own side (fail pickup -> whole stage invalid -> search fallback).
bool stagedMacroStillValid(const GameState& state, const Macro& m,
                           bool requireHeldBall = false);

} // namespace bb
