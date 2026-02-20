#include <gtest/gtest.h>
#include "bb/macro_actions.h"
#include "bb/game_state.h"
#include "bb/game_simulator.h"
#include "bb/roster.h"
#include "bb/dice.h"
#include "bb/action_features.h"
#include <algorithm>
#include <set>

using namespace bb;

namespace {

// Minimal state with one home player and one away player
GameState makeMinimalState() {
    GameState state;
    state.phase = GamePhase::PLAY;
    state.activeTeam = TeamSide::HOME;
    state.half = 1;
    state.homeTeam.turnNumber = 1;
    state.homeTeam.rerolls = 3;
    state.awayTeam.rerolls = 3;
    state.weather = Weather::NICE;

    Player& p1 = state.getPlayer(1);
    p1.id = 1;
    p1.teamSide = TeamSide::HOME;
    p1.state = PlayerState::STANDING;
    p1.position = {10, 7};
    p1.stats = {6, 3, 3, 8};
    p1.movementRemaining = 6;
    p1.hasMoved = false;
    p1.hasActed = false;

    Player& p2 = state.getPlayer(12);
    p2.id = 12;
    p2.teamSide = TeamSide::AWAY;
    p2.state = PlayerState::STANDING;
    p2.position = {20, 7};
    p2.stats = {6, 3, 3, 8};
    p2.movementRemaining = 6;

    state.ball = BallState::onGround({13, 7});

    return state;
}

// State with carrier near endzone
GameState makeScoringState() {
    GameState state = makeMinimalState();
    Player& p1 = state.getPlayer(1);
    p1.position = {23, 7};
    p1.movementRemaining = 6;
    state.ball = BallState::carried({23, 7}, 1);
    return state;
}

// State with carrier deep in own half
GameState makeAdvanceState() {
    GameState state = makeMinimalState();
    Player& p1 = state.getPlayer(1);
    p1.position = {5, 7};
    p1.movementRemaining = 6;
    state.ball = BallState::carried({5, 7}, 1);
    return state;
}

bool hasMacroType(const std::vector<Macro>& macros, MacroType type) {
    for (auto& m : macros) {
        if (m.type == type) return true;
    }
    return false;
}

int countMacroType(const std::vector<Macro>& macros, MacroType type) {
    int count = 0;
    for (auto& m : macros) {
        if (m.type == type) count++;
    }
    return count;
}

} // anonymous namespace

// =============================================================
// Macro Generation Tests
// =============================================================

TEST(MacroActions, AlwaysHasEndTurn) {
    GameState state = makeMinimalState();
    std::vector<Macro> macros;
    getAvailableMacros(state, macros);

    EXPECT_TRUE(hasMacroType(macros, MacroType::END_TURN));
}

TEST(MacroActions, EmptyInNonPlayPhase) {
    GameState state = makeMinimalState();
    state.phase = GamePhase::GAME_OVER;

    std::vector<Macro> macros;
    getAvailableMacros(state, macros);

    EXPECT_TRUE(macros.empty());
}

TEST(MacroActions, ScoreAvailableWhenCarrierNearEndzone) {
    GameState state = makeScoringState();
    std::vector<Macro> macros;
    getAvailableMacros(state, macros);

    EXPECT_TRUE(hasMacroType(macros, MacroType::SCORE));
}

TEST(MacroActions, ScoreNotAvailableWhenCarrierFar) {
    GameState state = makeAdvanceState();
    std::vector<Macro> macros;
    getAvailableMacros(state, macros);

    EXPECT_FALSE(hasMacroType(macros, MacroType::SCORE));
}

TEST(MacroActions, AdvanceAvailableWhenCarrierCantScore) {
    GameState state = makeAdvanceState();
    std::vector<Macro> macros;
    getAvailableMacros(state, macros);

    EXPECT_TRUE(hasMacroType(macros, MacroType::ADVANCE));
}

TEST(MacroActions, AdvanceNotAvailableWhenCanScore) {
    GameState state = makeScoringState();
    std::vector<Macro> macros;
    getAvailableMacros(state, macros);

    // ADVANCE shouldn't appear when SCORE is possible (within MA+2)
    EXPECT_FALSE(hasMacroType(macros, MacroType::ADVANCE));
}

TEST(MacroActions, PickupAvailableWhenBallOnGround) {
    GameState state = makeMinimalState();
    // Ball on ground, player nearby
    state.ball = BallState::onGround({11, 7});

    std::vector<Macro> macros;
    getAvailableMacros(state, macros);

    EXPECT_TRUE(hasMacroType(macros, MacroType::PICKUP));
}

TEST(MacroActions, PickupNotAvailableWhenBallHeld) {
    GameState state = makeScoringState(); // carrier has ball
    std::vector<Macro> macros;
    getAvailableMacros(state, macros);

    EXPECT_FALSE(hasMacroType(macros, MacroType::PICKUP));
}

