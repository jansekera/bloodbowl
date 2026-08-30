#pragma once

#include "bb/game_state.h"
#include "bb/dice.h"
#include "bb/game_event.h"
#include "bb/action_result.h"
#include <vector>

namespace bb {

// Měřidlo exkluzivity faulu (30.08.2026): kolik ležících těl bylo na výběr,
// když se faulovalo. „Na výběr" = ležící soupeř sousedící s naším stojícím
// hráčem, který ještě může jednat -- tedy VOLBA, ne jen dosažitelnost.
long takeFoulsSeenInSearch();
long takeFoulAlternativesInSearch();
long takeFoulsWithChoiceInSearch();

ActionResult resolveFoul(GameState& state, int foulerId, int targetId,
                         DiceRollerBase& dice, std::vector<GameEvent>* events);

} // namespace bb
