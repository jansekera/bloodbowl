#include "bb/gaze_handler.h"
#include "bb/helpers.h"
#include <algorithm>

namespace bb {

ActionResult resolveHypnoticGaze(GameState& state, int gazerId, int targetId,
                                 DiceRollerBase& dice, std::vector<GameEvent>* events) {
    Player& gazer = state.getPlayer(gazerId);
    Player& target = state.getPlayer(targetId);

    gazer.hasActed = true;

    // TA5 (24.08.2026) -- BB2016 l. 8181-8189: "Make an AGILITY ROLL for the
    // player with hypnotic gaze, with a -1 modifier for each opposing tackle
    // zone on the player with hypnotic gaze OTHER THAN THE VICTIM'S. ...
    // If the roll fails, then the hypnotic gaze HAS NO EFFECT."
    // Dve vady: (1) cil se pocital jako min(6, 2 + TZ) UPLNE BEZ AG -- pro AG4
    // upira to nahodou vychazelo stejne, pro jine AG ne; a zapocitaval se
    // i tacklezone OBETI, kterou pravidlo vyslovne vyjima. (2) neuspech byl
    // turnover -- katalog turnoveru (l. 366-382) gaze vubec nezna.
    int tz = 0;
    for (const Position& pos : gazer.position.getAdjacent()) {
        if (!pos.isOnPitch()) continue;
        const Player* opp = state.getPlayerAtPosition(pos);
        if (!opp || opp->teamSide == gazer.teamSide) continue;
        if (!canAct(opp->state) || opp->lostTacklezones) continue;
        if (opp->id == targetId) continue;   // "other than the victim's"
        ++tz;
    }
    int gazeTarget = std::clamp(7 - gazer.stats.agility + tz, 2, 6);

    int roll = dice.rollD6();
    emitEvent(events, {GameEvent::Type::SKILL_USED, gazerId, targetId, gazer.position,
                      target.position, static_cast<int>(SkillName::HypnoticGaze),
                      roll >= gazeTarget});

    if (roll >= gazeTarget) {
        // Success: target loses tacklezones (a l. 8185-8188 mu bere i chytani,
        // intercept, prihravku, asistence a dobrovolny pohyb)
        target.lostTacklezones = true;
    }
    // Neuspech = "has no effect". Akce je spotrebovana (hasActed vys), ale
    // turnover to NENI.
    return ActionResult::ok();
}

} // namespace bb