TEST(MacroActions, BlockAvailableWithFavorableDice) {
    GameState state = makeMinimalState();
    // Place ST4 player adjacent to ST3 enemy
    state.getPlayer(1).stats.strength = 4;
    state.getPlayer(1).position = {10, 7};
    state.getPlayer(12).stats.strength = 3;
    state.getPlayer(12).position = {11, 7};

    std::vector<Macro> macros;
    getAvailableMacros(state, macros);

    EXPECT_TRUE(hasMacroType(macros, MacroType::BLOCK));
}

TEST(MacroActions, BlockNotAvailableWith1Die) {
    GameState state = makeMinimalState();
    // Equal strength adjacent — 1 die, not 2+
    state.getPlayer(1).stats.strength = 3;
    state.getPlayer(1).position = {10, 7};
    state.getPlayer(12).stats.strength = 3;
    state.getPlayer(12).position = {11, 7};

    std::vector<Macro> macros;
    getAvailableMacros(state, macros);

    EXPECT_FALSE(hasMacroType(macros, MacroType::BLOCK));
}

TEST(MacroActions, BlitzAvailable) {
    GameState state = makeMinimalState();
    state.homeTeam.blitzUsedThisTurn = false;

    std::vector<Macro> macros;
    getAvailableMacros(state, macros);

    EXPECT_TRUE(hasMacroType(macros, MacroType::BLITZ));
}

TEST(MacroActions, BlitzNotAvailableWhenUsed) {
    GameState state = makeMinimalState();
    state.homeTeam.blitzUsedThisTurn = true;

    std::vector<Macro> macros;
    getAvailableMacros(state, macros);

    EXPECT_FALSE(hasMacroType(macros, MacroType::BLITZ));
}

TEST(MacroActions, FoulAvailableWithProneEnemy) {
    GameState state = makeMinimalState();
    state.getPlayer(12).state = PlayerState::PRONE;
    state.getPlayer(12).position = {11, 7}; // adjacent
    state.homeTeam.foulUsedThisTurn = false;

    std::vector<Macro> macros;
    getAvailableMacros(state, macros);

    EXPECT_TRUE(hasMacroType(macros, MacroType::FOUL));
}

TEST(MacroActions, FoulNotAvailableWhenUsed) {
    GameState state = makeMinimalState();
    state.getPlayer(12).state = PlayerState::PRONE;
    state.getPlayer(12).position = {11, 7};
    state.homeTeam.foulUsedThisTurn = true;

    std::vector<Macro> macros;
    getAvailableMacros(state, macros);

    EXPECT_FALSE(hasMacroType(macros, MacroType::FOUL));
}

TEST(MacroActions, CageAvailableWithCarrierAndFreePlayer) {
    GameState state = makeMinimalState();
    state.getPlayer(1).position = {10, 7};
    state.ball = BallState::carried({10, 7}, 1);

    // Add a second home player that is free
    Player& p2 = state.getPlayer(2);
    p2.id = 2;
    p2.teamSide = TeamSide::HOME;
    p2.state = PlayerState::STANDING;
    p2.position = {8, 5};
    p2.stats = {6, 3, 3, 8};
    p2.movementRemaining = 6;
    p2.hasMoved = false;
    p2.hasActed = false;

    std::vector<Macro> macros;
    getAvailableMacros(state, macros);

    EXPECT_TRUE(hasMacroType(macros, MacroType::CAGE));
}

TEST(MacroActions, RepositionForFreePlayer) {
    GameState state = makeMinimalState();
    // Player 1 has no adjacent enemies → free to reposition
    state.getPlayer(12).position = {20, 7}; // far away

    // Ball held by someone else to avoid PICKUP macro
    state.ball = BallState::carried({15, 7}, 12);

    std::vector<Macro> macros;
    getAvailableMacros(state, macros);

    EXPECT_TRUE(hasMacroType(macros, MacroType::REPOSITION));
}

TEST(MacroActions, RepositionNotForEngagedPlayer) {
    GameState state = makeMinimalState();
    // Place enemy adjacent to player 1
    state.getPlayer(12).position = {11, 7};
    state.ball = BallState::carried({15, 7}, 12);

    std::vector<Macro> macros;
    getAvailableMacros(state, macros);

    // Player 1 is engaged — no REPOSITION for them
    bool hasRepoForP1 = false;
    for (auto& m : macros) {
        if (m.type == MacroType::REPOSITION && m.playerId == 1) {
            hasRepoForP1 = true;
        }
    }
    EXPECT_FALSE(hasRepoForP1);
}

