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

// =============================================================
// Vrstva 4: Defensive Strategy Tests
// =============================================================

// Helper: create a defensive state (opponent has ball)
GameState makeDefensiveState() {
    GameState state;
    state.phase = GamePhase::PLAY;
    state.activeTeam = TeamSide::HOME;
    state.half = 1;
    state.homeTeam.turnNumber = 3;
    state.homeTeam.rerolls = 3;
    state.awayTeam.rerolls = 3;
    state.weather = Weather::NICE;

    // Home player 1 (free, fast)
    Player& p1 = state.getPlayer(1);
    p1.id = 1;
    p1.teamSide = TeamSide::HOME;
    p1.state = PlayerState::STANDING;
    p1.position = {5, 7};
    p1.stats = {7, 3, 3, 8};
    p1.movementRemaining = 7;
    p1.hasMoved = false;
    p1.hasActed = false;

    // Home player 2 (free)
    Player& p2 = state.getPlayer(2);
    p2.id = 2;
    p2.teamSide = TeamSide::HOME;
    p2.state = PlayerState::STANDING;
    p2.position = {6, 4};
    p2.stats = {6, 3, 3, 8};
    p2.movementRemaining = 6;
    p2.hasMoved = false;
    p2.hasActed = false;

    // Home player 3 (free)
    Player& p3 = state.getPlayer(3);
    p3.id = 3;
    p3.teamSide = TeamSide::HOME;
    p3.state = PlayerState::STANDING;
    p3.position = {4, 10};
    p3.stats = {6, 3, 3, 8};
    p3.movementRemaining = 6;
    p3.hasMoved = false;
    p3.hasActed = false;

    // Away player 12 — ball carrier at x=15
    Player& p12 = state.getPlayer(12);
    p12.id = 12;
    p12.teamSide = TeamSide::AWAY;
    p12.state = PlayerState::STANDING;
    p12.position = {15, 7};
    p12.stats = {6, 3, 3, 8};
    p12.movementRemaining = 6;

    // Ball held by away player 12
    state.ball = BallState::carried({15, 7}, 12);

    return state;
}

TEST(MacroActions, BlitzDefensePrioritizesCarrier) {
    GameState state = makeDefensiveState();
    state.homeTeam.blitzUsedThisTurn = false;

    // Add another away player far away
    Player& p13 = state.getPlayer(13);
    p13.id = 13;
    p13.teamSide = TeamSide::AWAY;
    p13.state = PlayerState::STANDING;
    p13.position = {20, 10};
    p13.stats = {6, 3, 3, 8};

    std::vector<Macro> macros;
    getAvailableMacros(state, macros);

    // First BLITZ macro should target the ball carrier (player 12)
    ASSERT_TRUE(hasMacroType(macros, MacroType::BLITZ));
    for (auto& m : macros) {
        if (m.type == MacroType::BLITZ) {
            EXPECT_EQ(m.targetId, 12) << "First BLITZ should target ball carrier";
            break;
        }
    }
}

TEST(MacroActions, BlitzDefenseMultipleTargets) {
    GameState state = makeDefensiveState();
    state.homeTeam.blitzUsedThisTurn = false;

    // Add second away player
    Player& p13 = state.getPlayer(13);
    p13.id = 13;
    p13.teamSide = TeamSide::AWAY;
    p13.state = PlayerState::STANDING;
    p13.position = {18, 5};
    p13.stats = {6, 3, 3, 8};

    std::vector<Macro> macros;
    getAvailableMacros(state, macros);

    // On defense with multiple targets, should have 2 BLITZ macros
    int blitzCount = countMacroType(macros, MacroType::BLITZ);
    EXPECT_EQ(blitzCount, 2);
}

TEST(MacroActions, BlitzOffenseSingleTarget) {
    GameState state = makeMinimalState();
    // Offense: we have the ball
    state.ball = BallState::carried({10, 7}, 1);
    state.homeTeam.blitzUsedThisTurn = false;

    // Add second away player
    Player& p13 = state.getPlayer(13);
    p13.id = 13;
    p13.teamSide = TeamSide::AWAY;
    p13.state = PlayerState::STANDING;
    p13.position = {18, 5};
    p13.stats = {6, 3, 3, 8};

    std::vector<Macro> macros;
    getAvailableMacros(state, macros);

    // On offense: only 1 BLITZ macro (best target)
    int blitzCount = countMacroType(macros, MacroType::BLITZ);
    EXPECT_EQ(blitzCount, 1);
}

