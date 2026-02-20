#include "bb/foul_handler.h"
#include "bb/helpers.h"
#include "bb/injury.h"
#include "bb/ball_handler.h"

namespace bb {

ActionResult resolveFoul(GameState& state, int foulerId, int targetId,
                         DiceRollerBase& dice, std::vector<GameEvent>* events) {
    Player& fouler = state.getPlayer(foulerId);
    Player& target = state.getPlayer(targetId);

    // Target must be prone or stunned
    if (target.state != PlayerState::PRONE && target.state != PlayerState::STUNNED) {
        return ActionResult::fail();
    }

    // Calculate foul assists
    int friendlyAssists = countAssists(state, target.position, fouler.teamSide,
                                        fouler.id, target.id);
    int enemyAssists = countAssists(state, fouler.position, target.teamSide,
                                     fouler.id, target.id);
    int assistMod = friendlyAssists - enemyAssists;

    // DirtyPlayer bonus
    if (fouler.hasSkill(SkillName::DirtyPlayer)) {
        assistMod += 1;
    }

    // Roll two D6 individually (need to check for doubles)
    int die1 = dice.rollD6();
    int die2 = dice.rollD6();
    int armourRoll = die1 + die2 + assistMod;
    bool isDoubles = (die1 == die2);

    emitEvent(events, {GameEvent::Type::FOUL, fouler.id, target.id,
                      fouler.position, target.position, armourRoll, true});

    bool armourBroken = (armourRoll > target.stats.armour);

    if (armourBroken) {
        emitEvent(events, {GameEvent::Type::ARMOR_BREAK, target.id, -1,
                          target.position, {}, armourRoll, true});

        // Injury roll
        InjuryContext ctx;
        ctx.armourModifier = 0; // already applied to armor roll
        if (target.hasSkill(SkillName::Decay)) ctx.hasDecay = true;
        if (fouler.hasSkill(SkillName::Stakes)) ctx.hasStakes = true;

        int injuryRoll = dice.roll2D6();

        // Stunty: +1 to injury
        if (target.hasSkill(SkillName::Stunty)) injuryRoll += 1;

        if (injuryRoll <= 7) {
            target.state = PlayerState::STUNNED;
        } else if (injuryRoll <= 9) {
            if (target.hasSkill(SkillName::ThickSkull)) {
                int tsRoll = dice.rollD6();
                if (tsRoll >= 4) {
                    target.state = PlayerState::STUNNED;
                } else {
                    target.state = PlayerState::KO;
                    target.position = {-1, -1};
                }
            } else {
                target.state = PlayerState::KO;
                target.position = {-1, -1};
            }
        } else {
            // Casualty
            if (target.hasSkill(SkillName::Regeneration) && !ctx.hasStakes) {
                int regenRoll = dice.rollD6();
                if (regenRoll >= 4) {
                    target.state = PlayerState::STUNNED;
                } else {
                    target.state = PlayerState::INJURED;
                    target.position = {-1, -1};
                }
            } else {
                target.state = PlayerState::INJURED;
                target.position = {-1, -1};
            }
        }

        handleBallOnPlayerDown(state, target.id, dice, events);
    }

    // Doubles: fouler ejected (SneakyGit prevents)
    if (isDoubles) {
        if (!fouler.hasSkill(SkillName::SneakyGit)) {
            fouler.state = PlayerState::EJECTED;
            fouler.position = {-1, -1};
            handleBallOnPlayerDown(state, fouler.id, dice, events);
            emitEvent(events, {GameEvent::Type::INJURY, fouler.id, -1, {}, {},
                              0, false}); // ejection event
        }
    }

    fouler.hasActed = true;
    state.getTeamState(fouler.teamSide).foulUsedThisTurn = true;

    return ActionResult::ok();
}

} // namespace bb
