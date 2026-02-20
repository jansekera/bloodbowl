#include <gtest/gtest.h>
#include "bb/macro_mcts.h"
#include "bb/game_state.h"
#include "bb/game_simulator.h"
#include "bb/roster.h"
#include "bb/value_function.h"
#include "bb/action_resolver.h"
#include <cmath>

using namespace bb;

namespace {

GameState makePlayState() {
    GameState state;
    setupHalf(state, getHumanRoster(), getHumanRoster());
    state.phase = GamePhase::PLAY;
    state.activeTeam = TeamSide::HOME;
    state.half = 1;
    state.homeTeam.turnNumber = 1;
    state.homeTeam.rerolls = 3;
    state.awayTeam.rerolls = 3;
    state.weather = Weather::NICE;
    state.ball = BallState::onGround({13, 7});
    return state;
}

GameState makeScoringState() {
    GameState state;
    state.phase = GamePhase::PLAY;
    state.activeTeam = TeamSide::HOME;
    state.half = 1;
    state.homeTeam.turnNumber = 1;
    state.homeTeam.rerolls = 3;
    state.weather = Weather::NICE;

    Player& carrier = state.getPlayer(1);
    carrier.id = 1;
    carrier.teamSide = TeamSide::HOME;
    carrier.state = PlayerState::STANDING;
    carrier.position = {23, 7};
    carrier.stats = {6, 3, 3, 8};
    carrier.movementRemaining = 6;
    carrier.hasMoved = false;
    carrier.hasActed = false;

    state.ball = BallState::carried({23, 7}, 1);
    return state;
}

} // anonymous namespace

// =============================================================
// MacroMCTSNode Tests
// =============================================================

TEST(MacroMCTSNode, MostVisitedChild) {
    MacroMCTSNode root;
    root.children.resize(3);
    root.children[0].visits = 5;
    root.children[1].visits = 20;
    root.children[2].visits = 10;

    auto* best = root.mostVisitedChild();
    ASSERT_NE(best, nullptr);
    EXPECT_EQ(best->visits, 20);
}

TEST(MacroMCTSNode, BestChildPUCTWithFPU) {
    MacroMCTSNode root;
    root.visits = 30;
    root.children.resize(3);

    // Two visited, one unvisited
    root.children[0].visits = 15;
    root.children[0].totalValue = 9.0;  // avg 0.6
    root.children[0].prior = 0.33f;
    root.children[0].parent = &root;

    root.children[1].visits = 15;
    root.children[1].totalValue = 3.0;  // avg 0.2
    root.children[1].prior = 0.33f;
    root.children[1].parent = &root;

    // Unvisited child should use FPU = avg(0.6, 0.2) = 0.4
    root.children[2].visits = 0;
    root.children[2].totalValue = 0.0;
    root.children[2].prior = 0.34f;
    root.children[2].parent = &root;

    auto* best = root.bestChildPUCT(2.5);
    ASSERT_NE(best, nullptr);
    // Unvisited child has high exploration bonus, should be selected
    EXPECT_EQ(best->visits, 0);
}

// =============================================================
// MacroMCTSSearch Tests
// =============================================================