TEST(MacroActions, RepositionDefenseMarksCarrier) {
    GameState state = makeDefensiveState();

    std::vector<Macro> macros;
    getAvailableMacros(state, macros);

    // One REPOSITION macro should target near the carrier position (marker)
    bool hasMarkerRepo = false;
    for (auto& m : macros) {
        if (m.type == MacroType::REPOSITION) {
            if (m.targetPos.distanceTo(state.getPlayer(12).position) <= 1) {
                hasMarkerRepo = true;
            }
        }
    }
    EXPECT_TRUE(hasMarkerRepo) << "Should have a REPOSITION targeting carrier (marker)";
}

TEST(MacroActions, RepositionDefenseSafetyPlayer) {
    GameState state = makeDefensiveState();

    std::vector<Macro> macros;
    getAvailableMacros(state, macros);

    // One REPOSITION should target near our endzone (safety)
    // Home endzone is at x=0
    bool hasSafety = false;
    for (auto& m : macros) {
        if (m.type == MacroType::REPOSITION && m.targetPos.x <= 2) {
            hasSafety = true;
        }
    }
    EXPECT_TRUE(hasSafety) << "Should have a safety REPOSITION near endzone";
}

TEST(MacroActions, RepositionDefenseEndzoneGuard) {
    GameState state = makeDefensiveState();

    // Add opponent with scoring threat (near our endzone, fast, uncontested)
    Player& p13 = state.getPlayer(13);
    p13.id = 13;
    p13.teamSide = TeamSide::AWAY;
    p13.state = PlayerState::STANDING;
    p13.position = {4, 3};  // near home endzone (x=0)
    p13.stats = {7, 3, 3, 8};  // MA 7, can score (dist=4, MA+2=9)
    p13.movementRemaining = 7;

    // Add more home players for endzone guard assignment
    Player& p4 = state.getPlayer(4);
    p4.id = 4;
    p4.teamSide = TeamSide::HOME;
    p4.state = PlayerState::STANDING;
    p4.position = {3, 12};
    p4.stats = {6, 3, 3, 8};
    p4.movementRemaining = 6;
    p4.hasMoved = false;
    p4.hasActed = false;

    Player& p5 = state.getPlayer(5);
    p5.id = 5;
    p5.teamSide = TeamSide::HOME;
    p5.state = PlayerState::STANDING;
    p5.position = {2, 2};
    p5.stats = {6, 3, 3, 8};
    p5.movementRemaining = 6;
    p5.hasMoved = false;
    p5.hasActed = false;

    std::vector<Macro> macros;
    getAvailableMacros(state, macros);

    // Should have endzone guard REPOSITION(s) targeting x=4 area (4 sq from EZ)
    bool hasGuard = false;
    for (auto& m : macros) {
        if (m.type == MacroType::REPOSITION && m.targetPos.x >= 3 && m.targetPos.x <= 5 &&
            (m.targetPos.y == 5 || m.targetPos.y == 9)) {
            hasGuard = true;
        }
    }
    EXPECT_TRUE(hasGuard) << "Should have endzone guard REPOSITION";
}

TEST(MacroExpansion, BlitzSelectsBestBlitzer) {
    GameState state = makeMinimalState();
    // Two home players that can blitz the target
    state.getPlayer(1).position = {10, 7};
    state.getPlayer(1).stats.strength = 3;
    state.getPlayer(1).movementRemaining = 6;

    Player& p2 = state.getPlayer(2);
    p2.id = 2;
    p2.teamSide = TeamSide::HOME;
    p2.state = PlayerState::STANDING;
    p2.position = {12, 7};  // closer to target
    p2.stats = {6, 4, 3, 8}; // ST4 = better dice
    p2.movementRemaining = 6;
    p2.hasMoved = false;
    p2.hasActed = false;

    state.getPlayer(12).position = {14, 7};
    state.getPlayer(12).stats.strength = 3;
    state.homeTeam.blitzUsedThisTurn = false;
    state.ball = BallState::onGround({20, 7});

    DiceRoller dice(42);
    Macro macro{MacroType::BLITZ, -1, 12, {-1, -1}};
    auto result = greedyExpandMacro(state, macro, dice);

    ASSERT_GE(result.actions.size(), 1u);
    EXPECT_EQ(result.actions[0].type, ActionType::BLITZ);
    // Should select player 2 (ST4 = 2-dice, closer)
    EXPECT_EQ(result.actions[0].playerId, 2);
}

