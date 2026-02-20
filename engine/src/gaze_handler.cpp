#include "bb/gaze_handler.h"
#include "bb/helpers.h"
#include <algorithm>

namespace bb {

ActionResult resolveHypnoticGaze(GameState& state, int gazerId, int targetId,
                                 DiceRollerBase& dice, std::vector<GameEvent>* events) {
    Player& gazer = state.getPlayer(gazerId);
    Player& target = state.getPlayer(targetId);

    gazer.hasActed = true;

    // Roll D6: need min(6, 2 + TZ_on_gazer)
    int tz = countTacklezones(state, gazer.position, gazer.teamSide);
    int gazeTarget = std::min(6, 2 + tz);

    int roll = dice.rollD6();
    emitEvent(events, {GameEvent::Type::SKILL_USED, gazerId, targetId, gazer.position,
                      target.position, static_cast<int>(SkillName::HypnoticGaze),
                      roll >= gazeTarget});

    if (roll >= gazeTarget) {
        // Success: target loses tacklezones
        target.lostTacklezones = true;
        return ActionResult::ok();
    }

    // Failure: turnover
    return ActionResult::turnovr();
}

} // namespace bb
