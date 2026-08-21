#include "bb/injury.h"
#include "bb/helpers.h"
#include "bb/ball_handler.h"
#include <string>

namespace bb {

// CRP Casualty table as a D68: a D6 gives the tens digit, a D8 the units.
// 11-38 Badly Hurt (24/48 = 50%), 41-48 miss next game, 51-52 Niggling,
// 53-54 -1 MA, 55-56 -1 AV, 57 -1 AG, 58 -1 ST, 61-68 DEAD (8/48 = 1 in 6).
CasualtyResult rollCasualty(DiceRollerBase& dice) {
    int tens = dice.rollD6();
    int units = dice.rollD8();
    if (tens <= 3) return CasualtyResult::BADLY_HURT;
    if (tens == 4) return CasualtyResult::MISS_NEXT_GAME;
    if (tens == 6) return CasualtyResult::DEAD;
    switch (units) {                      // tens == 5
        case 1: case 2: return CasualtyResult::NIGGLING;
        case 3: case 4: return CasualtyResult::MA_LOSS;
        case 5: case 6: return CasualtyResult::AV_LOSS;
        case 7:         return CasualtyResult::AG_LOSS;
        default:        return CasualtyResult::ST_LOSS;
    }
}

int resolveInjuryRoll(GameState& state, int playerId, DiceRollerBase& dice,
                      const InjuryContext& ctx, std::vector<GameEvent>* events,
                      bool* outDoubles) {
    Player& player = state.getPlayer(playerId);

    int d1 = dice.rollD6();
    int d2 = dice.rollD6();
    if (outDoubles) *outDoubles = (d1 == d2);
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
        // BB2016 l. 707: may not turn face up on the turn he is Stunned.
        player.stunnedThisTurn = true;
        emitEvent(events, {GameEvent::Type::INJURY, playerId, -1, player.position, {},
                          injuryRoll, false, d1, d2});
    } else if (injuryRoll <= 9) {
        // Thick Skull is DETERMINISTIC and applies to a modified 8 only
        // (rules parity, 2026-08-10). CRP: "This player treats a roll of 8 on
        // the Injury table, after any modifiers have been applied, as a
        // Stunned result rather than a KO'd result." We used to roll a D6 on
        // any KO result and save on 4+, which is wrong in both directions: an
        // 8 that should always be Stunned went to KO half the time, and a 9
        // that should always be a KO was saved half the time. Since 8 is the
        // commoner roll (5/36 vs 4/36) the net effect was HARSHER than the
        // rules -- KO on 12.5% of injury rolls instead of 11.1% -- and every
        // dwarf has this skill, so it was live and it cost us.
        if (player.hasSkill(SkillName::ThickSkull) && injuryRoll == 8) {
            player.state = PlayerState::STUNNED;
            player.stunnedThisTurn = true;
            emitEvent(events, {GameEvent::Type::SKILL_USED, playerId, -1, {}, {},
                              static_cast<int>(SkillName::ThickSkull), true});
            return injuryRoll;
        }
        player.state = PlayerState::KO;
        player.position = {-1, -1};
        emitEvent(events, {GameEvent::Type::INJURY, playerId, -1, {}, {},
                          injuryRoll, false, d1, d2});
    } else {
        // Casualty (10+) -- roll on the CASUALTY TABLE (package G, 2026-08-10).
        // Until now a 10+ was flatly INJURED, so death could not occur at all:
        // DEAD/game read 0.00 across 3200 games. CRP rolls D68 here; a sixth
        // of casualties are fatal.
        CasualtyResult cas = rollCasualty(dice);

        // Apothecary: "immediately after the player suffers the Casualty, you
        // can use the Apothecary to make your opponent roll again on the
        // Casualty table and then you choose which of the two results to
        // apply." Once per match. Taking the milder result is free, so it is
        // automatic. This is the apothecary's first use in the engine -- the
        // flags existed but nothing ever read them.
        // ⚠️ PLACEHOLDER POLICY, not a decision layer. The apothecary is
        // once per match, so WHEN to spend it is a real choice -- on the
        // lineman in front of you, or held for the Gutter Runner who may
        // never get hurt? Casualties suffered run ~0.4/game for dwarves and
        // ~1.5-2 for skaven, so "hold out" often means "never use it".
        //
        // Two conditions, both derived rather than invented:
        //  * WHO: not the cheapest body. Getting a lineman back is worth
        //    little, and the bench already covers him.
        //  * WHAT WAS ROLLED: the value of spending depends on the result.
        //    Badly Hurt is a CERTAIN return -- CRP says the Reserves rescue
        //    applies "even if it was the original Casualty roll", so no
        //    re-roll is needed. Dead is worth it for the opposite reason:
        //    it is the only irreversible outcome. The middle band (miss next
        //    game, stat loss) is the weak case -- there you would spend a
        //    once-per-match asset on a coin flip, so hold it instead.
        TeamState& ts = state.getTeamState(player.teamSide);
        // WHO is worth it, derived rather than named: a player is worth the
        // apothecary only if the bench cannot replace him. The old test asked
        // whether his position was called "Lineman", which is a label, not a
        // property -- and it silently did nothing for the dwarves, whose
        // roster contains no such name, so every Longbeard counted as
        // irreplaceable. The generic question is whether an identical
        // team-mate is sitting in Reserves ready to take his place; if one
        // is, spending a once-per-match asset on him buys nothing.
        bool replaceable = false;
        state.forEachPlayer(player.teamSide, [&](const Player& mate) {
            if (mate.id == player.id) return;
            if (mate.state != PlayerState::OFF_PITCH) return;
            if (mate.positionName == nullptr || player.positionName == nullptr) return;
            if (std::string(mate.positionName) == std::string(player.positionName)) {
                replaceable = true;
            }
        });
        bool worthSaving = !replaceable;
        bool worthSpendingOn = (cas == CasualtyResult::BADLY_HURT ||
                                cas == CasualtyResult::DEAD);
        if (ts.hasApothecary && !ts.apothecaryUsed && worthSaving && worthSpendingOn) {
            ts.apothecaryUsed = true;
            CasualtyResult second = rollCasualty(dice);
            if (casualtySeverity(second) < casualtySeverity(cas)) cas = second;
            emitEvent(events, {GameEvent::Type::SKILL_USED, playerId, -1, {}, {},
                              static_cast<int>(cas), true});
            // "If the player is only Badly Hurt after this roll (even if it
            // was the original Casualty roll) the Apothecary has managed to
            // patch him up ... the player may be moved into the Reserves box."
            if (cas == CasualtyResult::BADLY_HURT) {
                player.state = PlayerState::OFF_PITCH;   // Reserves
                player.position = {-1, -1};
                return injuryRoll;
            }
        }

        // Regeneration: rolled AFTER the casualty roll and after any
        // Apothecary, and may not be re-rolled. On 4+ the player "is placed
        // in the Reserves box instead" -- he is available again, not left
        // standing stunned on the pitch as we used to have it.
        if (player.hasSkill(SkillName::Regeneration) && !ctx.hasStakes) {
            int regenRoll = dice.rollD6();
            emitEvent(events, {GameEvent::Type::REGENERATION, playerId, -1, {}, {},
                              regenRoll, regenRoll >= 4});
            if (regenRoll >= 4) {
                player.state = PlayerState::OFF_PITCH;   // Reserves
                player.position = {-1, -1};
                return injuryRoll;
            }
        }

        player.state = (cas == CasualtyResult::DEAD) ? PlayerState::DEAD
                                                     : PlayerState::INJURED;
        player.position = {-1, -1};
        emitEvent(events, {GameEvent::Type::CASUALTY, playerId,
                          static_cast<int>(cas), {}, {},
                          injuryRoll, cas == CasualtyResult::DEAD, d1, d2});

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
    int base = aD1 + aD2 + ctx.armourModifier;

    // Claw: armor broken on 8+ regardless of AV, after modifications.
    auto breaksArmour = [&](int roll) {
        return ctx.hasClaw ? (roll >= 8 || roll > av) : (roll > av);
    };

    // Mighty Blow buys ONE roll, and which one is the coach's to choose: "if you
    // decide to use Mighty Blow to modify the Armour roll, you may not modify
    // the Injury roll as well." We used to add it to both at once, which is
    // wrong in every edition of the rules -- and, since Mighty Blow sits on the
    // orc Blitzer, the human Blitzer and Ogre and the wood elf Treeman while
    // neither our dwarves nor the Skaven field one, the error only ever hit us.
    //
    // Spend it where it does work: on the armour roll only when armour needs it
    // to break, and otherwise keep it for the injury roll. The naive repair --
    // always spend it on armour -- would weaken opponents further than the rules
    // do. Note this composes with Claw exactly as it should, since the +1 may
    // legitimately push a 7 up to the 8 Claw asks for.
    int armourRoll = base;
    bool spentOnArmour = false;
    if (ctx.mightyBlow && !breaksArmour(base) && breaksArmour(base + 1)) {
        armourRoll = base + 1;
        spentOnArmour = true;
    }
    bool broken = breaksArmour(armourRoll);

    emitEvent(events, {GameEvent::Type::ARMOR_BREAK, playerId, -1, player.position, {},
                      armourRoll, broken, aD1, aD2});

    if (broken) {
        InjuryContext injCtx = ctx;
        if (ctx.mightyBlow && !spentOnArmour) injCtx.injuryModifier += 1;
        resolveInjuryRoll(state, playerId, dice, injCtx, events);
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

    // A Stunned result from the crowd sends him to RESERVES, not to the KO
    // box (rules parity / package G, 2026-08-10). CRP: "If a 'Stunned' result
    // is rolled on the Injury table the player should be placed in the
    // Reserves box of the Dugout, and must remain there until a touchdown is
    // scored or the half ends." We used to convert it to a KO, which is
    // harsher: a KO must then roll 4+ to come back at all.
    if (isOnPitch(player.state)) {
        player.state = PlayerState::OFF_PITCH;   // Reserves
        player.position = {-1, -1};
    }
}

} // namespace bb
