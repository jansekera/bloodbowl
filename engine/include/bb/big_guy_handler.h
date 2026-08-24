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
};

BigGuyResult resolveBigGuyCheck(GameState& state, int playerId, ActionType actionType,
                                DiceRollerBase& dice, std::vector<GameEvent>* events);

} // namespace bb
