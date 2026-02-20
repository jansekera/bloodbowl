#include <gtest/gtest.h>
#include "bb/move_handler.h"
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

TEST(MoveHandler, SimpleMove) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    FixedDiceRoller dice({});
    auto result = resolveMoveStep(gs, 1, {11, 7}, dice, nullptr);
    EXPECT_TRUE(result.success);
    EXPECT_FALSE(result.turnover);
    EXPECT_EQ(gs.getPlayer(1).position, (Position{11, 7}));
    EXPECT_EQ(gs.getPlayer(1).movementRemaining, 5);
}

TEST(MoveHandler, MoveToOccupiedFails) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    placePlayer(gs, 2, {11, 7}, TeamSide::HOME);
    FixedDiceRoller dice({});
    auto result = resolveMoveStep(gs, 1, {11, 7}, dice, nullptr);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(gs.getPlayer(1).position, (Position{10, 7})); // didn't move
}

TEST(MoveHandler, MoveNotAdjacentFails) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    FixedDiceRoller dice({});
    auto result = resolveMoveStep(gs, 1, {12, 7}, dice, nullptr);
    EXPECT_FALSE(result.success);
}

TEST(MoveHandler, DodgeRequired) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    placePlayer(gs, 12, {9, 7}, TeamSide::AWAY); // enemy TZ at source
    // Dodge: AG3 target 4. Roll 5 → success
    FixedDiceRoller dice({5});
    auto result = resolveMoveStep(gs, 1, {11, 7}, dice, nullptr);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(gs.getPlayer(1).position, (Position{11, 7}));
}

TEST(MoveHandler, DodgeFails) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    placePlayer(gs, 12, {9, 7}, TeamSide::AWAY);
    // Dodge: AG3 target 4. Roll 2 → fail → turnover
    // Armor: 3+3=6 ≤ 8, not broken
    FixedDiceRoller dice({2, 3, 3});
    auto result = resolveMoveStep(gs, 1, {11, 7}, dice, nullptr);
    EXPECT_FALSE(result.success);
    EXPECT_TRUE(result.turnover);
    EXPECT_EQ(gs.getPlayer(1).position, (Position{11, 7})); // fell at dest
    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::PRONE);
}

TEST(MoveHandler, DodgeRerollWithDodgeSkill) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::Dodge);
    placePlayer(gs, 12, {9, 7}, TeamSide::AWAY);
    // Dodge target 3 (AG3 + Dodge -1). Roll 2 → fail, Dodge reroll: 4 → success
    FixedDiceRoller dice({2, 4});
    auto result = resolveMoveStep(gs, 1, {11, 7}, dice, nullptr);
    EXPECT_TRUE(result.success);
}

TEST(MoveHandler, TackleNegatesDodgeReroll) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::Dodge);
    placePlayer(gs, 12, {9, 7}, TeamSide::AWAY);
    gs.getPlayer(12).skills.add(SkillName::Tackle);
    // Tackle negates Dodge reroll AND Dodge -1. Target = 4. Roll 3 → fail
    // Armor: 3+3=6
    FixedDiceRoller dice({3, 3, 3});
    auto result = resolveMoveStep(gs, 1, {11, 7}, dice, nullptr);
    EXPECT_TRUE(result.turnover);
}

TEST(MoveHandler, GFISuccess) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).movementRemaining = 0; // needs GFI
    // GFI target 2. Roll 3 → success
    FixedDiceRoller dice({3});
    auto result = resolveMoveStep(gs, 1, {11, 7}, dice, nullptr);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(gs.getPlayer(1).movementRemaining, -1);
}

TEST(MoveHandler, GFIFails) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).movementRemaining = 0;
    // GFI target 2. Roll 1 → fail → turnover
    // Armor: 3+3=6
    FixedDiceRoller dice({1, 3, 3});
    auto result = resolveMoveStep(gs, 1, {11, 7}, dice, nullptr);
    EXPECT_TRUE(result.turnover);
    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::PRONE);
}

