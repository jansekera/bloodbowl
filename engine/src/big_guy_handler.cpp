#include "bb/big_guy_handler.h"
#include "bb/helpers.h"

namespace bb {

BigGuyResult resolveBigGuyCheck(GameState& state, int playerId, ActionType actionType,
                                DiceRollerBase& dice, std::vector<GameEvent>* events) {
    Player& player = state.getPlayer(playerId);
    BigGuyResult result;

    // BoneHead: D6, 1=fail → lostTZ + hasActed + hasMoved
    if (player.hasSkill(SkillName::BoneHead)) {
        int roll = dice.rollD6();
        emitEvent(events, {GameEvent::Type::SKILL_USED, playerId, -1, {}, {},
                          static_cast<int>(SkillName::BoneHead), roll >= 2});
        if (roll == 1) {
            player.lostTacklezones = true;
            player.hasActed = true;
            player.hasMoved = true;
            result.actionBlocked = true;
            result.proceed = false;
            return result;
        }
    }

    // ReallyStupid: D6, need 2+ with adjacent ally, 4+ alone
    if (player.hasSkill(SkillName::ReallyStupid)) {
        // Adjacent standing ally who is NOT himself Really Stupid (rules
        // parity, 2026-08-10). CRP: "If there are one or more players from
        // the same team standing adjacent to the Really Stupid player's
        // square, AND WHO AREN'T REALLY STUPID, then add 2 to the D6 roll."
        // We used to accept any ally, so two Really Stupid players propped
        // each other up.
        bool hasAdjacentAlly = false;
        auto adj = player.position.getAdjacent();
        for (auto& pos : adj) {
            if (!pos.isOnPitch()) continue;
            const Player* ally = state.getPlayerAtPosition(pos);
            if (ally && ally->teamSide == player.teamSide &&
                canAct(ally->state) && !ally->lostTacklezones &&
                !ally->hasSkill(SkillName::ReallyStupid)) {
                hasAdjacentAlly = true;
                break;
            }
        }

        int target = hasAdjacentAlly ? 2 : 4;
        int roll = dice.rollD6();
        emitEvent(events, {GameEvent::Type::SKILL_USED, playerId, -1, {}, {},
                          static_cast<int>(SkillName::ReallyStupid), roll >= target});
        if (roll < target) {
            player.lostTacklezones = true;
            player.hasActed = true;
            player.hasMoved = true;
            result.actionBlocked = true;
            result.proceed = false;
            return result;
        }
    }

    // WildAnimal: D6, +2 when the declared action is a Block or a Blitz,
    // 1-3 fails. Same shape as ReallyStupid above: needs 4+ normally, 2+
    // with the bonus. There is NO auto-pass -- a natural 1 fails even when
    // blocking or blitzing, because a roll of 1 before modifiers always
    // fails (rules correction, user 2026-08-07; before this the block/blitz
    // branch skipped the roll entirely, making a Rat Ogre / Minotaur a
    // sixth more reliable at hitting than the rules allow).
    if (player.hasSkill(SkillName::WildAnimal)) {
        bool hitting = (actionType == ActionType::BLOCK ||
                        actionType == ActionType::BLITZ);
        int target = hitting ? 2 : 4;
        int roll = dice.rollD6();
        emitEvent(events, {GameEvent::Type::SKILL_USED, playerId, -1, {}, {},
                          static_cast<int>(SkillName::WildAnimal), roll >= target});
        if (roll < target) {
            // WildAnimal keeps tacklezones (unlike BoneHead/ReallyStupid)
            player.hasActed = true;
            player.hasMoved = true;
            result.actionBlocked = true;
            result.proceed = false;
            return result;
        }
    }

    // TakeRoot -- BB2016 l. 8572-8584 (prepsano 24.08.2026, TA2).
    // Bylo spatne trojím zpusobem: (1) hod se hazel jen na MOVE a BLITZ, ale
    // pravidlo rika "immediately after declaring AN ACTION", tj. i na BLOCK,
    // PASS, HAND-OFF a FOUL -- Treeman tedy blokoval bez rizika; (2) zakorenení
    // NEPERZISTOVALO, takze priste zase normalne chodil; (3) na 1 se blokovala
    // KAZDA akce, ale pravidlo blok vyslovne DOVOLUJE ("may block adjacent
    // players without following-up as part of a Block Action") a zakazuje ho
    // jen po neuspesnem hodu v ramci BLITZE.
    if (player.hasSkill(SkillName::TakeRoot) && !player.rooted) {
        int roll = dice.rollD6();
        emitEvent(events, {GameEvent::Type::SKILL_USED, playerId, -1, {}, {},
                          static_cast<int>(SkillName::TakeRoot), roll >= 2});
        if (roll == 1) {
            // l. 8574-8576: MA = 0 az do konce drivu (nebo do srazeni).
            player.rooted = true;
            player.movementRemaining = 0;
            // l. 8580-8583: blok sousedu smi (bez follow-upu, resi se v
            // block_handleru); po neuspechu v ramci BLITZE blokovat NESMI.
            // MOVE se zakorenením ztraci smysl -- MA je 0 -- a nechavame ho
            // propadnout jako driv, at se aktivace spotrebuje spravne (P55).
            const bool mayStillAct = (actionType == ActionType::BLOCK ||
                                      actionType == ActionType::PASS ||
                                      actionType == ActionType::HAND_OFF ||
                                      actionType == ActionType::FOUL);
            if (!mayStillAct) {
                player.hasActed = true;
                player.hasMoved = true;
                result.actionBlocked = true;
                result.proceed = false;
                return result;
            }
        }
    }

    // Bloodlust: D6, 2+=pass. Fail: bite adjacent Thrall
    if (player.hasSkill(SkillName::Bloodlust)) {
        int roll = dice.rollD6();
        emitEvent(events, {GameEvent::Type::SKILL_USED, playerId, -1, {}, {},
                          static_cast<int>(SkillName::Bloodlust), roll >= 2});
        if (roll == 1) {
            // Find adjacent Thrall (teammate without Bloodlust skill)
            int thrallId = -1;
            auto adj = player.position.getAdjacent();
            for (auto& pos : adj) {
                if (!pos.isOnPitch()) continue;
                const Player* ally = state.getPlayerAtPosition(pos);
                if (ally && ally->teamSide == player.teamSide &&
                    canAct(ally->state) && !ally->hasSkill(SkillName::Bloodlust)) {
                    thrallId = ally->id;
                    break;
                }
            }

            if (thrallId >= 0) {
                // Bite Thrall: KO + remove from pitch
                Player& thrall = state.getPlayer(thrallId);
                thrall.state = PlayerState::KO;
                thrall.position = {-1, -1};
                emitEvent(events, {GameEvent::Type::INJURY, thrallId, playerId, {}, {},
                                  0, false});
                // Action still proceeds
                result.actionBlocked = false;
                result.proceed = true;
            } else {
                // No Thrall available: player goes off pitch
                player.state = PlayerState::KO;
                player.position = {-1, -1};
                result.actionBlocked = true;
                result.proceed = false;
            }
            return result;
        }
    }

    return result;
}

} // namespace bb
