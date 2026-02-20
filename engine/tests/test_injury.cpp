#include <gtest/gtest.h>
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

TEST(Injury, DecayTakesWorseRoll) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).state = PlayerState::PRONE;
    gs.getPlayer(1).skills.add(SkillName::Decay);
    // Armor: 5+4=9. Injury roll 1: 3+3=6 (stunned). Decay roll 2: 5+4=9 (KO).
    // Takes worse: 9 → KO
    FixedDiceRoller dice({5, 4, 3, 3, 5, 4});
    InjuryContext ctx;
    ctx.hasDecay = true;
    bool broken = resolveArmourAndInjury(gs, 1, dice, ctx, nullptr);
    EXPECT_TRUE(broken);
    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::KO);
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