TEST(MacroMCTS, SearchReturnsValidMacro) {
    GameState state = makePlayState();

    MCTSConfig config;
    config.timeBudgetMs = 0;
    config.maxIterations = 50;

    MacroMCTSSearch search(nullptr, config, 42);
    Macro result = search.search(state);

    // Verify the returned macro is among available macros
    std::vector<Macro> macros;
    getAvailableMacros(state, macros);

    bool found = false;
    for (auto& m : macros) {
        if (m.type == result.type) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

TEST(MacroMCTS, SearchWithValueFunction) {
    GameState state = makePlayState();

    std::vector<float> weights(NUM_FEATURES, 0.0f);
    weights[0] = 1.0f;
    LinearValueFunction vf(weights);

    MCTSConfig config;
    config.timeBudgetMs = 0;
    config.maxIterations = 100;

    MacroMCTSSearch search(&vf, config, 42);
    search.search(state);

    EXPECT_GT(search.lastIterations(), 0);
}

TEST(MacroMCTS, ScoringPositionFindsScore) {
    GameState state = makeScoringState();

    // Value function that rewards scoring
    std::vector<float> weights(NUM_FEATURES, 0.0f);
    weights[0] = 5.0f;  // score_diff
    weights[1] = 3.0f;  // my_score
    LinearValueFunction vf(weights);

    MCTSConfig config;
    config.timeBudgetMs = 0;
    config.maxIterations = 200;

    MacroMCTSSearch search(&vf, config, 42);
    Macro result = search.search(state);

    // Should prefer SCORE macro
    EXPECT_EQ(result.type, MacroType::SCORE);
}

TEST(MacroMCTS, ChildVisitsRecorded) {
    GameState state = makePlayState();

    MCTSConfig config;
    config.timeBudgetMs = 0;
    config.maxIterations = 50;

    MacroMCTSSearch search(nullptr, config, 42);
    search.search(state);

    const auto& childVisits = search.lastChildVisits();
    EXPECT_GT(childVisits.size(), 0u);

    int totalVisits = 0;
    for (auto& cv : childVisits) {
        EXPECT_GT(cv.visits, 0);
        totalVisits += cv.visits;
    }
    EXPECT_GT(totalVisits, 0);
}

// =============================================================
// MacroMCTSPolicy Tests
// =============================================================

TEST(MacroMCTSPolicy, ReturnsValidAction) {
    GameState state = makePlayState();

    MCTSConfig config;
    config.timeBudgetMs = 0;
    config.maxIterations = 50;

    MacroMCTSPolicy policy(nullptr, config, 42);
    Action action = policy(state);

    // Verify it's a valid action
    std::vector<Action> available;
    getAvailableActions(state, available);

    bool found = false;
    for (auto& a : available) {
        if (a.type == action.type && a.playerId == action.playerId &&
            a.targetId == action.targetId && a.target == action.target) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

TEST(MacroMCTSPolicy, PlanExecutionMultipleActions) {
    // A SCORE macro should produce multiple MOVE actions from the plan
    GameState state = makeScoringState();

    // Value function that rewards scoring
    std::vector<float> weights(NUM_FEATURES, 0.0f);
    weights[0] = 5.0f;
    weights[1] = 3.0f;
    LinearValueFunction vf(weights);

    MCTSConfig config;
    config.timeBudgetMs = 0;
    config.maxIterations = 200;

    MacroMCTSPolicy policy(&vf, config, 42);

    // First call should trigger search + return first action
    Action a1 = policy(state);
    EXPECT_EQ(a1.type, ActionType::MOVE);
    EXPECT_EQ(a1.playerId, 1);

    // Execute and call again — should continue from plan
    DiceRoller dice(42);
    executeAction(state, a1, dice, nullptr);
    Action a2 = policy(state);
    EXPECT_EQ(a2.type, ActionType::MOVE);
    EXPECT_EQ(a2.playerId, 1);
}

TEST(MacroMCTSPolicy, DecisionLogging) {
    GameState state = makePlayState();

    MCTSConfig config;
    config.timeBudgetMs = 0;
    config.maxIterations = 50;

    MacroMCTSPolicy policy(nullptr, config, 42);
    policy.setLogDecisions(true, 10);

    policy(state);

    const auto& decisions = policy.decisions();
    EXPECT_GE(decisions.size(), 1u);

    if (!decisions.empty()) {
        const auto& dec = decisions[0];
        EXPECT_GT(dec.visits.size(), 0u);
        // Visit fractions should sum to ~1
        float sum = 0;
        for (auto& v : dec.visits) sum += v.visitFraction;
        EXPECT_NEAR(sum, 1.0f, 0.1f);
    }
}

TEST(MacroMCTSPolicy, FallbackOnInvalidPlan) {
    // Test that policy gracefully handles plan invalidation
    GameState state = makePlayState();

    MCTSConfig config;
    config.timeBudgetMs = 0;
    config.maxIterations = 30;

    MacroMCTSPolicy policy(nullptr, config, 42);

    // First call
    Action a = policy(state);

    // Execute action
    DiceRoller dice(42);
    executeAction(state, a, dice, nullptr);

    // Drastically change the state to invalidate the plan
    state.activeTeam = TeamSide::AWAY;
    state.phase = GamePhase::PLAY;
    state.awayTeam.turnNumber = 1;

    // Policy should handle this gracefully (search again or use fallback)
    Action a2 = policy(state);
    // Just verify it returns something valid
    std::vector<Action> available;
    getAvailableActions(state, available);
    bool found = false;
    for (auto& av : available) {
        if (av.type == a2.type && av.playerId == a2.playerId &&
            av.targetId == a2.targetId && av.target == a2.target) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}
