#include <gtest/gtest.h>
#include "bb/action_features.h"
#include "bb/game_state.h"
#include "bb/game_simulator.h"
#include "bb/roster.h"
#include "bb/helpers.h"
#include <cmath>

using namespace bb;

namespace {

GameState makeSimpleState() {
    GameState state;
    state.phase = GamePhase::PLAY;
    state.activeTeam = TeamSide::HOME;
    state.half = 1;
    state.homeTeam.turnNumber = 1;
    state.homeTeam.rerolls = 3;
    state.weather = Weather::NICE;

    // Place one home player
    Player& p1 = state.getPlayer(1);
    p1.id = 1;
    p1.teamSide = TeamSide::HOME;
    p1.state = PlayerState::STANDING;
    p1.position = {10, 7};
    p1.stats = {6, 3, 3, 8};
    p1.movementRemaining = 6;
    p1.hasMoved = false;
    p1.hasActed = false;

    // Place one away player
    Player& p2 = state.getPlayer(12);
    p2.id = 12;
    p2.teamSide = TeamSide::AWAY;
    p2.state = PlayerState::STANDING;
    p2.position = {11, 7};
    p2.stats = {6, 3, 3, 8};
    p2.movementRemaining = 6;

    state.ball = BallState::onGround({13, 7});

    return state;
}

} // anonymous namespace

TEST(ActionFeatures, EndTurn) {
    GameState state = makeSimpleState();
    Action action;
    action.type = ActionType::END_TURN;

    float feats[NUM_ACTION_FEATURES];
    extractActionFeatures(state, action, feats);

    EXPECT_FLOAT_EQ(feats[0], 1.0f);  // is_end_turn
    EXPECT_FLOAT_EQ(feats[1], 0.0f);  // is_move
    EXPECT_FLOAT_EQ(feats[2], 0.0f);  // is_block
    EXPECT_FLOAT_EQ(feats[6], 0.0f);  // player_strength (no player)
}

TEST(ActionFeatures, MoveAction) {
    GameState state = makeSimpleState();
    Action action;
    action.type = ActionType::MOVE;
    action.playerId = 1;
    action.target = {11, 6};

    float feats[NUM_ACTION_FEATURES];
    extractActionFeatures(state, action, feats);

    EXPECT_FLOAT_EQ(feats[0], 0.0f);  // not end_turn
    EXPECT_FLOAT_EQ(feats[1], 1.0f);  // is_move
    EXPECT_FLOAT_EQ(feats[6], 3.0f / 7.0f);  // strength / 7
    EXPECT_FLOAT_EQ(feats[7], 3.0f / 7.0f);  // agility / 7
    EXPECT_FLOAT_EQ(feats[8], 0.0f);  // not ball carrier
}

TEST(ActionFeatures, BlockAction2Dice) {
    GameState state = makeSimpleState();

    // ST4 home player vs ST3 away player = 2 dice attacker
    state.getPlayer(1).stats.strength = 4;
    state.getPlayer(1).position = {10, 7};
    state.getPlayer(12).stats.strength = 3;
    state.getPlayer(12).position = {11, 7};

    Action action;
    action.type = ActionType::BLOCK;
    action.playerId = 1;
    action.targetId = 12;

    float feats[NUM_ACTION_FEATURES];
    extractActionFeatures(state, action, feats);

    EXPECT_FLOAT_EQ(feats[2], 1.0f);  // is_block
    // ST4 vs ST3: 2 dice attacker chooses → +2/3
    EXPECT_NEAR(feats[11], 2.0f / 3.0f, 0.01f);
}

TEST(ActionFeatures, BlockAction1Die) {
    GameState state = makeSimpleState();

    // ST3 vs ST3 = 1 die
    state.getPlayer(1).stats.strength = 3;
    state.getPlayer(12).stats.strength = 3;

    Action action;
    action.type = ActionType::BLOCK;
    action.playerId = 1;
    action.targetId = 12;

    float feats[NUM_ACTION_FEATURES];
    extractActionFeatures(state, action, feats);

    EXPECT_NEAR(feats[11], 1.0f / 3.0f, 0.01f);
}