TEST(MacroActions, DefensiveScreenEvenSpread) {
    GameState state = makeDefensiveState();

    // Add many free home players (no adjacent enemies)
    for (int i = 4; i <= 8; ++i) {
        Player& p = state.getPlayer(i);
        p.id = i;
        p.teamSide = TeamSide::HOME;
        p.state = PlayerState::STANDING;
        p.position = {static_cast<int8_t>(2 + i), static_cast<int8_t>(2)};
        p.stats = {5, 3, 3, 8};  // slow (MA5, won't get safety)
        p.movementRemaining = 5;
        p.hasMoved = false;
        p.hasActed = false;
    }

    std::vector<Macro> macros;
    getAvailableMacros(state, macros);

    // Collect screen REPOSITION Y targets (exclude safety and marker)
    std::set<int> screenYs;
    for (auto& m : macros) {
        if (m.type != MacroType::REPOSITION) continue;
        // Exclude safety (near endzone) and marker (near carrier)
        if (m.targetPos.x <= 2) continue;  // safety
        if (m.targetPos.distanceTo(state.getPlayer(12).position) <= 1) continue;  // marker
        screenYs.insert(m.targetPos.y);
    }

    // Screen should have diverse Y values (not all the same)
    EXPECT_GE(screenYs.size(), 3u) << "Screen should spread across Y values";
}

// =============================================================
// Vrstva 5: Scoring Chains Tests
// =============================================================

TEST(MacroActions, HandOffScoreAvailableWhenTeammateCanScore) {
    // Carrier stuck (far from endzone), but adjacent teammate can reach EZ
    GameState state = makeMinimalState();
    Player& carrier = state.getPlayer(1);
    carrier.position = {18, 7};
    carrier.movementRemaining = 2; // can't reach EZ (dist=7)
    state.ball = BallState::carried({18, 7}, 1);

    // Teammate adjacent (HOME slot 2), can reach endzone
    Player& teammate = state.getPlayer(2);
    teammate.id = 2;
    teammate.teamSide = TeamSide::HOME;
    teammate.state = PlayerState::STANDING;
    teammate.position = {19, 7};
    teammate.stats = {6, 3, 3, 8};
    teammate.movementRemaining = 6; // dist to EZ = 6, can reach
    teammate.hasActed = false;
    teammate.hasMoved = false;

    // Move away player out of the way
    Player& away1 = state.getPlayer(12);
    away1.position = {5, 5};

    std::vector<Macro> macros;
    getAvailableMacros(state, macros);
    EXPECT_TRUE(hasMacroType(macros, MacroType::HAND_OFF_SCORE));
}

TEST(MacroActions, HandOffScoreNotAvailableWhenCarrierCanScore) {
    // Carrier CAN reach endzone directly and isn't in TZ → no hand-off needed
    GameState state = makeScoringState();
    // Add teammate (HOME slot 2)
    Player& tm = state.getPlayer(2);
    tm.id = 2;
    tm.teamSide = TeamSide::HOME;
    tm.state = PlayerState::STANDING;
    tm.position = {24, 7};
    tm.stats = {6, 3, 3, 8};
    tm.movementRemaining = 6;
    tm.hasActed = false;
    tm.hasMoved = false;

    std::vector<Macro> macros;
    getAvailableMacros(state, macros);
    // Carrier at 23 with MA=6, no TZ → not "stuck"
    EXPECT_FALSE(hasMacroType(macros, MacroType::HAND_OFF_SCORE));
}

