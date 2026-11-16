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
// Blitz smí blokovat jen jednou (ř. 347-350, `blitzBlockThrown`).
inline bool mayBlockNow(const Player& p) {
    // P303: ležící s Jump Up smí ohlásit Block Action (ř. 8199-8203; resolver hází AG +2), ne však v blitzu.
    const bool jumpUp = p.state == PlayerState::PRONE && p.hasSkill(SkillName::JumpUp) && !p.usedBlitz;
    return (p.state == PlayerState::STANDING || jumpUp) && !(p.hasMoved && !p.usedBlitz) &&
           !(p.usedBlitz && p.blitzBlockThrown);
}

} // namespace bb