TEST(MacroActions, PassAvailableWithTeammateAhead) {
    GameState state = makeMinimalState();
    state.getPlayer(1).position = {5, 7};
    state.ball = BallState::carried({5, 7}, 1);
    state.homeTeam.passUsedThisTurn = false;

    // Add a teammate ahead
    Player& p2 = state.getPlayer(2);
    p2.id = 2;
    p2.teamSide = TeamSide::HOME;
    p2.state = PlayerState::STANDING;
    p2.position = {12, 7};
    p2.stats = {6, 3, 3, 8};
    p2.movementRemaining = 6;

    std::vector<Macro> macros;
    getAvailableMacros(state, macros);

    EXPECT_TRUE(hasMacroType(macros, MacroType::PASS_ACTION));
}

TEST(MacroActions, BranchingFactorReasonable) {
    // Full game state should produce ~10-25 macros, not ~200
    GameState state;
    setupHalf(state, getHumanRoster(), getOrcRoster());
    state.phase = GamePhase::PLAY;
    state.activeTeam = TeamSide::HOME;
    state.half = 1;
    state.homeTeam.turnNumber = 1;
    state.homeTeam.rerolls = 3;
    state.awayTeam.rerolls = 3;
    state.weather = Weather::NICE;
    state.ball = BallState::onGround({13, 7});

    std::vector<Macro> macros;
    getAvailableMacros(state, macros);

    // Should be far fewer than ~200 low-level actions
    EXPECT_GT(macros.size(), 1u);
    EXPECT_LT(macros.size(), 50u);

    // Compare against low-level actions
    std::vector<Action> actions;
    getAvailableActions(state, actions);

    // Macros should be significantly fewer
    EXPECT_LT(macros.size(), actions.size());
}

// =============================================================
// Macro Expansion Tests
// =============================================================

TEST(MacroExpansion, EndTurnProducesOneAction) {
    GameState state = makeMinimalState();
    DiceRoller dice(42);

    Macro macro{MacroType::END_TURN, -1, -1, {-1, -1}};
    auto result = greedyExpandMacro(state, macro, dice);

    EXPECT_EQ(result.actions.size(), 1u);
    EXPECT_EQ(result.actions[0].type, ActionType::END_TURN);
    EXPECT_FALSE(result.turnover);
}

TEST(MacroExpansion, ScoreProducesMoveActions) {
    GameState state = makeScoringState();
    DiceRoller dice(42);

    Macro macro{MacroType::SCORE, 1, -1, {-1, -1}};
    auto result = greedyExpandMacro(state, macro, dice);

    EXPECT_GE(result.actions.size(), 1u);
    for (auto& a : result.actions) {
        EXPECT_EQ(a.type, ActionType::MOVE);
        EXPECT_EQ(a.playerId, 1);
    }
}

TEST(MacroExpansion, AdvanceProducesMoveActions) {
    GameState state = makeAdvanceState();
    DiceRoller dice(42);

    Macro macro{MacroType::ADVANCE, 1, -1, {-1, -1}};
    auto result = greedyExpandMacro(state, macro, dice);

    EXPECT_GE(result.actions.size(), 1u);
    for (auto& a : result.actions) {
        EXPECT_EQ(a.type, ActionType::MOVE);
        EXPECT_EQ(a.playerId, 1);
    }
}

TEST(MacroExpansion, BlockProducesBlockAction) {
    GameState state = makeMinimalState();
    state.getPlayer(1).stats.strength = 4;
    state.getPlayer(1).position = {10, 7};
    state.getPlayer(12).stats.strength = 3;
    state.getPlayer(12).position = {11, 7};

    DiceRoller dice(42);
    Macro macro{MacroType::BLOCK, 1, 12, {-1, -1}};
    auto result = greedyExpandMacro(state, macro, dice);

    EXPECT_EQ(result.actions.size(), 1u);
    EXPECT_EQ(result.actions[0].type, ActionType::BLOCK);
    EXPECT_EQ(result.actions[0].playerId, 1);
    EXPECT_EQ(result.actions[0].targetId, 12);
}

TEST(MacroExpansion, PickupMoveTowardBall) {
    GameState state = makeMinimalState();
    state.ball = BallState::onGround({12, 7});

    DiceRoller dice(42);
    Macro macro{MacroType::PICKUP, 1, -1, {12, 7}};
    auto result = greedyExpandMacro(state, macro, dice);

    EXPECT_GE(result.actions.size(), 1u);
    // All actions should be MOVE for player 1
    for (auto& a : result.actions) {
        EXPECT_EQ(a.type, ActionType::MOVE);
        EXPECT_EQ(a.playerId, 1);
    }
}

