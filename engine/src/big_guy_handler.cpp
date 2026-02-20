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
        // Check for adjacent standing ally (non-ReallyStupid or any ally)
        bool hasAdjacentAlly = false;
        auto adj = player.position.getAdjacent();
        for (auto& pos : adj) {
            if (!pos.isOnPitch()) continue;
            const Player* ally = state.getPlayerAtPosition(pos);
            if (ally && ally->teamSide == player.teamSide &&
                canAct(ally->state) && !ally->lostTacklezones) {
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

    // WildAnimal: D6, 1-2=fail. Auto-pass for Block/Blitz
    if (player.hasSkill(SkillName::WildAnimal)) {
        if (actionType != ActionType::BLOCK && actionType != ActionType::BLITZ) {
            int roll = dice.rollD6();
            emitEvent(events, {GameEvent::Type::SKILL_USED, playerId, -1, {}, {},
                              static_cast<int>(SkillName::WildAnimal), roll >= 3});
            if (roll < 3) {
                // WildAnimal keeps tacklezones (unlike BoneHead/ReallyStupid)
                player.hasActed = true;
                player.hasMoved = true;
                result.actionBlocked = true;
                result.proceed = false;
                return result;
            }
        }
    }

    // TakeRoot: D6, 1=fail on MOVE actions only
    if (player.hasSkill(SkillName::TakeRoot)) {
        if (actionType == ActionType::MOVE || actionType == ActionType::BLITZ) {
            int roll = dice.rollD6();
            emitEvent(events, {GameEvent::Type::SKILL_USED, playerId, -1, {}, {},
                              static_cast<int>(SkillName::TakeRoot), roll >= 2});
            if (roll == 1) {
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
