#pragma once

#include "bb/game_state.h"
#include "bb/dice.h"
#include "bb/game_event.h"
#include <vector>

namespace bb {

// JEDINÁ výkopová funkce enginu (od 02.10.2026; do té doby vedle ní žil `simpleKickoff`, na kterém
// běžely všechny hry, a tahle byla mrtvá cesta — viz evidence/kickoff_sjednoceni_20261002.md).
// Pořadí BB2016 ř. 1242-1248: kop, rozptyl, [Kick-Off Return ř. 8249-8256], výkopová tabulka,
// dopad (odraz / chycení / touchback). Po výsledku Blitz! skončí s kopajícím týmem na tahu a míčem
// ve vzduchu (`GameState::kickoffBallInAir`); dopad pak dodělá `resolveKickoffLanding`.
void resolveKickoff(GameState& state, DiceRollerBase& dice, std::vector<GameEvent>* events);

// Koná se výkop? Ř. 1033-1035: „Play stops when both coaches have had eight turns each“ ⇒ přijímající
// (soupeř `state.kickingTeam`) musí mít ještě kolo. Volat PŘED `setupDrive`: návrat z KO a Sweltering
// Heat se hází jen před výkopem, který se koná (ř. 1007-1012).
bool receivingTeamHasATurnLeft(const GameState& state);

// Dopad míče po bonusovém kole Blitz!. Volá `executeAction`, jakmile kolo skončí (END_TURN nebo
// turnover); předtím `resolveEndTurn` předal tah přijímajícím.
void resolveKickoffLanding(GameState& state, DiceRollerBase& dice, std::vector<GameEvent>* events);

} // namespace bb
