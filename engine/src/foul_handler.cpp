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

    bool armourBroken = (armourRoll > target.stats.armour);

    emitEvent(events, {GameEvent::Type::FOUL, fouler.id, target.id,
                      fouler.position, target.position, armourRoll, armourBroken,
                      die1, die2});

    if (armourBroken) {
        emitEvent(events, {GameEvent::Type::ARMOR_BREAK, target.id, -1,
                          target.position, {}, armourRoll, true, die1, die2});

        // Injury roll -- delegate to the shared helper (also used by
        // BLOCK/bomb/ball-and-chain) instead of the previous inline
        // reimplementation, which built this same InjuryContext but never
        // actually passed it anywhere: Decay was silently inert on FOUL-
        // caused injuries (no roll-twice-take-worse), and no INJURY/
        // CASUALTY/SKILL_USED/REGENERATION event was ever emitted, unlike
        // every other injury-causing path (see
        // project_bloodbowl_why_not_beating_frozen_20260723, item 3.6).
        InjuryContext ctx;
        ctx.armourModifier = 0; // already applied to armor roll
        if (target.hasSkill(SkillName::Decay)) ctx.hasDecay = true;
        if (fouler.hasSkill(SkillName::Stakes)) ctx.hasStakes = true;

        resolveInjuryRoll(state, target.id, dice, ctx, events);

        handleBallOnPlayerDown(state, target.id, dice, events);
    }

    // Doubles: fouler ejected (SneakyGit prevents)
    if (isDoubles) {
        if (!fouler.hasSkill(SkillName::SneakyGit)) {
            fouler.state = PlayerState::EJECTED;
            fouler.position = {-1, -1};
            handleBallOnPlayerDown(state, fouler.id, dice, events);
            emitEvent(events, {GameEvent::Type::EJECTED, fouler.id, -1, {}, {},
                              0, false});
        }
    }

    fouler.hasActed = true;
    state.getTeamState(fouler.teamSide).foulUsedThisTurn = true;

    return ActionResult::ok();
}

} // namespace bb
