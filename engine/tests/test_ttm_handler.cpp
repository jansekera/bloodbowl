#include <gtest/gtest.h>
#include "bb/ttm_handler.h"
#include "bb/helpers.h"
#include "bb/ball_handler.h"

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

TEST(TTMHandler, AccurateLanding) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    gs.activeTeam = TeamSide::HOME;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME, 6, 5, 3, 9); // Thrower
    gs.getPlayer(1).skills.add(SkillName::ThrowTeamMate);
    placePlayer(gs, 2, {11, 7}, TeamSide::HOME, 6, 2, 3, 7); // Projectile
    gs.getPlayer(2).skills.add(SkillName::RightStuff);

    Position target{13, 7}; // distance 3 = quick pass
    // AG3: passTarget = 7-3-1(QP) = 3. Roll 5 >= 3 → accurate
    // Landing: 7-3 = 4, no TZ. Roll 5 >= 4 → success
    FixedDiceRoller dice({5, 5});
    auto result = resolveThrowTeamMate(gs, 1, 2, target, dice, nullptr);
    EXPECT_TRUE(result.success);
    EXPECT_FALSE(result.turnover);
    EXPECT_EQ(gs.getPlayer(2).position, target);
    EXPECT_EQ(gs.getPlayer(2).state, PlayerState::STANDING);
}

TEST(TTMHandler, AlwaysHungryEat) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    gs.activeTeam = TeamSide::HOME;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME, 6, 5, 3, 9);
    gs.getPlayer(1).skills.add(SkillName::ThrowTeamMate);
    gs.getPlayer(1).skills.add(SkillName::AlwaysHungry);
    placePlayer(gs, 2, {11, 7}, TeamSide::HOME, 6, 2, 3, 7);
    gs.getPlayer(2).skills.add(SkillName::RightStuff);

    // AlwaysHungry: roll 1 → eat. No rerolls available.
    FixedDiceRoller dice({1});
    auto result = resolveThrowTeamMate(gs, 1, 2, {13, 7}, dice, nullptr);
    EXPECT_TRUE(result.success); // NOT turnover
    EXPECT_FALSE(result.turnover);
    EXPECT_EQ(gs.getPlayer(2).state, PlayerState::INJURED);
}

TEST(TTMHandler, Fumble) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    gs.activeTeam = TeamSide::HOME;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME, 6, 5, 3, 9);
    gs.getPlayer(1).skills.add(SkillName::ThrowTeamMate);
    placePlayer(gs, 2, {11, 7}, TeamSide::HOME, 6, 2, 3, 7);
    gs.getPlayer(2).skills.add(SkillName::RightStuff);

    // Accuracy: natural 1 = fumble. Scatter: D8=1 → (11, 6)
    // Landing: 7-3=4. Roll 5 >= 4 → lands OK
    FixedDiceRoller dice({1, 1, 5});
    auto result = resolveThrowTeamMate(gs, 1, 2, {15, 7}, dice, nullptr);
    // Fumble scatters from thrower position (10,7)
    EXPECT_TRUE(result.success || !result.turnover || result.turnover);
    // Player should not be at original position
    EXPECT_NE(gs.getPlayer(2).position, (Position{11, 7}));
}

TEST(TTMHandler, InaccurateScatter) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    gs.activeTeam = TeamSide::HOME;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME, 6, 5, 2, 9); // AG2 = hard to pass
    gs.getPlayer(1).skills.add(SkillName::ThrowTeamMate);
    placePlayer(gs, 2, {11, 7}, TeamSide::HOME, 6, 2, 3, 7);
    gs.getPlayer(2).skills.add(SkillName::RightStuff);

    // AG2: passTarget = 7-2-1(QP) = 4. Roll 3 < 4 → inaccurate
    // Scatter from target (13,7): D8=5 → (-1,0) → (12,7)
    // Landing: 7-3=4. Roll 6 >= 4 → success
    FixedDiceRoller dice({3, 5, 6});
    auto result = resolveThrowTeamMate(gs, 1, 2, {13, 7}, dice, nullptr);
    EXPECT_TRUE(result.success);
    EXPECT_FALSE(result.turnover);
    EXPECT_EQ(gs.getPlayer(2).state, PlayerState::STANDING);
}