TEST(MacroActions, HandOffScoreNotAvailableWhenPassUsed) {
    GameState state = makeMinimalState();
    Player& carrier = state.getPlayer(1);
    carrier.position = {18, 7};
    carrier.movementRemaining = 2;
    state.ball = BallState::carried({18, 7}, 1);
    state.homeTeam.passUsedThisTurn = true;

    // HOME slot 2
    Player& teammate = state.getPlayer(2);
    teammate.id = 2;
    teammate.teamSide = TeamSide::HOME;
    teammate.state = PlayerState::STANDING;
    teammate.position = {19, 7};
    teammate.stats = {6, 3, 3, 8};
    teammate.movementRemaining = 6;
    teammate.hasActed = false;
    teammate.hasMoved = false;

    Player& away = state.getPlayer(12);
    away.position = {5, 5};

    std::vector<Macro> macros;
    getAvailableMacros(state, macros);
    EXPECT_FALSE(hasMacroType(macros, MacroType::HAND_OFF_SCORE));
}

TEST(MacroActions, HandOffScoreAvailableWhenCarrierInHeavyTZ) {
    // Carrier CAN reach endzone but is in 2+ enemy TZ (risky dodge)
    GameState state = makeMinimalState();
    Player& carrier = state.getPlayer(1);
    carrier.position = {23, 7};
    carrier.movementRemaining = 6;
    state.ball = BallState::carried({23, 7}, 1);

    // Two enemies adjacent to carrier (2+ TZ)
    Player& e1 = state.getPlayer(12);
    e1.position = {23, 6};
    e1.state = PlayerState::STANDING;

    Player& e2 = state.getPlayer(13);
    e2.id = 13;
    e2.teamSide = TeamSide::AWAY;
    e2.state = PlayerState::STANDING;
    e2.position = {23, 8};
    e2.stats = {6, 3, 3, 8};

    // Free teammate adjacent, can score (HOME slot 2)
    Player& teammate = state.getPlayer(2);
    teammate.id = 2;
    teammate.teamSide = TeamSide::HOME;
    teammate.state = PlayerState::STANDING;
    teammate.position = {24, 7};
    teammate.stats = {6, 3, 3, 8};
    teammate.movementRemaining = 6;
    teammate.hasActed = false;
    teammate.hasMoved = false;

    std::vector<Macro> macros;
    getAvailableMacros(state, macros);
    EXPECT_TRUE(hasMacroType(macros, MacroType::HAND_OFF_SCORE));
}

TEST(MacroActions, ScoreAvoidsEnemyTZ) {
    // Carrier at y=7, enemies blocking y=7 path to endzone
    // expandScore should route around (y=5 or y=9)
    GameState state = makeMinimalState();
    Player& carrier = state.getPlayer(1);
    carrier.position = {22, 7};
    carrier.movementRemaining = 6;
    state.ball = BallState::carried({22, 7}, 1);

    // Enemies blocking direct path at y=7
    Player& e1 = state.getPlayer(12);
    e1.position = {23, 7};
    e1.state = PlayerState::STANDING;

    Player& e2 = state.getPlayer(13);
    e2.id = 13;
    e2.teamSide = TeamSide::AWAY;
    e2.state = PlayerState::STANDING;
    e2.position = {24, 7};
    e2.stats = {6, 3, 3, 8};

    std::vector<Macro> macros;
    getAvailableMacros(state, macros);
    ASSERT_TRUE(hasMacroType(macros, MacroType::SCORE));

    // Expand and verify carrier moves toward endzone (should route around)
    Macro scoreMacro;
    for (auto& m : macros) {
        if (m.type == MacroType::SCORE) { scoreMacro = m; break; }
    }

    FixedDiceRoller dice({6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6});
    GameState sim = state;
    auto result = greedyExpandMacro(sim, scoreMacro, dice);

    // Should have moved (not empty expansion)
    EXPECT_FALSE(result.actions.empty());
}

