#pragma once

#include "bb/game_state.h"
#include "bb/dice.h"
#include "bb/game_event.h"
#include "bb/enums.h"
#include <vector>

namespace bb {

struct BigGuyResult {
    bool actionBlocked = false;  // true = action cannot proceed
    bool proceed = true;         // for Bloodlust bite: action still proceeds
    // TA10 (24.08.2026): Blood Lust umi zpusobit TURNOVER -- kdyz upir nema
    // koho kousnout (l. 7942-7943), nebo kdyz kousnuty Thrall drzel mic
    // (l. 7941-7942). Driv se turnover z teto cesty nedal vratit vubec.
    bool turnover = false;
    // M2/N13 = P55 (29.08.2026): rika, jestli propadla akce bere TYMU jeho
    // deklarovanou akci pro tohle kolo.
    //
    // Odpoved je ANO pro vsechny ctyri dovednosti, a rozhoduje o tom jedno
    // misto -- l. 351-352 u Blitze, l. 357-358 u Passe, l. 360-362 u Hand-off
    // a Foulu: "IMPORTANT: This Action may NOT BE DECLARED by more than one
    // player per turn." Limit visi na DEKLARACI, ne na dokonceni, a vsechny
    // big-guy hody se hazi "immediately after declaring an Action".
    //
    // Vety u jednotlivych dovednosti tedy nic nezavadeji, jen to znovu rikaji,
    // aby se "the player can't do anything for the turn" nedalo cist jako
    // "akce se nestala":
    //   Bone-head    l. 7980-7983  "the player's team loses the declared
    //                              Action for the turn"
    //   Really Stupid l. 8398-8401 tataz veta
    //   Wild Animal  l. 8668-8669  "the Action is wasted"
    //   Take Root    l. 8580-8583  mluvi jen o zakazu bloku -- ale mlceni
    //                              o tymove akci NEODVOLAVA l. 351-352
    //
    // Priznak zustava, aby to rozhodnuti bylo videt na jednom miste a dalo se
    // zmenit, kdyby se nasla dovednost, u ktere se akce nedeklaruje.
    bool wastesTeamAction = false;
};

// `standUpInPlace` = deklarovana akce je pouhe VSTANI na vlastnim poli.
// BB2016 r. 8583-8584 konci vetou "(he can still roll to stand up if he is
// Prone)" -- zakorenení tedy vstavani NEBRANI, i kdyz kazdou jinou MOVE akci
// zabije. Bez tohohle rozliseni se muselo vybirat mezi dvema chybami: bud
// se hod pri vstavani NEHAZEL (a Treeman, ktery mel zakorenit, priste zase
// chodil -- N14), nebo se hazel a zakoreneny uz nikdy nevstal.
BigGuyResult resolveBigGuyCheck(GameState& state, int playerId, ActionType actionType,
                                DiceRollerBase& dice, std::vector<GameEvent>* events,
                                bool standUpInPlace = false);

} // namespace bb