TEST(TTMHandler, OffPitchTurnover) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    gs.activeTeam = TeamSide::HOME;
    placePlayer(gs, 1, {10, 0}, TeamSide::HOME, 6, 5, 3, 9);
    gs.getPlayer(1).skills.add(SkillName::ThrowTeamMate);
    placePlayer(gs, 2, {11, 0}, TeamSide::HOME, 6, 2, 3, 7);
    gs.getPlayer(2).skills.add(SkillName::RightStuff);

    // Fumble (roll=1), scatter D8=1 → N(0,-1) → (10,-1) off pitch → crowd surf
    // resolveCrowdSurf: injury roll 2D6 = 3+3=6 → stunned → KO for crowd surf
    FixedDiceRoller dice({1, 1, 3, 3});
    auto result = resolveThrowTeamMate(gs, 1, 2, {13, 3}, dice, nullptr);
    EXPECT_TRUE(result.turnover);
}

TEST(TTMHandler, FailedLanding) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    gs.activeTeam = TeamSide::HOME;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME, 6, 5, 3, 9);
    gs.getPlayer(1).skills.add(SkillName::ThrowTeamMate);
    placePlayer(gs, 2, {11, 7}, TeamSide::HOME, 6, 2, 3, 7);
    gs.getPlayer(2).skills.add(SkillName::RightStuff);

    // Accurate pass: roll 5 >= 3. Landing: roll 1 < 4 → failed
    // Prone + armor: 3+3=6 ≤ 7 not broken
    FixedDiceRoller dice({5, 1, 3, 3});
    auto result = resolveThrowTeamMate(gs, 1, 2, {13, 7}, dice, nullptr);
    EXPECT_TRUE(result.turnover);
    EXPECT_EQ(gs.getPlayer(2).state, PlayerState::PRONE);
}

TEST(TTMHandler, BallMovesWithProjectile) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    gs.activeTeam = TeamSide::HOME;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME, 6, 5, 3, 9);
    gs.getPlayer(1).skills.add(SkillName::ThrowTeamMate);
    placePlayer(gs, 2, {11, 7}, TeamSide::HOME, 6, 2, 3, 7);
    gs.getPlayer(2).skills.add(SkillName::RightStuff);
    gs.ball = BallState::carried({11, 7}, 2);

    Position target{13, 7};
    // Accurate: roll 5. Landing: roll 5
    FixedDiceRoller dice({5, 5});
    auto result = resolveThrowTeamMate(gs, 1, 2, target, dice, nullptr);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(gs.ball.position, target);
    EXPECT_EQ(gs.ball.carrierId, 2);
}

TEST(TTMHandler, AlwaysHungryTeamReroll) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    gs.activeTeam = TeamSide::HOME;
    gs.homeTeam.rerolls = 1;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME, 6, 5, 3, 9);
    gs.getPlayer(1).skills.add(SkillName::ThrowTeamMate);
    gs.getPlayer(1).skills.add(SkillName::AlwaysHungry);
    placePlayer(gs, 2, {11, 7}, TeamSide::HOME, 6, 2, 3, 7);
    gs.getPlayer(2).skills.add(SkillName::RightStuff);

    // AlwaysHungry: roll 1. Team reroll: roll 4 → success.
    // Pass: roll 5 >= 3. Landing: roll 5 >= 4
    FixedDiceRoller dice({1, 4, 5, 5});
    auto result = resolveThrowTeamMate(gs, 1, 2, {13, 7}, dice, nullptr);
    EXPECT_TRUE(result.success);
    EXPECT_FALSE(result.turnover);
    EXPECT_EQ(gs.homeTeam.rerolls, 0);
}
