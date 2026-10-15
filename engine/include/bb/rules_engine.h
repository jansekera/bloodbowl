#pragma once

#include "bb/game_state.h"
#include "bb/enums.h"
#include "bb/position.h"
#include <vector>

namespace bb {

struct Action {
    ActionType type = ActionType::END_TURN;
    int playerId = -1;
    int targetId = -1;
    Position target{-1, -1};
};

void getAvailableActions(const GameState& state, std::vector<Action>& out);

// Smí hráč, který smí jednat, TEĎ blokovat? Ne z lehu (ř. 674-676) a ne po pohybu, pokud nedeklaroval
// blitz (ř. 675: „you may not move when you take a Block Action“). P196 (15.10.2026): jediné místo pro
// primitivní BLOCK i pro makro BLOCK — nabídka makra hlídala jen `hasActed`, které se nastaví až
// aktivací JINÉHO hráče, takže hráč s rozehraným MOVE dostal BLOCK, který rozbalení nenašlo.
inline bool mayBlockNow(const Player& p) {
    return p.state == PlayerState::STANDING && !(p.hasMoved && !p.usedBlitz);
}

} // namespace bb