TEST(ActionFeatures, ScoringMove) {
    GameState state = makeSimpleState();

    // Home carrier at x=24, moving to endzone x=25
    state.getPlayer(1).position = {24, 7};
    state.ball = BallState::carried({24, 7}, 1);

    Action action;
    action.type = ActionType::MOVE;
    action.playerId = 1;
    action.target = {25, 7};

    float feats[NUM_ACTION_FEATURES];
    extractActionFeatures(state, action, feats);

    EXPECT_FLOAT_EQ(feats[8], 1.0f);  // is_ball_carrier
    EXPECT_FLOAT_EQ(feats[9], 1.0f);  // is_scoring_move
    EXPECT_FLOAT_EQ(feats[10], 0.0f);  // distance_to_endzone (at endzone)
}

TEST(ActionFeatures, BallForward) {
    GameState state = makeSimpleState();
    state.getPlayer(1).position = {10, 7};

    // Move forward (toward x=25 for home)
    Action forward;
    forward.type = ActionType::MOVE;
    forward.playerId = 1;
    forward.target = {11, 7};

    float featsF[NUM_ACTION_FEATURES];
    extractActionFeatures(state, forward, featsF);
    EXPECT_FLOAT_EQ(featsF[12], 1.0f);  // moves_ball_forward

    // Move backward
    Action backward;
    backward.type = ActionType::MOVE;
    backward.playerId = 1;
    backward.target = {9, 7};

    float featsB[NUM_ACTION_FEATURES];
    extractActionFeatures(state, backward, featsB);
    EXPECT_FLOAT_EQ(featsB[12], 0.0f);  // not forward
}

TEST(ActionFeatures, GFIRequired) {
    GameState state = makeSimpleState();
    state.getPlayer(1).movementRemaining = 2;  // 2 MA remaining

    // Move 4 squares → 2 GFI needed
    Action action;
    action.type = ActionType::MOVE;
    action.playerId = 1;
    action.target = {14, 7};

    float feats[NUM_ACTION_FEATURES];
    extractActionFeatures(state, action, feats);

    // distanceTo uses chebyshev — (10,7) to (14,7) = 4
    // remaining 2, so gfi = 2
    EXPECT_NEAR(feats[13], 2.0f / 3.0f, 0.01f);
}

TEST(ActionFeatures, FoulTargetProne) {
    GameState state = makeSimpleState();
    state.getPlayer(12).state = PlayerState::PRONE;

    Action action;
    action.type = ActionType::FOUL;
    action.playerId = 1;
    action.targetId = 12;

    float feats[NUM_ACTION_FEATURES];
    extractActionFeatures(state, action, feats);

    EXPECT_FLOAT_EQ(feats[5], 1.0f);   // is_other (foul)
    EXPECT_FLOAT_EQ(feats[14], 1.0f);  // target_is_prone
}

TEST(ActionFeatures, BlitzWithHorns) {
    GameState state = makeSimpleState();

    // ST3 + Horns blitz vs ST3 = effectively ST4 vs ST3 = 2 dice
    state.getPlayer(1).stats.strength = 3;
    state.getPlayer(1).skills.add(SkillName::Horns);
    state.getPlayer(12).stats.strength = 3;

    Action action;
    action.type = ActionType::BLITZ;
    action.playerId = 1;
    action.targetId = 12;

    float feats[NUM_ACTION_FEATURES];
    extractActionFeatures(state, action, feats);

    EXPECT_FLOAT_EQ(feats[3], 1.0f);  // is_blitz
    // ST3+1(Horns) vs ST3 = 2 dice attacker
    EXPECT_NEAR(feats[11], 2.0f / 3.0f, 0.01f);
}
