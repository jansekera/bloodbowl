#pragma once

#include "bb/game_state.h"
#include "bb/dice.h"
#include "bb/game_event.h"
#include "bb/action_result.h"
#include "bb/rules_engine.h"
#include <vector>

namespace bb {

// M13 diagnostika (01.09.2026): akce zacate VLEZE a jejich osud.
// acts = kolik jich bylo (mimo vstani na miste) · turnovers = z toho ztrata
// mice/kola · standFail = neuspesne vstani (r. 693: NENI turnover)
// noBlock = blitz z lehu, ktery ranu nakonec nehodil
long takeProneActsInSearch();
long takeProneTurnoversInSearch();
long takeProneStandFailInSearch();
long takeProneNoBlockInSearch();
long takeProneBlitzesInSearch();
// Reference pro podil turnoveru: tytez akce od STOJICICH hracu.
long takeStandActsInSearch();
long takeStandTurnoversInSearch();
long takeStandBlitzesInSearch();
long takeStandNoBlockInSearch();
// ⭐ Poctive srovnani ceny: turnover BLITZU z lehu proti turnoveru BLITZU ze
//   stoje. Podil "vsech akci" byl zavadejici -- stojici pohyb je N akci MOVE,
//   akce z lehu je jedna akce obsahujici cely blitz.
long takeProneBlitzTOInSearch();
long takeStandBlitzTOInSearch();
// Rozpad vyhozeneho blitzu podle priciny: [nedosah, pohyb, turnover, srazen,
// stoji, daleko]. Nez se zacne opravovat 9,3 % vyhozenych blitzu, musi se
// vedet PROC -- hadani uz dvakrat neslo.
void takeBlitzWastedBreakdown(long* out6);

ActionResult resolveAction(GameState& state, const Action& action,
                           DiceRollerBase& dice, std::vector<GameEvent>* events);

// resolveAction + auto-end-turn on turnover + check touchdown/half
ActionResult executeAction(GameState& state, const Action& action,
                           DiceRollerBase& dice, std::vector<GameEvent>* events);

} // namespace bb