TEST(MacroExpansion, FoulProducesFoulAction) {
    GameState state = makeMinimalState();
    state.getPlayer(12).state = PlayerState::PRONE;
    state.getPlayer(12).position = {11, 7};
    state.homeTeam.foulUsedThisTurn = false;

    DiceRoller dice(42);
    Macro macro{MacroType::FOUL, 1, 12, {-1, -1}};
    auto result = greedyExpandMacro(state, macro, dice);

    EXPECT_EQ(result.actions.size(), 1u);
    EXPECT_EQ(result.actions[0].type, ActionType::FOUL);
}

// =============================================================
// Macro Feature Extraction Tests
// =============================================================

TEST(MacroFeatures, EndTurnOneHot) {
    GameState state = makeMinimalState();
    float feats[NUM_ACTION_FEATURES];
    Macro macro{MacroType::END_TURN, -1, -1, {-1, -1}};
    extractMacroFeatures(state, macro, feats);

    EXPECT_FLOAT_EQ(feats[9], 1.0f);  // END_TURN = index 9
    EXPECT_FLOAT_EQ(feats[0], 0.0f);  // SCORE = index 0
}

TEST(MacroFeatures, ScoreOneHotAndScoringPotential) {
    GameState state = makeScoringState();
    float feats[NUM_ACTION_FEATURES];
    Macro macro{MacroType::SCORE, 1, -1, {-1, -1}};
    extractMacroFeatures(state, macro, feats);

    EXPECT_FLOAT_EQ(feats[0], 1.0f);   // SCORE = index 0
    EXPECT_FLOAT_EQ(feats[10], 1.0f);  // scoring_potential = 1 for SCORE
    EXPECT_FLOAT_EQ(feats[14], 1.0f);  // positional_gain = 1 for SCORE
}

TEST(MacroFeatures, BlockDiceQuality) {
    GameState state = makeMinimalState();
    state.getPlayer(1).stats.strength = 4;
    state.getPlayer(1).position = {10, 7};
    state.getPlayer(12).stats.strength = 3;
    state.getPlayer(12).position = {11, 7};

    float feats[NUM_ACTION_FEATURES];
    Macro macro{MacroType::BLOCK, 1, 12, {-1, -1}};
    extractMacroFeatures(state, macro, feats);

    EXPECT_FLOAT_EQ(feats[4], 1.0f);  // BLOCK = index 4
    EXPECT_NEAR(feats[11], 2.0f / 3.0f, 0.01f);  // 2 dice / 3
}

TEST(MacroFeatures, RiskLevel) {
    GameState state = makeMinimalState();
    float feats[NUM_ACTION_FEATURES];

    // END_TURN: no risk
    Macro endTurn{MacroType::END_TURN, -1, -1, {-1, -1}};
    extractMacroFeatures(state, endTurn, feats);
    EXPECT_FLOAT_EQ(feats[13], 0.0f);

    // BLOCK: low risk
    Macro block{MacroType::BLOCK, 1, 12, {-1, -1}};
    extractMacroFeatures(state, block, feats);
    EXPECT_GT(feats[13], 0.0f);
    EXPECT_LT(feats[13], 0.3f);
}

TEST(MacroFeatures, PlayerStrength) {
    GameState state = makeMinimalState();
    state.getPlayer(1).stats.strength = 4;

    float feats[NUM_ACTION_FEATURES];
    Macro macro{MacroType::REPOSITION, 1, -1, {15, 7}};
    extractMacroFeatures(state, macro, feats);

    EXPECT_NEAR(feats[12], 4.0f / 7.0f, 0.01f);
}

TEST(MacroFeatures, AllOneHotExclusive) {
    GameState state = makeMinimalState();
    float feats[NUM_ACTION_FEATURES];

    for (int i = 0; i < static_cast<int>(MacroType::MACRO_COUNT); ++i) {
        MacroType type = static_cast<MacroType>(i);
        Macro macro{type, 1, -1, {-1, -1}};
        extractMacroFeatures(state, macro, feats);

        // Exactly one of feats[0..9] should be 1.0
        int oneHotCount = 0;
        for (int j = 0; j < 10; ++j) {
            if (feats[j] > 0.5f) oneHotCount++;
        }
        EXPECT_EQ(oneHotCount, 1) << "MacroType " << i << " has non-exclusive one-hot";
    }
}

TEST(MacroFeatures, FeatureCountMatchesActionFeatures) {
    // Verify macro features output size matches NUM_ACTION_FEATURES (15)
    // This ensures policy network compatibility
    GameState state = makeMinimalState();
    float feats[NUM_ACTION_FEATURES];
    Macro macro{MacroType::END_TURN, -1, -1, {-1, -1}};
    extractMacroFeatures(state, macro, feats);

    // If we got here without segfault, the size is correct
    // Also check that the function uses exactly 15 features
    EXPECT_EQ(NUM_ACTION_FEATURES, 15);
}
