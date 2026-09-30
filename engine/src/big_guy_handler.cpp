#include "bb/big_guy_handler.h"
#include "bb/helpers.h"
#include "bb/injury.h"
#include "bb/ball_handler.h"

namespace bb {

BigGuyResult resolveBigGuyCheck(GameState& state, int playerId, ActionType actionType,
                                DiceRollerBase& dice, std::vector<GameEvent>* events,
                                bool standUpInPlace) {
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
            // M3b (29.08.2026): `!ally->lostTacklezones` tu bylo NAVIC.
            // r. 8393-8396 zada jen "players from the same team STANDING
            // ADJACENT ... AND WHO AREN'T REALLY STUPID" -- o tacklezonach nic.
            // Bone-headuv zakaz asistence je vyslovne jen "on a BLOCK OR FOUL"
            // (r. 7984-7985), a bonus k hodu Really Stupid neni ani jedno.
            // Podminka kousala vzacne, dokud se priznak cistil kazde kolo;
            // M3 ho necha pretrvat, takze od ni bylo potreba se zbavit hned.
            if (ally && ally->teamSide == player.teamSide &&
                canAct(ally->state) &&
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
            // ⭐ N14 (01.09.2026): VSTANI ZAKORENENI NEBRANI. r. 8583-8584
            //   konci zavorkou "(he can still roll to stand up if he is
            //   Prone)". Do dneska se to resilo tak, ze se pri vstavani hod
            //   NEHAZEL VUBEC (`standUpAttempt && onlyTakeRoot` v resolveru) --
            //   jenze tim Treeman, ktery mel zakorenit, priste zase normalne
            //   chodil. Ted se hod hazi a zakorenení PLATI; jen vstani samotne
            //   projde, protoze to pravidlo vyslovne dovoluje.
            const bool mayStillAct = (actionType == ActionType::BLOCK ||
                                      actionType == ActionType::PASS ||
                                      actionType == ActionType::HAND_OFF ||
                                      actionType == ActionType::FOUL ||
                                      standUpInPlace);
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
    if (player.hasSkill(SkillName::Bloodlust) && !player.bloodlustHungry) {
        int roll = dice.rollD6();
        emitEvent(events, {GameEvent::Type::SKILL_USED, playerId, -1, {}, {},
                          static_cast<int>(SkillName::Bloodlust), roll >= 2});
        if (roll == 1) {
            // ⛔ P71 (30.09.2026), vzor PHP 9e980cac. r. 7929-7936: upir akci
            //   DOKONCI a krmi se az na jejim konci ("at the end of the action,
            //   before actually passing, handing off or scoring"). C++ kousal
            //   hned pri ohlaseni a bez Thralla sel do rezerv, aniz se pohnul.
            //   r. 7932-7933: ohlaseny BLOCK smi zmenit na MOVE. Politika: kdyz
            //   vedle stoji Thrall, blokuje; jinak se rana nehodi a upir zustava
            //   NEAKTIVOVANY, aby mohl dojit k Thrallovi (priorita uzivatele 30.09.).
            player.bloodlustHungry = true;
            if (actionType == ActionType::BLOCK && adjacentThrall(state, player) < 0) {
                result.actionBlocked = true;
                result.proceed = false;
            }
            return result;
        }
    }

    return result;
}

int adjacentThrall(const GameState& state, const Player& vampire) {
    // l. 7938-7939: Thrall smi byt "standing, PRONE OR STUNNED".
    for (const Position& pos : vampire.position.getAdjacent()) {
        if (!pos.isOnPitch()) continue;
        const Player* ally = state.getPlayerAtPosition(pos);
        if (ally && ally->teamSide == vampire.teamSide && isOnPitch(ally->state) &&
            !ally->hasSkill(SkillName::Bloodlust)) {
            return ally->id;
        }
    }
    return -1;
}

bool feedBloodlust(GameState& state, int vampireId, DiceRollerBase& dice,
                   std::vector<GameEvent>* events) {
    Player& vampire = state.getPlayer(vampireId);
    vampire.bloodlustHungry = false;
    if (!vampire.isOnPitch()) return false;

    const int thrallId = adjacentThrall(state, vampire);
    emitEvent(events, {GameEvent::Type::BLOODLUST_FEED, vampireId, thrallId,
                      vampire.position, {}, 0, thrallId >= 0});
    if (thrallId >= 0) {
        // l. 7939-7941: "make an INJURY ROLL on the Thrall treating any
        // casualty roll as BADLY HURT" -- bez hodu na zbroj. "The injury will
        // not cause a turnover UNLESS THE THRALL WAS HOLDING THE BALL."
        const bool thrallHadBall = state.ball.isHeld && state.ball.carrierId == thrallId;
        InjuryContext ctx{};
        resolveInjuryRoll(state, thrallId, dice, ctx, events);
        Player& thrall = state.getPlayer(thrallId);
        if (thrall.state == PlayerState::DEAD) thrall.setState(PlayerState::INJURED);
        if (thrallHadBall) {
            handleBallOnPlayerDown(state, thrallId, dice, events);
            return true;
        }
        return false;
    }

    // l. 7942-7947: "Failure to bite a Thrall IS A TURNOVER ... move the
    // Vampire to the RESERVES BOX. If he was holding the ball, IT BOUNCES from
    // the square he occupied."
    if (state.ball.isHeld && state.ball.carrierId == vampireId) {
        handleBallOnPlayerDown(state, vampireId, dice, events);
    }
    vampire.setState(PlayerState::OFF_PITCH);   // reserves
    vampire.position = {-1, -1};
    return true;
}

} // namespace bb
