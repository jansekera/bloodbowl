#include <gtest/gtest.h>
#include "bb/gaze_handler.h"
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

TEST(GazeHandler, SuccessLosesTZ) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::HypnoticGaze);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);

    // No TZ on gazer (only target is adjacent) → target = min(6, 2+1) = 3
    // Roll 3 >= 3 → success
    FixedDiceRoller dice({3});
    auto result = resolveHypnoticGaze(gs, 1, 12, dice, nullptr);
    EXPECT_TRUE(result.success);
    EXPECT_FALSE(result.turnover);
    EXPECT_TRUE(gs.getPlayer(12).lostTacklezones);
    EXPECT_TRUE(gs.getPlayer(1).hasActed);
}

TEST(GazeHandler, FailureTurnover) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::HypnoticGaze);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);

    // TZ = 1 from target → gaze target = min(6, 2+1) = 3
    // Roll 2 < 3 → fail → turnover
    FixedDiceRoller dice({2});
    auto result = resolveHypnoticGaze(gs, 1, 12, dice, nullptr);
    EXPECT_TRUE(result.turnover);
    EXPECT_FALSE(gs.getPlayer(12).lostTacklezones);
}

TEST(GazeHandler, TZModifier) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::HypnoticGaze);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY); // +1 TZ
    placePlayer(gs, 13, {10, 6}, TeamSide::AWAY); // +1 TZ
    placePlayer(gs, 14, {10, 8}, TeamSide::AWAY); // +1 TZ

    // 3 enemy TZs on gazer → target = min(6, 2+3) = 5
    // Roll 4 < 5 → fail
    FixedDiceRoller dice({4});
    auto result = resolveHypnoticGaze(gs, 1, 12, dice, nullptr);
    EXPECT_TRUE(result.turnover);
}

TEST(GazeHandler, GazerActed) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::HypnoticGaze);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);

    // Success
    FixedDiceRoller dice({6});
    resolveHypnoticGaze(gs, 1, 12, dice, nullptr);
    EXPECT_TRUE(gs.getPlayer(1).hasActed);
}