TEST(MacroActions, PickupPrefersHighAG) {
    // AG4 player slightly farther should beat AG3 player closer
    GameState state = makeMinimalState();
    state.ball = BallState::onGround({13, 7});

    Player& p1 = state.getPlayer(1);
    p1.position = {12, 7}; // dist=1
    p1.stats = {6, 3, 3, 8}; // AG3

    Player& p2 = state.getPlayer(2);
    p2.id = 2;
    p2.teamSide = TeamSide::HOME;
    p2.state = PlayerState::STANDING;
    p2.position = {11, 7}; // dist=2
    p2.stats = {6, 3, 4, 7}; // AG4
    p2.movementRemaining = 6;
    p2.hasMoved = false;
    p2.hasActed = false;

    // Remove away player from path
    Player& away = state.getPlayer(12);
    away.position = {1, 1};

    std::vector<Macro> macros;
    getAvailableMacros(state, macros);
    ASSERT_TRUE(hasMacroType(macros, MacroType::PICKUP));

    for (auto& m : macros) {
        if (m.type == MacroType::PICKUP) {
            // AG4 player (id=2) should be picked over AG3 (id=1)
            // AG4: 4*10 - 2*3 = 34
            // AG3: 3*10 - 1*3 = 27
            EXPECT_EQ(m.playerId, 2);
            break;
        }
    }
}

TEST(MacroActions, PickupAdvancesAfterPickup) {
    // After picking up ball, expansion should advance toward endzone
    GameState state = makeMinimalState();
    state.ball = BallState::onGround({10, 7});

    Player& p1 = state.getPlayer(1);
    p1.position = {9, 7};
    p1.movementRemaining = 6;

    // Remove away from path
    Player& away = state.getPlayer(12);
    away.position = {1, 1};

    Macro pickup{MacroType::PICKUP, 1, -1, {10, 7}};
    // Use dice that succeed on pickup (3+ on AG3)
    FixedDiceRoller dice({6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6});
    GameState sim = state;
    auto result = greedyExpandMacro(sim, pickup, dice);

    // Should move to ball (1 step) + advance toward endzone (more steps)
    EXPECT_GT(result.actions.size(), 1u) << "Should advance after pickup";

    // Player should have moved forward (x > 10)
    const Player& p = sim.getPlayer(1);
    if (p.isOnPitch()) {
        EXPECT_GT(p.position.x, 10) << "Should advance toward endzone after pickup";
    }
}

TEST(MacroActions, HandOffScoreExpansionProducesActions) {
    // Verify expansion produces hand-off + move actions
    GameState state = makeMinimalState();
    Player& carrier = state.getPlayer(1);
    carrier.position = {20, 7};
    carrier.movementRemaining = 2;
    state.ball = BallState::carried({20, 7}, 1);

    // HOME slot 2 as receiver
    Player& receiver = state.getPlayer(2);
    receiver.id = 2;
    receiver.teamSide = TeamSide::HOME;
    receiver.state = PlayerState::STANDING;
    receiver.position = {21, 7}; // adjacent
    receiver.stats = {6, 3, 3, 8};
    receiver.movementRemaining = 6; // can reach EZ (dist=4)
    receiver.hasActed = false;
    receiver.hasMoved = false;

    // Move away player out of path
    Player& away = state.getPlayer(12);
    away.position = {1, 1};

    Macro handoff{MacroType::HAND_OFF_SCORE, 1, 2, {-1, -1}};
    FixedDiceRoller dice({6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6});
    GameState sim = state;
    auto result = greedyExpandMacro(sim, handoff, dice);

    // Should produce at least a HAND_OFF action
    bool hasHandOff = false;
    for (auto& a : result.actions) {
        if (a.type == ActionType::HAND_OFF) hasHandOff = true;
    }
    EXPECT_TRUE(hasHandOff) << "Expansion should include HAND_OFF action";
    EXPECT_FALSE(result.turnover) << "Should not turnover on success";
}

TEST(MacroActions, PassScoreAvailableWhenCarrierStuck) {
    // Carrier stuck far from EZ, teammate in pass range can score
    GameState state = makeMinimalState();
    Player& carrier = state.getPlayer(1);
    carrier.position = {10, 7};
    carrier.movementRemaining = 2; // can't reach EZ
    state.ball = BallState::carried({10, 7}, 1);

    // Teammate at pass range (dist=5), can score (dist to EZ = 5, MA=6+2GFI=8)
    Player& receiver = state.getPlayer(2);
    receiver.id = 2;
    receiver.teamSide = TeamSide::HOME;
    receiver.state = PlayerState::STANDING;
    receiver.position = {20, 7};
    receiver.stats = {6, 3, 3, 8};
    receiver.movementRemaining = 6;
    receiver.hasActed = false;
    receiver.hasMoved = false;

    Player& away = state.getPlayer(12);
    away.position = {1, 1};

    std::vector<Macro> macros;
    getAvailableMacros(state, macros);
    EXPECT_TRUE(hasMacroType(macros, MacroType::PASS_SCORE));
}

