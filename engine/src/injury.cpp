#include "bb/injury.h"
#include "bb/helpers.h"
#include "bb/ball_handler.h"

namespace bb {

static int resolveInjuryRoll(GameState& state, int playerId, DiceRollerBase& dice,
                             const InjuryContext& ctx, std::vector<GameEvent>* events) {
    Player& player = state.getPlayer(playerId);

    int injuryRoll = dice.roll2D6() + ctx.injuryModifier;

    // Decay: roll twice, take worse
    if (ctx.hasDecay) {
        int secondRoll = dice.roll2D6() + ctx.injuryModifier;
        injuryRoll = std::max(injuryRoll, secondRoll);
    }

    // Stunty: +1 to injury
    if (player.hasSkill(SkillName::Stunty)) {
        injuryRoll += 1;
    }

    if (injuryRoll <= 7) {
        // Stunned
        player.state = PlayerState::STUNNED;
        emitEvent(events, {GameEvent::Type::INJURY, playerId, -1, player.position, {},
                          injuryRoll, false});
    } else if (injuryRoll <= 9) {
        // KO — ThickSkull: 4+ saves from KO (stays stunned)
        if (player.hasSkill(SkillName::ThickSkull)) {
            int thickSkullRoll = dice.rollD6();
            if (thickSkullRoll >= 4) {
                player.state = PlayerState::STUNNED;
                emitEvent(events, {GameEvent::Type::SKILL_USED, playerId, -1, {}, {},
                                  static_cast<int>(SkillName::ThickSkull), true});
                return injuryRoll;
            }
        }
        player.state = PlayerState::KO;
        player.position = {-1, -1};
        emitEvent(events, {GameEvent::Type::INJURY, playerId, -1, {}, {},
                          injuryRoll, false});
    } else {
        // Casualty (10+)
        // Regeneration save (4+), blocked by Stakes
        if (player.hasSkill(SkillName::Regeneration) && !ctx.hasStakes) {
            int regenRoll = dice.rollD6();
            emitEvent(events, {GameEvent::Type::REGENERATION, playerId, -1, {}, {},
                              regenRoll, regenRoll >= 4});
            if (regenRoll >= 4) {
                player.state = PlayerState::STUNNED;
                return injuryRoll;
            }
        }
        player.state = PlayerState::INJURED;
        player.position = {-1, -1};
        emitEvent(events, {GameEvent::Type::CASUALTY, playerId, -1, {}, {},
                          injuryRoll, false});

        if (ctx.hasNurglesRot) {
            emitEvent(events, {GameEvent::Type::SKILL_USED, playerId, -1, {}, {},
                              static_cast<int>(SkillName::NurglesRot), true});
        }
    }

    return injuryRoll;
}

bool resolveArmourAndInjury(GameState& state, int playerId, DiceRollerBase& dice,
                            const InjuryContext& ctx, std::vector<GameEvent>* events) {
    Player& player = state.getPlayer(playerId);
    int av = player.stats.armour;

    int armourRoll = dice.roll2D6() + ctx.armourModifier;

    // Claw: armor broken on 8+ regardless of AV
    bool broken;
    if (ctx.hasClaw) {
        broken = (armourRoll >= 8) || (armourRoll > av);
    } else {
        broken = (armourRoll > av);
    }

    emitEvent(events, {GameEvent::Type::ARMOR_BREAK, playerId, -1, player.position, {},
                      armourRoll, broken});

    if (broken) {
        resolveInjuryRoll(state, playerId, dice, ctx, events);
        return true;
    }

    return false;
}

void resolveCrowdSurf(GameState& state, int playerId, DiceRollerBase& dice,
                      std::vector<GameEvent>* events) {
    Player& player = state.getPlayer(playerId);

    emitEvent(events, {GameEvent::Type::INJURY, playerId, -1, player.position, {},
                      0, true});

    // Crowd injury: injury roll with +1
    InjuryContext ctx;
    ctx.injuryModifier = 1;
    if (player.hasSkill(SkillName::Decay)) ctx.hasDecay = true;

    // No armor roll — go straight to injury
    resolveInjuryRoll(state, playerId, dice, ctx, events);

    // If player survived (KO), they're already placed off-pitch by injury resolver
    // If still on pitch (STUNNED from ThickSkull), remove them
    if (isOnPitch(player.state)) {
        player.state = PlayerState::KO;
        player.position = {-1, -1};
    }
}

} // namespace bb
