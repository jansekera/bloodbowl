#include <gtest/gtest.h>
#include <algorithm>
#include "bb/injury.h"
#include "bb/helpers.h"

using namespace bb;

static void placePlayer(GameState& gs, int id, Position pos, TeamSide side,
                         int ma = 6, int st = 3, int ag = 3, int av = 8) {
    Player& p = gs.getPlayer(id);
    p.state = PlayerState::STANDING;
    p.position = pos;
    p.stats = {static_cast<int8_t>(ma), static_cast<int8_t>(st),
               static_cast<int8_t>(ag), static_cast<int8_t>(av)};
    p.movementRemaining = ma;
}

TEST(Injury, ArmourNotBroken) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).state = PlayerState::PRONE;
    // AV8: need > 8 to break. Roll 3+4=7, not broken
    FixedDiceRoller dice({3, 4});
    InjuryContext ctx;
    bool broken = resolveArmourAndInjury(gs, 1, dice, ctx, nullptr);
    EXPECT_FALSE(broken);
    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::PRONE); // unchanged
}

TEST(Injury, ArmourBrokenStunned) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).state = PlayerState::PRONE;
    // AV8: 5+4=9 > 8, broken. Injury: 2+3=5 ≤ 7 → stunned
    FixedDiceRoller dice({5, 4, 2, 3});
    InjuryContext ctx;
    bool broken = resolveArmourAndInjury(gs, 1, dice, ctx, nullptr);
    EXPECT_TRUE(broken);
    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::STUNNED);
}

TEST(Injury, EventsCarryIndividualDice) {
    // 2026-07-24 (item 3.6): `roll` alone can't tell an unmodified 2d6
    // result apart from a modified one during forensic replay analysis --
    // ARMOR_BREAK/INJURY events now also carry the individual d6 faces.
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).state = PlayerState::PRONE;
    // AV8: 5+4=9 > 8, broken. Injury: 2+3=5 ≤ 7 → stunned
    FixedDiceRoller dice({5, 4, 2, 3});
    InjuryContext ctx;
    std::vector<GameEvent> events;
    bool broken = resolveArmourAndInjury(gs, 1, dice, ctx, &events);
    ASSERT_TRUE(broken);

    auto armorEvt = std::find_if(events.begin(), events.end(), [](const GameEvent& e) {
        return e.type == GameEvent::Type::ARMOR_BREAK;
    });
    ASSERT_NE(armorEvt, events.end());
    EXPECT_EQ(armorEvt->die1, 5);
    EXPECT_EQ(armorEvt->die2, 4);

    auto injuryEvt = std::find_if(events.begin(), events.end(), [](const GameEvent& e) {
        return e.type == GameEvent::Type::INJURY;
    });
    ASSERT_NE(injuryEvt, events.end());
    EXPECT_EQ(injuryEvt->die1, 2);
    EXPECT_EQ(injuryEvt->die2, 3);
}

TEST(Injury, DecayDoesNotAffectTheInjuryRoll) {
    // Rules parity 2026-08-10: Decay fires only AFTER a Casualty result and
    // doubles the CASUALTY roll -- it never modifies the Injury roll itself.
    // We used to roll the injury twice and keep the worse, which made a Decay
    // player markedly easier to remove. With no Casualty table in this
    // single-match engine, a correct Decay is inert here.
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).state = PlayerState::PRONE;
    gs.getPlayer(1).skills.add(SkillName::Decay);
    // Armour: 5+4=9 > 8, broken. Injury: 3+3=6 -> Stunned, and that stands.
    // The trailing 5,4 must NOT be consumed as a second injury roll.
    FixedDiceRoller dice({5, 4, 3, 3, 5, 4});
    InjuryContext ctx;
    ctx.hasDecay = true;
    std::vector<GameEvent> events;
    bool broken = resolveArmourAndInjury(gs, 1, dice, ctx, &events);
    ASSERT_TRUE(broken);
    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::STUNNED);

    auto injuryEvt = std::find_if(events.begin(), events.end(), [](const GameEvent& e) {
        return e.type == GameEvent::Type::INJURY;
    });
    ASSERT_NE(injuryEvt, events.end());
    EXPECT_EQ(injuryEvt->roll, 6);
    EXPECT_EQ(injuryEvt->die1, 3);
    EXPECT_EQ(injuryEvt->die2, 3);
}

TEST(Injury, ArmourBrokenKO) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).state = PlayerState::PRONE;
    // AV8: 5+4=9 > 8. Injury: 4+4=8 → KO
    FixedDiceRoller dice({5, 4, 4, 4});
    InjuryContext ctx;
    bool broken = resolveArmourAndInjury(gs, 1, dice, ctx, nullptr);
    EXPECT_TRUE(broken);
    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::KO);
    EXPECT_EQ(gs.getPlayer(1).position, (Position{-1, -1}));
}

