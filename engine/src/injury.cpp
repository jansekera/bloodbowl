#include "bb/injury.h"
#include "bb/helpers.h"
#include "bb/ball_handler.h"

namespace bb {

int resolveInjuryRoll(GameState& state, int playerId, DiceRollerBase& dice,
                      const InjuryContext& ctx, std::vector<GameEvent>* events) {
    Player& player = state.getPlayer(playerId);

    int d1 = dice.rollD6();
    int d2 = dice.rollD6();
    int injuryRoll = d1 + d2 + ctx.injuryModifier;

    // Decay does NOT touch the Injury roll (rules parity, 2026-08-10). CRP:
    // "When this player suffers a Casualty result on the Injury table, roll
    // twice on the Casualty table and apply both results." It fires AFTER a
    // Casualty and doubles the CASUALTY roll -- it never made the injury
    // itself worse. We used to roll the injury twice and keep the worse,
    // which made a Decay player markedly easier to remove from the match.
    // Since this engine models a single match and has no Casualty table
    // (10+ is simply INJURED), a correct Decay has no in-match effect at
    // all; ctx.hasDecay is kept for the day a league/Casualty model exists.

    // Stunty: +1 to injury
    if (player.hasSkill(SkillName::Stunty)) {
        injuryRoll += 1;
    }

    if (injuryRoll <= 7) {
        // Stunned
        player.state = PlayerState::STUNNED;
        emitEvent(events, {GameEvent::Type::INJURY, playerId, -1, player.position, {},
                          injuryRoll, false, d1, d2});
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
                          injuryRoll, false, d1, d2});
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
                          injuryRoll, false, d1, d2});

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

    int aD1 = dice.rollD6();
    int aD2 = dice.rollD6();
    int armourRoll = aD1 + aD2 + ctx.armourModifier;

    // Claw: armor broken on 8+ regardless of AV
    bool broken;
    if (ctx.hasClaw) {
        broken = (armourRoll >= 8) || (armourRoll > av);
    } else {
        broken = (armourRoll > av);
    }

    emitEvent(events, {GameEvent::Type::ARMOR_BREAK, playerId, -1, player.position, {},
                      armourRoll, broken, aD1, aD2});

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

    // Crowd injury: one plain Injury roll, NO modifiers (rules parity,
    // 2026-08-10; user decision). CRP: "beaten up only by the crowd and
    // receives one roll on the Injury table. The crowd does not have any
    // injury modifying skills." The +1 we used to add here had no basis in
    // the text. The victim's own traits (Stunty, Decay) still apply -- that
    // sentence is about the CROWD lacking skills like Mighty Blow.
    InjuryContext ctx;
    ctx.injuryModifier = 0;
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
