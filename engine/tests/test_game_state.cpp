#include <gtest/gtest.h>
#include "bb/game_state.h"

using namespace bb;

TEST(GameState, InitialState) {
    GameState gs;
    EXPECT_EQ(gs.half, 1);
    EXPECT_EQ(gs.phase, GamePhase::COIN_TOSS);
    EXPECT_EQ(gs.weather, Weather::NICE);
    EXPECT_EQ(gs.homeTeam.side, TeamSide::HOME);
    EXPECT_EQ(gs.awayTeam.side, TeamSide::AWAY);
}

TEST(GameState, PlayerLookupById) {
    GameState gs;

    // Home team: IDs 1-11
    EXPECT_EQ(gs.getPlayer(1).id, 1);
    EXPECT_EQ(gs.getPlayer(1).teamSide, TeamSide::HOME);
    EXPECT_EQ(gs.getPlayer(11).id, 11);
    EXPECT_EQ(gs.getPlayer(11).teamSide, TeamSide::HOME);

    // Away team: IDs 12-22
    EXPECT_EQ(gs.getPlayer(12).id, 12);
    EXPECT_EQ(gs.getPlayer(12).teamSide, TeamSide::AWAY);
    EXPECT_EQ(gs.getPlayer(22).id, 22);
    EXPECT_EQ(gs.getPlayer(22).teamSide, TeamSide::AWAY);

    // Out of range
    EXPECT_THROW(gs.getPlayer(0), std::out_of_range);
    EXPECT_THROW(gs.getPlayer(23), std::out_of_range);
}

TEST(GameState, PlayerAtPosition) {
    GameState gs;
    auto& p = gs.getPlayer(5);
    p.state = PlayerState::STANDING;
    p.position = {10, 7};

    auto* found = gs.getPlayerAtPosition({10, 7});
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->id, 5);

    // No player at empty position
    EXPECT_EQ(gs.getPlayerAtPosition({0, 0}), nullptr);

    // Off-pitch players not found even at their position
    auto& p2 = gs.getPlayer(6);
    p2.state = PlayerState::KO;
    p2.position = {5, 5};
    EXPECT_EQ(gs.getPlayerAtPosition({5, 5}), nullptr);
}

TEST(GameState, TeamStateLookup) {
    GameState gs;
    gs.homeTeam.score = 2;
    gs.awayTeam.score = 1;

    EXPECT_EQ(gs.getTeamState(TeamSide::HOME).score, 2);
    EXPECT_EQ(gs.getTeamState(TeamSide::AWAY).score, 1);
}

TEST(GameState, ForEachPlayer) {
    GameState gs;
    int count = 0;
    gs.forEachPlayer(TeamSide::HOME, [&](Player& p) {
        EXPECT_EQ(p.teamSide, TeamSide::HOME);
        ++count;
    });
    EXPECT_EQ(count, 11);

    count = 0;
    gs.forEachPlayer(TeamSide::AWAY, [&](Player& p) {
        EXPECT_EQ(p.teamSide, TeamSide::AWAY);
        ++count;
    });
    EXPECT_EQ(count, 11);
}

TEST(GameState, ForEachOnPitch) {
    GameState gs;
    // Put 3 home players on pitch
    gs.getPlayer(1).state = PlayerState::STANDING;
    gs.getPlayer(2).state = PlayerState::PRONE;
    gs.getPlayer(3).state = PlayerState::STANDING;

    int count = 0;
    gs.forEachOnPitch(TeamSide::HOME, [&](Player&) { ++count; });
    EXPECT_EQ(count, 3);
}

TEST(GameState, ResetPlayersForNewTurn) {
    GameState gs;
    auto& p = gs.getPlayer(1);
    p.state = PlayerState::STUNNED;
    p.stats.movement = 6;
    p.hasMoved = true;
    p.hasActed = true;
    p.lostTacklezones = true;
    p.proUsedThisTurn = true;
    p.movementRemaining = 0;

    gs.resetPlayersForNewTurn(TeamSide::HOME);

    EXPECT_EQ(p.state, PlayerState::PRONE);  // stunned → prone
    EXPECT_FALSE(p.hasMoved);
    EXPECT_FALSE(p.hasActed);
    EXPECT_FALSE(p.lostTacklezones);
    EXPECT_FALSE(p.proUsedThisTurn);
    EXPECT_EQ(p.movementRemaining, 6);
}

TEST(GameState, Clone) {
    GameState gs;
    gs.getPlayer(1).state = PlayerState::STANDING;
    gs.getPlayer(1).position = {10, 7};
    gs.homeTeam.score = 3;

    auto clone = gs.clone();

    // Clone has same data
    EXPECT_EQ(clone.getPlayer(1).position, (Position{10, 7}));
    EXPECT_EQ(clone.homeTeam.score, 3);

    // Modifying clone doesn't affect original
    clone.getPlayer(1).position = {5, 5};
    clone.homeTeam.score = 99;
    EXPECT_EQ(gs.getPlayer(1).position, (Position{10, 7}));
    EXPECT_EQ(gs.homeTeam.score, 3);
}