TEST(Injury, ArmourBrokenCasualty) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).state = PlayerState::PRONE;
    // AV8: 5+4=9 > 8. Injury: 5+5=10 -> casualty.
    // Casualty table is D68: tens=1 -> Badly Hurt, out for the match.
    FixedDiceRoller dice({5, 4, 5, 5, 1, 1});
    InjuryContext ctx;
    bool broken = resolveArmourAndInjury(gs, 1, dice, ctx, nullptr);
    EXPECT_TRUE(broken);
    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::INJURED);
}

TEST(Injury, ClawBreaksOn8Plus) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME, 6, 3, 3, 9); // AV9
    gs.getPlayer(1).state = PlayerState::PRONE;
    // Without Claw: 4+4=8, not > 9. With Claw: 8 >= 8 → broken
    // Injury: 2+2=4 → stunned
    FixedDiceRoller dice({4, 4, 2, 2});
    InjuryContext ctx;
    ctx.hasClaw = true;
    bool broken = resolveArmourAndInjury(gs, 1, dice, ctx, nullptr);
    EXPECT_TRUE(broken);
}

TEST(Injury, ArmourModifier) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).state = PlayerState::PRONE;
    // AV8: 4+4+1=9 > 8. Injury: 2+2=4 → stunned
    FixedDiceRoller dice({4, 4, 2, 2});
    InjuryContext ctx;
    ctx.armourModifier = 1;
    bool broken = resolveArmourAndInjury(gs, 1, dice, ctx, nullptr);
    EXPECT_TRUE(broken);
}

TEST(Injury, ThickSkullTurnsAnEightIntoStunned) {
    // CRP Thick Skull: "treats a roll of 8 on the Injury table, after any
    // modifiers have been applied, as a Stunned result rather than a KO'd
    // result." Deterministic -- no save roll, so no extra die is consumed.
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).state = PlayerState::PRONE;
    gs.getPlayer(1).skills.add(SkillName::ThickSkull);
    // Armour: 5+4=9 > 8, broken. Injury: 4+4=8 -> Stunned.
    FixedDiceRoller dice({5, 4, 4, 4});
    InjuryContext ctx;
    bool broken = resolveArmourAndInjury(gs, 1, dice, ctx, nullptr);
    EXPECT_TRUE(broken);
    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::STUNNED);
}

TEST(Injury, ThickSkullDoesNotSaveANine) {
    // Only an 8 is downgraded; a 9 is still a KO. We used to roll a D6 and
    // save on 4+ for ANY KO result, which rescued nines that should stand.
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).state = PlayerState::PRONE;
    gs.getPlayer(1).skills.add(SkillName::ThickSkull);
    // Armour: 5+4=9. Injury: 4+5=9 -> KO despite Thick Skull.
    FixedDiceRoller dice({5, 4, 4, 5});
    InjuryContext ctx;
    bool broken = resolveArmourAndInjury(gs, 1, dice, ctx, nullptr);
    EXPECT_TRUE(broken);
    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::KO);
}

TEST(Injury, RegenerationSaves) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).state = PlayerState::PRONE;
    gs.getPlayer(1).skills.add(SkillName::Regeneration);
    // Armour 5+5=10, injury 5+5=10 -> casualty; D68 6,1 -> DEAD; regen 4 saves.
    // CRP: a successful Regeneration puts the player "in the Reserves box" --
    // available again -- not standing stunned on the pitch as we used to have
    // it. And it saves from DEATH too, not just from a lesser casualty.
    FixedDiceRoller dice({5, 5, 5, 5, 6, 1, 4});
    InjuryContext ctx;
    bool broken = resolveArmourAndInjury(gs, 1, dice, ctx, nullptr);
    EXPECT_TRUE(broken);
    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::OFF_PITCH) << "back in Reserves";
}

TEST(Injury, StakesBlocksRegeneration) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).state = PlayerState::PRONE;
    gs.getPlayer(1).skills.add(SkillName::Regeneration);
    // Armour 5+5=10, injury 5+5=10, D68 1,1 -> Badly Hurt. Stakes blocks regen.
    FixedDiceRoller dice({5, 5, 5, 5, 1, 1});
    InjuryContext ctx;
    ctx.hasStakes = true;
    bool broken = resolveArmourAndInjury(gs, 1, dice, ctx, nullptr);
    EXPECT_TRUE(broken);
    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::INJURED);
}

TEST(Injury, CrowdSurfHasNoInjuryModifier) {
    // Rules parity 2026-08-10 (user decision): "beaten up only by the crowd
    // and receives one roll on the Injury table. The crowd does not have any
    // injury modifying skills." The +1 we used to add had no basis in the
    // text. 3+3=6 is a Stunned result, so the player only leaves the pitch
    // through the crowd-surf removal below, not through the injury itself.
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    FixedDiceRoller dice({3, 3});
    std::vector<GameEvent> events;
    resolveCrowdSurf(gs, 1, dice, &events);
    auto injuryEvt = std::find_if(events.begin(), events.end(), [](const GameEvent& e) {
        return e.type == GameEvent::Type::INJURY && e.roll > 0;
    });
    ASSERT_NE(injuryEvt, events.end());
    EXPECT_EQ(injuryEvt->roll, 6);  // was 7 with the old +1
}

