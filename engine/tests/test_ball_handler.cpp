#include <gtest/gtest.h>
#include "bb/ball_handler.h"
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

TEST(BallHandler, PickupSuccess) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.ball = BallState::onGround({10, 7});
    // AG3: target 3. Roll 4 → success
    FixedDiceRoller dice({4});
    bool ok = resolvePickup(gs, 1, dice, nullptr);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(gs.ball.isHeld);
    EXPECT_EQ(gs.ball.carrierId, 1);
}

TEST(BallHandler, PickupFail) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.ball = BallState::onGround({10, 7});
    // AG3: target 3. Roll 2 → fail
    FixedDiceRoller dice({2});
    bool ok = resolvePickup(gs, 1, dice, nullptr);
    EXPECT_FALSE(ok);
    EXPECT_FALSE(gs.ball.isHeld);
}

TEST(BallHandler, PickupSureHandsReroll) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::SureHands);
    gs.ball = BallState::onGround({10, 7});
    // Roll 2 (fail), SureHands reroll: 4 (success)
    FixedDiceRoller dice({2, 4});
    bool ok = resolvePickup(gs, 1, dice, nullptr);
    EXPECT_TRUE(ok);
}

TEST(BallHandler, PickupNoHands) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::NoHands);
    gs.ball = BallState::onGround({10, 7});
    FixedDiceRoller dice({6}); // doesn't matter
    bool ok = resolvePickup(gs, 1, dice, nullptr);
    EXPECT_FALSE(ok);
}

TEST(BallHandler, CatchSuccess) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.ball = BallState::onGround({10, 7});
    // AG3: target 4. Roll 5 → success
    FixedDiceRoller dice({5});
    bool ok = resolveCatch(gs, 1, dice, 0, nullptr);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(gs.ball.isHeld);
    EXPECT_EQ(gs.ball.carrierId, 1);
}

TEST(BallHandler, CatchFail) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.ball = BallState::onGround({10, 7});
    // AG3: target 4. Roll 2 → fail
    FixedDiceRoller dice({2});
    bool ok = resolveCatch(gs, 1, dice, 0, nullptr);
    EXPECT_FALSE(ok);
}

TEST(BallHandler, CatchWithModifier) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.ball = BallState::onGround({10, 7});
    // AG3, modifier +1: target = 7-3-1 = 3. Roll 3 → success
    FixedDiceRoller dice({3});
    bool ok = resolveCatch(gs, 1, dice, 1, nullptr);
    EXPECT_TRUE(ok);
}

TEST(BallHandler, BounceToEmptySquare) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    // Ball bounces from (10,7). D8=3 → East → (11,7)
    FixedDiceRoller dice({3});
    resolveBounce(gs, {10, 7}, dice, 0, nullptr);
    EXPECT_EQ(gs.ball.position, (Position{11, 7}));
    EXPECT_FALSE(gs.ball.isHeld);
}

TEST(BallHandler, BounceToPlayer) {
    GameState gs;
    placePlayer(gs, 1, {11, 7}, TeamSide::HOME);
    // Ball bounces from (10,7). D8=3 → East → (11,7) where player is
    // Catch attempt: AG3 target 4, roll 5 → success
    FixedDiceRoller dice({3, 5});
    resolveBounce(gs, {10, 7}, dice, 0, nullptr);
    EXPECT_TRUE(gs.ball.isHeld);
    EXPECT_EQ(gs.ball.carrierId, 1);
}

TEST(BallHandler, BounceToPlayerFailsCatch) {
    GameState gs;
    placePlayer(gs, 1, {11, 7}, TeamSide::HOME);
    // Bounce → (11,7), catch fail (roll 2), then bounce again
    // Second bounce: D8=3 → (12,7), no player there
    FixedDiceRoller dice({3, 2, 3});
    resolveBounce(gs, {10, 7}, dice, 0, nullptr);
    EXPECT_FALSE(gs.ball.isHeld);
    EXPECT_EQ(gs.ball.position, (Position{12, 7}));
}

TEST(BallHandler, HandleBallOnPlayerDown) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.ball = BallState::carried({10, 7}, 1);
    // Ball bounces from player's position. D8=3 → (11,7)
    FixedDiceRoller dice({3});
    handleBallOnPlayerDown(gs, 1, dice, nullptr);
    EXPECT_FALSE(gs.ball.isHeld);
    EXPECT_EQ(gs.ball.position, (Position{11, 7}));
}

TEST(BallHandler, HandleBallOnPlayerDownNotCarrier) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.ball = BallState::carried({12, 7}, 2);
    // Player 1 doesn't have ball — no effect
    FixedDiceRoller dice({});
    handleBallOnPlayerDown(gs, 1, dice, nullptr);
    EXPECT_TRUE(gs.ball.isHeld);
    EXPECT_EQ(gs.ball.carrierId, 2);
}

TEST(BallHandler, ThrowIn) {
    GameState gs;
    // Ball thrown in from edge. D8=3 (East), distance 2D6=3+2=5
    // From (0,7): direction E, 5 squares → (5,7)
    FixedDiceRoller dice({3, 3, 2});
    resolveThrowIn(gs, {0, 7}, dice, nullptr);
    EXPECT_EQ(gs.ball.position, (Position{5, 7}));
    EXPECT_FALSE(gs.ball.isHeld);
}