TEST(MoveHandler, SureFeetReroll) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).movementRemaining = 0;
    gs.getPlayer(1).skills.add(SkillName::SureFeet);
    // GFI: roll 1 (fail), SureFeet reroll: 4 (success)
    FixedDiceRoller dice({1, 4});
    auto result = resolveMoveStep(gs, 1, {11, 7}, dice, nullptr);
    EXPECT_TRUE(result.success);
}

TEST(MoveHandler, GFIBlizzardTarget3) {
    GameState gs;
    gs.weather = Weather::BLIZZARD;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).movementRemaining = 0;
    // GFI target 3 in blizzard. Roll 2 → fail
    FixedDiceRoller dice({2, 3, 3}); // GFI fail + armor
    auto result = resolveMoveStep(gs, 1, {11, 7}, dice, nullptr);
    EXPECT_TRUE(result.turnover);
}

TEST(MoveHandler, SprintAllows3GFI) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::Sprint);
    gs.getPlayer(1).movementRemaining = -2; // already used 2 GFI
    // Third GFI should be allowed with Sprint
    FixedDiceRoller dice({4}); // GFI success
    auto result = resolveMoveStep(gs, 1, {11, 7}, dice, nullptr);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(gs.getPlayer(1).movementRemaining, -3);
}

TEST(MoveHandler, NoSprintLimitsTo2GFI) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).movementRemaining = -2; // already used 2 GFI, no Sprint
    FixedDiceRoller dice({});
    auto result = resolveMoveStep(gs, 1, {11, 7}, dice, nullptr);
    EXPECT_FALSE(result.success); // can't GFI a third time without Sprint
}

TEST(MoveHandler, PickupOnMove) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.ball = BallState::onGround({11, 7});
    // Move to (11,7) where ball is. Pickup: AG3 target 3. Roll 4 → success
    FixedDiceRoller dice({4});
    auto result = resolveMoveStep(gs, 1, {11, 7}, dice, nullptr);
    EXPECT_TRUE(result.success);
    EXPECT_TRUE(gs.ball.isHeld);
    EXPECT_EQ(gs.ball.carrierId, 1);
}

TEST(MoveHandler, PickupFailOnMove) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.ball = BallState::onGround({11, 7});
    // Pickup fail: roll 1 → turnover. Bounce: D8=3 → (12,7)
    FixedDiceRoller dice({1, 3});
    auto result = resolveMoveStep(gs, 1, {11, 7}, dice, nullptr);
    EXPECT_TRUE(result.turnover);
    EXPECT_FALSE(gs.ball.isHeld);
}

TEST(MoveHandler, StandUpCosts3MA) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).state = PlayerState::PRONE;
    FixedDiceRoller dice({});
    auto result = resolveStandUp(gs, 1, dice, nullptr);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::STANDING);
    EXPECT_EQ(gs.getPlayer(1).movementRemaining, 3); // 6 - 3
}

TEST(MoveHandler, StandUpNotEnoughMA) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).state = PlayerState::PRONE;
    gs.getPlayer(1).movementRemaining = 2;
    FixedDiceRoller dice({});
    auto result = resolveStandUp(gs, 1, dice, nullptr);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::PRONE); // still prone
}

TEST(MoveHandler, JumpUpFreeStandUp) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).state = PlayerState::PRONE;
    gs.getPlayer(1).skills.add(SkillName::JumpUp);
    FixedDiceRoller dice({});
    auto result = resolveStandUp(gs, 1, dice, nullptr);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::STANDING);
    EXPECT_EQ(gs.getPlayer(1).movementRemaining, 6); // no MA cost
}

TEST(MoveHandler, MoveBallCarrierUpdatesBallPos) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.ball = BallState::carried({10, 7}, 1);
    FixedDiceRoller dice({});
    auto result = resolveMoveStep(gs, 1, {11, 7}, dice, nullptr);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(gs.ball.position, (Position{11, 7}));
}
