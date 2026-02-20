#include <gtest/gtest.h>
#include "bb/feature_extractor.h"
#include "bb/game_state.h"
#include "bb/roster.h"
#include "bb/game_simulator.h"
#include "bb/helpers.h"
#include <cmath>

using namespace bb;

namespace {

// Helper: create a basic game state with players set up
GameState makeSetupState() {
    GameState state;
    setupHalf(state, getHumanRoster(), getHumanRoster());
    state.phase = GamePhase::PLAY;
    state.activeTeam = TeamSide::HOME;
    state.half = 1;
    state.homeTeam.turnNumber = 1;
    state.homeTeam.rerolls = 3;
    state.awayTeam.rerolls = 3;
    state.weather = Weather::NICE;
    // Place ball on ground at center
    state.ball = BallState::onGround({13, 7});
    return state;
}

} // anonymous namespace

TEST(FeatureExtractor, EmptyStateNoNaN) {
    GameState state;
    state.phase = GamePhase::PLAY;
    state.activeTeam = TeamSide::HOME;
    state.weather = Weather::NICE;

    float features[NUM_FEATURES];
    extractFeatures(state, TeamSide::HOME, features);

    for (int i = 0; i < NUM_FEATURES; ++i) {
        EXPECT_FALSE(std::isnan(features[i])) << "Feature " << i << " is NaN";
        EXPECT_FALSE(std::isinf(features[i])) << "Feature " << i << " is Inf";
    }
}

TEST(FeatureExtractor, ScoreDiffNormalization) {
    GameState state;
    state.phase = GamePhase::PLAY;
    state.activeTeam = TeamSide::HOME;
    state.homeTeam.score = 3;
    state.awayTeam.score = 1;

    float features[NUM_FEATURES];
    extractFeatures(state, TeamSide::HOME, features);

    // score_diff = (3-1)/6 = 0.333...
    EXPECT_NEAR(features[0], 2.0f / 6.0f, 0.001f);
    // my_score = 3/4 = 0.75
    EXPECT_NEAR(features[1], 0.75f, 0.001f);
    // opp_score = 1/4 = 0.25
    EXPECT_NEAR(features[2], 0.25f, 0.001f);
}

TEST(FeatureExtractor, ScoreDiffClamped) {
    GameState state;
    state.phase = GamePhase::PLAY;
    state.homeTeam.score = 10;
    state.awayTeam.score = 0;

    float features[NUM_FEATURES];
    extractFeatures(state, TeamSide::HOME, features);

    // Should be clamped to 1.0
    EXPECT_LE(features[0], 1.0f);
    EXPECT_GE(features[0], -1.0f);
}

TEST(FeatureExtractor, PlayerCountFeatures) {
    GameState state = makeSetupState();

    float features[NUM_FEATURES];
    extractFeatures(state, TeamSide::HOME, features);

    // Both teams have 11 standing players
    EXPECT_NEAR(features[4], 1.0f, 0.001f);  // my_standing = 11/11
    EXPECT_NEAR(features[5], 1.0f, 0.001f);  // opp_standing = 11/11
    EXPECT_NEAR(features[6], 0.0f, 0.001f);  // my_ko = 0
    EXPECT_NEAR(features[7], 0.0f, 0.001f);  // opp_ko = 0
    EXPECT_NEAR(features[8], 0.0f, 0.001f);  // my_injured = 0
    EXPECT_NEAR(features[9], 0.0f, 0.001f);  // opp_injured = 0
}

TEST(FeatureExtractor, BallPossessionIHaveBall) {
    GameState state = makeSetupState();

    // Give ball to home player 1
    Player& p = state.getPlayer(1);
    state.ball = BallState::carried(p.position, p.id);

    float features[NUM_FEATURES];
    extractFeatures(state, TeamSide::HOME, features);

    EXPECT_NEAR(features[12], 1.0f, 0.001f);  // i_have_ball
    EXPECT_NEAR(features[13], 0.0f, 0.001f);  // opp_has_ball
    EXPECT_NEAR(features[14], 0.0f, 0.001f);  // ball_on_ground
}

TEST(FeatureExtractor, BallPossessionOppHasBall) {
    GameState state = makeSetupState();

    // Give ball to away player 12
    Player& p = state.getPlayer(12);
    state.ball = BallState::carried(p.position, p.id);

    float features[NUM_FEATURES];
    extractFeatures(state, TeamSide::HOME, features);

    EXPECT_NEAR(features[12], 0.0f, 0.001f);  // i_have_ball
    EXPECT_NEAR(features[13], 1.0f, 0.001f);  // opp_has_ball
    EXPECT_NEAR(features[14], 0.0f, 0.001f);  // ball_on_ground
}

TEST(FeatureExtractor, BallOnGround) {
    GameState state = makeSetupState();

    float features[NUM_FEATURES];
    extractFeatures(state, TeamSide::HOME, features);

    EXPECT_NEAR(features[12], 0.0f, 0.001f);  // i_have_ball
    EXPECT_NEAR(features[13], 0.0f, 0.001f);  // opp_has_ball
    EXPECT_NEAR(features[14], 1.0f, 0.001f);  // ball_on_ground
}