TEST(MacroActions, ChainScoreAvailableWith3Players) {
    // 3-player chain: carrier passes to relay, relay hand-offs to scorer
    GameState state = makeMinimalState();
    Player& carrier = state.getPlayer(1);
    carrier.position = {5, 7};
    carrier.movementRemaining = 2; // stuck far from EZ
    state.ball = BallState::carried({5, 7}, 1);

    // Relay player: in pass range from carrier, near scorer
    Player& relay = state.getPlayer(2);
    relay.id = 2;
    relay.teamSide = TeamSide::HOME;
    relay.state = PlayerState::STANDING;
    relay.position = {15, 7};
    relay.stats = {6, 3, 3, 8};
    relay.movementRemaining = 6;
    relay.hasActed = false;
    relay.hasMoved = false;

    // Scorer: adjacent to relay, can reach endzone
    Player& scorer = state.getPlayer(3);
    scorer.id = 3;
    scorer.teamSide = TeamSide::HOME;
    scorer.state = PlayerState::STANDING;
    scorer.position = {16, 7};
    scorer.stats = {8, 3, 3, 8}; // MA=8
    scorer.movementRemaining = 8; // dist to EZ = 9, MA+2=10
    scorer.hasActed = false;
    scorer.hasMoved = false;

    Player& away = state.getPlayer(12);
    away.position = {1, 1};

    std::vector<Macro> macros;
    getAvailableMacros(state, macros);
    EXPECT_TRUE(hasMacroType(macros, MacroType::CHAIN_SCORE))
        << "Should generate CHAIN_SCORE with 3 eligible players";
}

TEST(MacroActions, ChainScoreNotAvailableWithout3Players) {
    // Only carrier + one teammate — not enough for 3-player chain
    GameState state = makeMinimalState();
    Player& carrier = state.getPlayer(1);
    carrier.position = {5, 7};
    carrier.movementRemaining = 2;
    state.ball = BallState::carried({5, 7}, 1);

    Player& away = state.getPlayer(12);
    away.position = {1, 1};

    std::vector<Macro> macros;
    getAvailableMacros(state, macros);
    EXPECT_FALSE(hasMacroType(macros, MacroType::CHAIN_SCORE));
}

TEST(MacroActions, ReceiverSetupWhenTurnsLeft2) {
    // When 2 turns left with ball, REPOSITION should place receiver near endzone
    GameState state = makeMinimalState();
    state.homeTeam.turnNumber = 7; // turnsLeft = max(0, 9-7) = 2

    Player& carrier = state.getPlayer(1);
    carrier.position = {10, 7};
    carrier.movementRemaining = 6;
    state.ball = BallState::carried({10, 7}, 1);

    // Fast free teammate far from carrier
    Player& fast = state.getPlayer(2);
    fast.id = 2;
    fast.teamSide = TeamSide::HOME;
    fast.state = PlayerState::STANDING;
    fast.position = {15, 5};
    fast.stats = {7, 3, 3, 8}; // MA=7 (fast)
    fast.movementRemaining = 7;
    fast.hasActed = false;
    fast.hasMoved = false;

    Player& away = state.getPlayer(12);
    away.position = {1, 1};

    std::vector<Macro> macros;
    getAvailableMacros(state, macros);

    // Should have REPOSITION targeting near endzone (x >= 20)
    bool hasReceiverSetup = false;
    for (auto& m : macros) {
        if (m.type == MacroType::REPOSITION && m.playerId == 2) {
            if (m.targetPos.x >= 20) {
                hasReceiverSetup = true;
            }
        }
    }
    EXPECT_TRUE(hasReceiverSetup) << "Should place receiver near endzone when 2 turns left";
}
