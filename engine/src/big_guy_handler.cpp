#include "bb/big_guy_handler.h"
#include "bb/helpers.h"
#include "bb/injury.h"
#include "bb/ball_handler.h"

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
        if (roll >= 2) {
            // M3: "until he manages to roll a 2 OR BETTER at the start of a
            // future Action" (r. 7985-7986) -- uspesny hod stav ukoncuje.
            player.bigGuyStupefied = false;
            player.lostTacklezones = false;
        }
        if (roll == 1) {
            player.lostTacklezones = true;
            player.bigGuyStupefied = true;
            player.hasActed = true;
            player.hasMoved = true;
            result.actionBlocked = true;
            result.proceed = false;
            result.wastesTeamAction = true;   // M2: tym prichazi o deklarovanou akci
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
        if (roll >= target) {
            // M3: r. 8404-8405, "until he manages to roll a successful result
            // for a Really Stupid roll at the start of a future Action".
            player.bigGuyStupefied = false;
            player.lostTacklezones = false;
        }
        if (roll < target) {
            player.lostTacklezones = true;
            player.bigGuyStupefied = true;
            player.hasActed = true;
            player.hasMoved = true;
            result.actionBlocked = true;
            result.proceed = false;
            result.wastesTeamAction = true;   // M2: tym prichazi o deklarovanou akci
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
            result.wastesTeamAction = true;   // M2: "the Action is wasted"
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
                // M2 (29.08.): tym o deklarovanou akci PRICHAZI i tady.
                // Nejdriv jsem sem napsal opak, protoze l. 8580-8583 mluvi jen
                // o zakazu bloku. Rozhoduje ale l. 351-352: "IMPORTANT: This
                // Action may NOT BE DECLARED by more than one player per turn"
                // -- limit visi na DEKLARACI, ne na dokonceni, a Take Root se
                // hazi az "immediately after declaring an Action".
                result.wastesTeamAction = true;
                return result;
            }
        }
    }

    // TA10 (24.08.2026) -- BB2016 l. 7929-7947. Puvodni kod delal z kousnuti
    // AUTO-KO Thralla a z upira bez Thralla take KO, a ani jedno nebylo
    // turnover. Pravidlo rika neco jineho na obou stranach.
    if (player.hasSkill(SkillName::Bloodlust)) {
        int roll = dice.rollD6();
        emitEvent(events, {GameEvent::Type::SKILL_USED, playerId, -1, {}, {},
                          static_cast<int>(SkillName::Bloodlust), roll >= 2});
        if (roll == 1) {
            // l. 7938-7939: Thrall smi byt "standing, PRONE OR STUNNED" --
            // puvodni kod chtel canAct(), tedy jen stojiciho.
            int thrallId = -1;
            for (const Position& pos : player.position.getAdjacent()) {
                if (!pos.isOnPitch()) continue;
                const Player* ally = state.getPlayerAtPosition(pos);
                if (ally && ally->teamSide == player.teamSide &&
                    isOnPitch(ally->state) && !ally->hasSkill(SkillName::Bloodlust)) {
                    thrallId = ally->id;
                    break;
                }
            }

            if (thrallId >= 0) {
                // l. 7939-7941: "make an INJURY ROLL on the Thrall treating any
                // casualty roll as BADLY HURT" -- tedy hod na zraneni, ne KO,
                // a bez hodu na zbroj. "The injury will not cause a turnover
                // UNLESS THE THRALL WAS HOLDING THE BALL."
                const bool thrallHadBall =
                    state.ball.isHeld && state.ball.carrierId == thrallId;

                InjuryContext ctx{};
                resolveInjuryRoll(state, thrallId, dice, ctx, events);

                Player& thrall = state.getPlayer(thrallId);
                if (thrall.state == PlayerState::DEAD) {
                    // "treating any casualty roll as Badly Hurt" -- z kousnuti
                    // se neumira.
                    thrall.state = PlayerState::INJURED;
                }
                if (thrallHadBall) {
                    handleBallOnPlayerDown(state, thrallId, dice, events);
                    result.actionBlocked = true;
                    result.proceed = false;
                    result.turnover = true;
                    return result;
                }
                // Nakrmil se, akce pokracuje.
                result.actionBlocked = false;
                result.proceed = true;
            } else {
                // l. 7942-7947: "Failure to bite a Thrall IS A TURNOVER and
                // requires you to feed on a spectator -- move the Vampire to
                // the RESERVES BOX. If he was holding the ball, IT BOUNCES from
                // the square he occupied." Puvodne se z upira delalo KO (tedy
                // hráč, ktery se muze vratit hodem 4+) a turnover zadny.
                const bool vampHadBall =
                    state.ball.isHeld && state.ball.carrierId == playerId;
                if (vampHadBall) {
                    handleBallOnPlayerDown(state, playerId, dice, events);
                }
                player.state = PlayerState::OFF_PITCH;   // reserves
                player.position = {-1, -1};
                result.actionBlocked = true;
                result.proceed = false;
                result.turnover = true;
            }
            return result;
        }
    }

    return result;
}

} // namespace bb
