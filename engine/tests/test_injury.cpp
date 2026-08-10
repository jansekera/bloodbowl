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
    // AV8: 5+4=9 > 8. Injury: 5+5=10 → casualty
    FixedDiceRoller dice({5, 4, 5, 5});
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

TEST(Injury, ThickSkullSavesFromKO) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).state = PlayerState::PRONE;
    gs.getPlayer(1).skills.add(SkillName::ThickSkull);
    // Armor: 5+4=9 > 8. Injury: 4+5=9 → KO range. ThickSkull: 4 → saves
    FixedDiceRoller dice({5, 4, 4, 5, 4});
    InjuryContext ctx;
    bool broken = resolveArmourAndInjury(gs, 1, dice, ctx, nullptr);
    EXPECT_TRUE(broken);
    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::STUNNED);
}

TEST(Injury, ThickSkullFails) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).state = PlayerState::PRONE;
    gs.getPlayer(1).skills.add(SkillName::ThickSkull);
    // Armor: 5+4=9. Injury: 4+4=8 → KO. ThickSkull: 3 → fails
    FixedDiceRoller dice({5, 4, 4, 4, 3});
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
    // Armor: 5+5=10. Injury: 5+5=10 → casualty. Regen: 4 → saves
    FixedDiceRoller dice({5, 5, 5, 5, 4});
    InjuryContext ctx;
    bool broken = resolveArmourAndInjury(gs, 1, dice, ctx, nullptr);
    EXPECT_TRUE(broken);
    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::STUNNED);
}

TEST(Injury, StakesBlocksRegeneration) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).state = PlayerState::PRONE;
    gs.getPlayer(1).skills.add(SkillName::Regeneration);
    // Armor: 5+5=10. Injury: 5+5=10. Stakes blocks regen.
    FixedDiceRoller dice({5, 5, 5, 5});
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
