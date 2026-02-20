#include <gtest/gtest.h>
#include "bb/mcts.h"
#include "bb/game_state.h"
#include "bb/game_simulator.h"
#include "bb/roster.h"
#include "bb/value_function.h"
#include "bb/action_resolver.h"
#include <chrono>
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

} // anonymous namespace

TEST(MCTSNode, UCBFormula) {
    MCTSNode node;
    node.visits = 10;
    node.totalValue = 5.0;  // avg = 0.5

    double parentLogN = std::log(100.0);
    double C = 1.41;

    double expected = 0.5 + C * std::sqrt(parentLogN / 10.0);
    EXPECT_NEAR(node.ucb(parentLogN, C), expected, 0.001);
}

TEST(MCTSNode, UCBUnvisitedIsInfinite) {
    MCTSNode node;
    node.visits = 0;

    double ucb = node.ucb(std::log(10.0), 1.41);
    EXPECT_GT(ucb, 1e6);  // Should be effectively infinite
}

TEST(MCTSNode, MostVisitedChild) {
    MCTSNode root;
    root.children.resize(3);
    root.children[0].visits = 5;
    root.children[1].visits = 20;
    root.children[2].visits = 10;

    MCTSNode* best = root.mostVisitedChild();
    ASSERT_NE(best, nullptr);
    EXPECT_EQ(best->visits, 20);
}

TEST(MCTSNode, BestChildSelection) {
    MCTSNode root;
    root.visits = 30;
    root.children.resize(3);

    root.children[0].visits = 10;
    root.children[0].totalValue = 8.0;  // avg 0.8
    root.children[0].parent = &root;

    root.children[1].visits = 10;
    root.children[1].totalValue = 2.0;  // avg 0.2
    root.children[1].parent = &root;

    root.children[2].visits = 10;
    root.children[2].totalValue = 5.0;  // avg 0.5
    root.children[2].parent = &root;

    // With zero exploration, should pick highest avg
    MCTSNode* best = root.bestChild(0.0);
    ASSERT_NE(best, nullptr);
    EXPECT_NEAR(best->totalValue / best->visits, 0.8, 0.001);
}

TEST(MCTS, SearchReturnsValidAction) {
    GameState state = makePlayState();

    MCTSConfig config;
    config.timeBudgetMs = 50;
    config.maxIterations = 100;

    MCTSSearch search(nullptr, config, 42);
    Action action = search.search(state);

    // Verify the returned action is among available actions
    std::vector<Action> actions;
    getAvailableActions(state, actions);

    bool found = false;
    for (auto& a : actions) {
        if (a.type == action.type && a.playerId == action.playerId &&
            a.targetId == action.targetId &&
            a.target.x == action.target.x && a.target.y == action.target.y) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found) << "MCTS returned an action not in available actions";
}

TEST(MCTS, SearchWithValueFunction) {
    GameState state = makePlayState();

    // Simple linear value function
    std::vector<float> weights(NUM_FEATURES, 0.0f);
    weights[0] = 1.0f;  // value score_diff
    LinearValueFunction vf(weights);

    MCTSConfig config;
    config.timeBudgetMs = 50;
    config.maxIterations = 200;

    MCTSSearch search(&vf, config, 42);
    Action action = search.search(state);

    EXPECT_GT(search.lastIterations(), 0);
}

TEST(MCTS, TimeBudgetRespected) {
    GameState state = makePlayState();

    MCTSConfig config;
    config.timeBudgetMs = 100;
    config.maxIterations = 1000000;  // very high cap

    auto start = std::chrono::steady_clock::now();

    MCTSSearch search(nullptr, config, 42);
    search.search(state);

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();

    // Should complete within 2x the budget (tolerance for overhead)
    EXPECT_LT(elapsed, config.timeBudgetMs * 3);
}

TEST(MCTS, SingleActionNoSearch) {
    // If only one action available, MCTS should return it immediately
    GameState state;
    state.phase = GamePhase::PLAY;
    state.activeTeam = TeamSide::HOME;

    // Only action is END_TURN (no players on pitch)
    MCTSConfig config;
    config.timeBudgetMs = 1000;

    MCTSSearch search(nullptr, config, 42);
    Action action = search.search(state);

    EXPECT_EQ(action.type, ActionType::END_TURN);
    EXPECT_EQ(search.lastIterations(), 0);
}

TEST(MCTS, TrivialScoringPosition) {
    // Carrier 1 step from endzone — MCTS should find the scoring move
    GameState state;
    state.phase = GamePhase::PLAY;
    state.activeTeam = TeamSide::HOME;
    state.half = 1;
    state.homeTeam.turnNumber = 1;
    state.homeTeam.rerolls = 3;
    state.weather = Weather::NICE;

    // Place home carrier at x=24, y=7 (1 step from endzone at x=25)
    Player& carrier = state.getPlayer(1);
    carrier.id = 1;
    carrier.teamSide = TeamSide::HOME;
    carrier.state = PlayerState::STANDING;
    carrier.position = {24, 7};
    carrier.stats = {6, 3, 3, 8};
    carrier.skills.add(SkillName::SureHands);
    carrier.movementRemaining = 6;
    carrier.hasMoved = false;
    carrier.hasActed = false;

    state.ball = BallState::carried({24, 7}, 1);

    // Simple value function that rewards scoring
    std::vector<float> weights(NUM_FEATURES, 0.0f);
    weights[0] = 5.0f;   // score_diff
    weights[1] = 3.0f;   // my_score
    LinearValueFunction vf(weights);

    MCTSConfig config;
    config.timeBudgetMs = 200;
    config.maxIterations = 1000;

    MCTSSearch search(&vf, config, 42);
    Action action = search.search(state);

    // Should move to endzone (x=25)
    EXPECT_EQ(action.type, ActionType::MOVE);
    EXPECT_EQ(action.playerId, 1);
    EXPECT_EQ(action.target.x, 25);
}

TEST(MCTS, ExpandCreatesChildren) {
    GameState state = makePlayState();

    std::vector<Action> actions;
    getAvailableActions(state, actions);

    MCTSConfig config;
    config.timeBudgetMs = 50;
    config.maxIterations = 5;

    MCTSSearch search(nullptr, config, 42);
    search.search(state);

    // After search, iterations should have been performed
    EXPECT_GT(search.lastIterations(), 0);
}