TEST(FeatureExtractor, PerspectiveSymmetry) {
    GameState state = makeSetupState();

    // Give ball to home player
    Player& hp = state.getPlayer(1);
    state.ball = BallState::carried(hp.position, hp.id);

    float homeFeatures[NUM_FEATURES];
    float awayFeatures[NUM_FEATURES];
    extractFeatures(state, TeamSide::HOME, homeFeatures);
    extractFeatures(state, TeamSide::AWAY, awayFeatures);

    // Home's i_have_ball should be 1, away's should be 0
    EXPECT_NEAR(homeFeatures[12], 1.0f, 0.001f);
    EXPECT_NEAR(awayFeatures[12], 0.0f, 0.001f);

    // Home's opp_has_ball should be 0, away's should be 1
    EXPECT_NEAR(homeFeatures[13], 0.0f, 0.001f);
    EXPECT_NEAR(awayFeatures[13], 1.0f, 0.001f);

    // Home's my_standing == Away's opp_standing
    EXPECT_NEAR(homeFeatures[4], awayFeatures[5], 0.001f);
    EXPECT_NEAR(homeFeatures[5], awayFeatures[4], 0.001f);
}

TEST(FeatureExtractor, CarrierDistanceToTouchdown) {
    GameState state;
    state.phase = GamePhase::PLAY;
    state.activeTeam = TeamSide::HOME;

    // Place home player at x=20 (5 squares from endzone at x=25)
    Player& p = state.getPlayer(1);
    p.id = 1;
    p.teamSide = TeamSide::HOME;
    p.state = PlayerState::STANDING;
    p.position = {20, 7};
    p.stats = {6, 3, 3, 8};

    state.ball = BallState::carried({20, 7}, 1);

    float features[NUM_FEATURES];
    extractFeatures(state, TeamSide::HOME, features);

    // dist to endzone = 25-20 = 5, normalized = 5/26 ≈ 0.192
    EXPECT_NEAR(features[15], 5.0f / 26.0f, 0.01f);
}

TEST(FeatureExtractor, WeatherFeatures) {
    GameState state;
    state.phase = GamePhase::PLAY;

    float features[NUM_FEATURES];

    state.weather = Weather::NICE;
    extractFeatures(state, TeamSide::HOME, features);
    EXPECT_NEAR(features[24], 1.0f, 0.001f);
    EXPECT_NEAR(features[25], 0.0f, 0.001f);
    EXPECT_NEAR(features[26], 0.0f, 0.001f);

    state.weather = Weather::POURING_RAIN;
    extractFeatures(state, TeamSide::HOME, features);
    EXPECT_NEAR(features[24], 0.0f, 0.001f);
    EXPECT_NEAR(features[25], 1.0f, 0.001f);
    EXPECT_NEAR(features[26], 0.0f, 0.001f);

    state.weather = Weather::BLIZZARD;
    extractFeatures(state, TeamSide::HOME, features);
    EXPECT_NEAR(features[24], 0.0f, 0.001f);
    EXPECT_NEAR(features[25], 0.0f, 0.001f);
    EXPECT_NEAR(features[26], 1.0f, 0.001f);
}

TEST(FeatureExtractor, BiasAlwaysOne) {
    GameState state;
    state.phase = GamePhase::PLAY;

    float features[NUM_FEATURES];
    extractFeatures(state, TeamSide::HOME, features);
    EXPECT_NEAR(features[29], 1.0f, 0.001f);
}

TEST(FeatureExtractor, RerollFeatures) {
    GameState state;
    state.phase = GamePhase::PLAY;
    state.homeTeam.rerolls = 3;
    state.awayTeam.rerolls = 1;

    float features[NUM_FEATURES];
    extractFeatures(state, TeamSide::HOME, features);
    EXPECT_NEAR(features[10], 0.75f, 0.001f);  // 3/4
    EXPECT_NEAR(features[11], 0.25f, 0.001f);  // 1/4
}

TEST(FeatureExtractor, TurnProgress) {
    GameState state;
    state.phase = GamePhase::PLAY;
    state.half = 1;
    state.homeTeam.turnNumber = 4;

    float features[NUM_FEATURES];
    extractFeatures(state, TeamSide::HOME, features);
    // (4 + (1-1)*8) / 16 = 4/16 = 0.25
    EXPECT_NEAR(features[3], 0.25f, 0.001f);

    state.half = 2;
    extractFeatures(state, TeamSide::HOME, features);
    // (4 + (2-1)*8) / 16 = 12/16 = 0.75
    EXPECT_NEAR(features[3], 0.75f, 0.001f);
}

TEST(FeatureExtractor, SkillFractions) {
    GameState state = makeSetupState();

    float features[NUM_FEATURES];
    extractFeatures(state, TeamSide::HOME, features);

    // Human roster: 4 blitzers with Block out of 11 standing = ~0.36
    // (depending on exact roster build)
    // Block skill fraction should be > 0 since blitzers have Block
    EXPECT_GE(features[48], 0.0f);
    EXPECT_LE(features[48], 1.0f);
}

TEST(FeatureExtractor, ScoringThreat) {
    GameState state;
    state.phase = GamePhase::PLAY;
    state.activeTeam = TeamSide::HOME;

    // Place home player at x=22 with MA=6 (3 squares from endzone)
    Player& p = state.getPlayer(1);
    p.id = 1;
    p.teamSide = TeamSide::HOME;
    p.state = PlayerState::STANDING;
    p.position = {22, 7};
    p.stats = {6, 3, 3, 8};

    state.ball = BallState::carried({22, 7}, 1);

    float features[NUM_FEATURES];
    extractFeatures(state, TeamSide::HOME, features);

    // Distance to endzone = 25-22 = 3, MA = 6 >= 3 → scoring threat
    EXPECT_NEAR(features[41], 1.0f, 0.001f);
    // Carrier near endzone (dist <= 3)
    EXPECT_NEAR(features[34], 1.0f, 0.001f);
}
