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
    // deklarovanou akci pro tohle kolo. Nese PRAVIDLO, ne dovednost, protoze
    // pravidla nemluvi stejne:
    //   Bone-head (r. 7980-7983) i Really Stupid (r. 8398-8401): "the player's
    //     team loses the declared Action for the turn (...the team cannot
    //     declare another Blitz Action that turn)"          => true
    //   Wild Animal (r. 8668-8669): "the Action is wasted."  => true
    //   Take Root (r. 8580-8583) mluvi JEN o bloku a o tymove akci nerika
    //     nic                                               => false
    bool wastesTeamAction = false;
};

BigGuyResult resolveBigGuyCheck(GameState& state, int playerId, ActionType actionType,
                                DiceRollerBase& dice, std::vector<GameEvent>* events);

} // namespace bb
