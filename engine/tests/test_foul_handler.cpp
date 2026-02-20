#include <gtest/gtest.h>
#include "bb/foul_handler.h"
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

TEST(FoulHandler, BasicFoulArmourNotBroken) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    gs.getPlayer(12).state = PlayerState::PRONE;
    // Armor: 3+3=6 ≤ 8, not broken. Not doubles.
    FixedDiceRoller dice({3, 4});
    auto result = resolveFoul(gs, 1, 12, dice, nullptr);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(gs.getPlayer(12).state, PlayerState::PRONE); // unchanged
    EXPECT_TRUE(gs.getPlayer(1).hasActed);
    EXPECT_TRUE(gs.homeTeam.foulUsedThisTurn);
}

TEST(FoulHandler, FoulBreaksArmour) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    gs.getPlayer(12).state = PlayerState::PRONE;
    // Armor: 5+4=9 > 8, broken. Injury: 3+3=6 → stunned
    FixedDiceRoller dice({5, 4, 3, 3});
    auto result = resolveFoul(gs, 1, 12, dice, nullptr);
    EXPECT_EQ(gs.getPlayer(12).state, PlayerState::STUNNED);
}

TEST(FoulHandler, DirtyPlayerBonus) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::DirtyPlayer);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    gs.getPlayer(12).state = PlayerState::PRONE;
    // Armor: 4+4+1(DP)=9 > 8, broken. Injury: 3+3=6 → stunned
    FixedDiceRoller dice({4, 4, 3, 3});
    auto result = resolveFoul(gs, 1, 12, dice, nullptr);
    EXPECT_EQ(gs.getPlayer(12).state, PlayerState::STUNNED);
}

TEST(FoulHandler, DoublesEjection) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    gs.getPlayer(12).state = PlayerState::PRONE;
    // Armor: 3+3=6 (doubles!), not broken. Fouler ejected.
    FixedDiceRoller dice({3, 3});
    auto result = resolveFoul(gs, 1, 12, dice, nullptr);
    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::EJECTED);
}

TEST(FoulHandler, SneakyGitPreventsEjection) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::SneakyGit);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    gs.getPlayer(12).state = PlayerState::PRONE;
    // Armor: 3+3=6 (doubles), not broken. SneakyGit prevents ejection.
    FixedDiceRoller dice({3, 3});
    auto result = resolveFoul(gs, 1, 12, dice, nullptr);
    EXPECT_NE(gs.getPlayer(1).state, PlayerState::EJECTED);
}

TEST(FoulHandler, FoulAssists) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME); // fouler
    placePlayer(gs, 2, {11, 6}, TeamSide::HOME); // assist (adj to target, no enemy TZ)
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    gs.getPlayer(12).state = PlayerState::PRONE;
    // +1 assist. Armor: 4+3+1=8 ≤ 8, not broken (need > AV)
    FixedDiceRoller dice({4, 3});
    auto result = resolveFoul(gs, 1, 12, dice, nullptr);
    EXPECT_EQ(gs.getPlayer(12).state, PlayerState::PRONE);
}

TEST(FoulHandler, FoulOnStunnedTarget) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    gs.getPlayer(12).state = PlayerState::STUNNED;
    // Valid target
    FixedDiceRoller dice({5, 4, 3, 3}); // broken + stunned
    auto result = resolveFoul(gs, 1, 12, dice, nullptr);
    EXPECT_TRUE(result.success);
}

TEST(FoulHandler, FoulOnStandingFails) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    FixedDiceRoller dice({});
    auto result = resolveFoul(gs, 1, 12, dice, nullptr);
    EXPECT_FALSE(result.success);
}