TEST(Injury, CrowdSurf) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    // Crowd surf: injury with +1 modifier. Roll: 3+3+1=7 → stunned → forced to KO
    FixedDiceRoller dice({3, 3});
    resolveCrowdSurf(gs, 1, dice, nullptr);
    // Crowd surf always removes from pitch, even if injury says stunned
    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::KO);
    EXPECT_EQ(gs.getPlayer(1).position, (Position{-1, -1}));
}

TEST(Injury, StuntyInjuryBonus) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).state = PlayerState::PRONE;
    gs.getPlayer(1).skills.add(SkillName::Stunty);
    // Armor: 5+4=9 > 8. Injury: 3+4=7 +1(Stunty)=8 → KO
    FixedDiceRoller dice({5, 4, 3, 4});
    InjuryContext ctx;
    bool broken = resolveArmourAndInjury(gs, 1, dice, ctx, nullptr);
    EXPECT_TRUE(broken);
    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::KO);
}


// --- Casualty table (package G, 2026-08-10) ---------------------------

TEST(Injury, CasualtyTableCanKill) {
    // Until the table existed a 10+ was flatly INJURED, so DEAD/game read
    // 0.00 across 3200 games. D68 with tens=6 is the fatal band, 8 of 48.
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).state = PlayerState::PRONE;
    FixedDiceRoller dice({5, 5, 5, 5, 6, 3});   // armour, injury 10, D68 63
    InjuryContext ctx;
    resolveArmourAndInjury(gs, 1, dice, ctx, nullptr);
    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::DEAD);
}

TEST(Injury, ApothecaryPicksTheMilderOfTwoCasualties) {
    // CRP: the opponent rolls again and you choose which result to apply;
    // if the outcome is only Badly Hurt the player goes to Reserves -- back
    // into this match. That is the whole reason a single-match engine needs
    // the casualty table at all.
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).state = PlayerState::PRONE;
    gs.getPlayer(1).positionName = "Blitzer";     // not a lineman: worth saving
    gs.getTeamState(TeamSide::HOME).hasApothecary = true;
    // armour, injury 10, first D68 = 6,1 (DEAD), apothecary D68 = 1,1 (BH)
    FixedDiceRoller dice({5, 5, 5, 5, 6, 1, 1, 1});
    InjuryContext ctx;
    resolveArmourAndInjury(gs, 1, dice, ctx, nullptr);
    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::OFF_PITCH);
    EXPECT_TRUE(gs.getTeamState(TeamSide::HOME).apothecaryUsed);
}

TEST(Injury, ApothecaryIsNotSpentOnALineman) {
    // Placeholder policy, not a decision layer: the apothecary is once per
    // match, so it is never burned on the cheapest body. Choosing WHEN to
    // spend it properly is queued separately.
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).state = PlayerState::PRONE;
    gs.getPlayer(1).positionName = "Lineman";
    gs.getTeamState(TeamSide::HOME).hasApothecary = true;
    FixedDiceRoller dice({5, 5, 5, 5, 6, 1});    // DEAD, no apothecary reroll
    InjuryContext ctx;
    resolveArmourAndInjury(gs, 1, dice, ctx, nullptr);
    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::DEAD);
    EXPECT_FALSE(gs.getTeamState(TeamSide::HOME).apothecaryUsed) << "kept for someone better";
}

TEST(Injury, ApothecaryIsHeldOnTheWeakMiddleBand) {
    // Spending on "miss next game" buys only a coin flip on the re-roll,
    // for a once-per-match asset. Badly Hurt (a certain return) and Dead
    // (the only irreversible result) are the cases worth it; the middle is
    // not. Placeholder policy - the real decision is queued.
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).state = PlayerState::PRONE;
    gs.getPlayer(1).positionName = "Blitzer";
    gs.getTeamState(TeamSide::HOME).hasApothecary = true;
    // armour, injury 10, D68 = 4,1 -> Miss next game
    FixedDiceRoller dice({5, 5, 5, 5, 4, 1});
    InjuryContext ctx;
    resolveArmourAndInjury(gs, 1, dice, ctx, nullptr);
    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::INJURED);
    EXPECT_FALSE(gs.getTeamState(TeamSide::HOME).apothecaryUsed) << "kept back";
}

TEST(Injury, ApothecaryOnAnOriginalBadlyHurtIsACertainReturn) {
    // CRP: the Reserves rescue applies "even if it was the original Casualty
    // roll" -- no second roll is needed to earn it. This is the strongest
    // possible use of the apothecary and must not be missed.
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).state = PlayerState::PRONE;
    gs.getPlayer(1).positionName = "Gutter Runner";
    gs.getTeamState(TeamSide::HOME).hasApothecary = true;
    // armour, injury 10, D68 = 1,1 -> Badly Hurt already
    FixedDiceRoller dice({5, 5, 5, 5, 1, 1, 6, 8});
    InjuryContext ctx;
    resolveArmourAndInjury(gs, 1, dice, ctx, nullptr);
    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::OFF_PITCH) << "back in Reserves";
    EXPECT_TRUE(gs.getTeamState(TeamSide::HOME).apothecaryUsed);
}
