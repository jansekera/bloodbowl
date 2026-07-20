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

int countMacroType(const std::vector<Macro>& macros, MacroType type) {
    int count = 0;
    for (auto& m : macros) {
        if (m.type == type) count++;
    }
    return count;
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

    auto* best = root.bestChildPUCT(2.5, /*maximize=*/true);
    ASSERT_NE(best, nullptr);
    // Unvisited child has high exploration bonus, should be selected
    EXPECT_EQ(best->visits, 0);
}

TEST(MacroMCTSNode, BestChildPUCTMinimizeForOpponentNode) {
    // When this node represents the OPPONENT's decision (maximize=false),
    // ranking should flip: with no exploration bonus advantage, the child
    // with the WORST value for searchingSide (i.e. best for the opponent)
    // should be favored over one with a better value.
    MacroMCTSNode root;
    root.visits = 30;
    root.children.resize(2);

    root.children[0].visits = 15;
    root.children[0].totalValue = 12.0;  // avg 0.8 -- great for searchingSide
    root.children[0].prior = 0.5f;
    root.children[0].parent = &root;

    root.children[1].visits = 15;
    root.children[1].totalValue = -12.0;  // avg -0.8 -- bad for searchingSide
    root.children[1].prior = 0.5f;
    root.children[1].parent = &root;

    auto* bestCooperative = root.bestChildPUCT(0.0, /*maximize=*/true);
    ASSERT_NE(bestCooperative, nullptr);
    EXPECT_DOUBLE_EQ(bestCooperative->totalValue, 12.0);

    auto* bestAdversarial = root.bestChildPUCT(0.0, /*maximize=*/false);
    ASSERT_NE(bestAdversarial, nullptr);
    EXPECT_DOUBLE_EQ(bestAdversarial->totalValue, -12.0);
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

// =============================================================
// Item 10 (proposals_item10_prior_floor_validation_20260714): defensive
// prior-floor rebalance -- REPOSITION floor 0.05->0.08, new FOUL cap 0.08
// (onDef-gated). Ratios survive the shared renorm, so assertions compare
// ratios, not absolute priors; each test asserts its candidate count n
// first since the floor/cap binding thresholds depend on it.
// =============================================================

TEST(MacroMCTS, DefensiveRepositionFloorBindsAtLargeNodes) {
    GameState state;
    state.phase = GamePhase::PLAY;
    state.activeTeam = TeamSide::HOME;
    state.half = 1;
    state.homeTeam.turnNumber = 3;
    state.homeTeam.rerolls = 3;
    state.awayTeam.rerolls = 3;
    state.weather = Weather::NICE;

    Player& carrier = state.getPlayer(12);
    carrier.id = 12;
    carrier.teamSide = TeamSide::AWAY;
    carrier.state = PlayerState::STANDING;
    carrier.position = {5, 7};
    carrier.stats = {6, 3, 3, 8};
    carrier.movementRemaining = 6;
    state.ball = BallState::carried({5, 7}, 12);

    Player& away2 = state.getPlayer(13);
    away2.id = 13;
    away2.teamSide = TeamSide::AWAY;
    away2.state = PlayerState::STANDING;
    away2.position = {6, 9};
    away2.stats = {6, 3, 3, 8};
    away2.movementRemaining = 6;

    // 14 free HOME players -> 14 REPOSITION + 2 BLITZ + END_TURN.
    int id = 1;
    for (int i = 0; i < 14; ++i) {
        Player& p = state.getPlayer(id);
        p.id = id;
        p.teamSide = TeamSide::HOME;
        p.state = PlayerState::STANDING;
        int x = 18 + (i % 3);
        int y = std::min(1 + i, 13);
        p.position = {static_cast<int8_t>(x), static_cast<int8_t>(y)};
        p.stats = {6, 3, 3, 8};
        p.movementRemaining = 6;
        id++;
        if (id == 12) id = 14; // skip reserved ids 12/13
    }

    PolicyNetwork zeroPolicy;
    MCTSConfig cfg;
    cfg.policy = &zeroPolicy;
    cfg.policyBlend = 0.0f;
    MacroMCTSSearch search(nullptr, cfg, 42);
    auto priors = search.expandRootPriorsForTest(state);
    ASSERT_GE(priors.size(), 13u);
    ASSERT_LE(priors.size(), 20u);

    float repoPrior = -1, blitzPrior = -1;
    for (auto& [m, prior] : priors) {
        if (m.type == MacroType::REPOSITION) repoPrior = prior;
        if (m.type == MacroType::BLITZ) blitzPrior = prior;
    }
    ASSERT_GE(repoPrior, 0.0f);
    ASSERT_GE(blitzPrior, 0.0f);
    // Post-patch: 0.08/0.20 = 0.400. Pre-patch (negative control): REPOSITION
    // is raw uniform (1/14 ~= 0.0714, the old 0.05 floor doesn't bind at
    // n=14) -> ratio ~= 0.357, outside this tolerance.
    EXPECT_NEAR(repoPrior / blitzPrior, 0.08f / 0.20f, 0.02f);
}

TEST(MacroMCTS, DefensiveFoulCapBindsAtSparseNodes) {
    GameState state;
    state.phase = GamePhase::PLAY;
    state.activeTeam = TeamSide::HOME;
    state.half = 1;
    state.homeTeam.turnNumber = 3;
    state.homeTeam.rerolls = 3;
    state.awayTeam.rerolls = 3;
    state.weather = Weather::NICE;

    Player& carrier = state.getPlayer(12);
    carrier.id = 12;
    carrier.teamSide = TeamSide::AWAY;
    carrier.state = PlayerState::STANDING;
    carrier.position = {5, 7};
    carrier.stats = {6, 3, 3, 8};
    carrier.movementRemaining = 6;
    state.ball = BallState::carried({5, 7}, 12);

    Player& prone = state.getPlayer(13);
    prone.id = 13;
    prone.teamSide = TeamSide::AWAY;
    prone.state = PlayerState::PRONE;
    prone.position = {15, 8};

    Player& blockTarget = state.getPlayer(14);
    blockTarget.id = 14;
    blockTarget.teamSide = TeamSide::AWAY;
    blockTarget.state = PlayerState::STANDING;
    blockTarget.position = {18, 4};
    blockTarget.stats = {6, 3, 3, 8};
    blockTarget.movementRemaining = 6;

    Player& fouler = state.getPlayer(1);
    fouler.id = 1;
    fouler.teamSide = TeamSide::HOME;
    fouler.state = PlayerState::STANDING;
    fouler.position = {15, 7};
    fouler.stats = {6, 3, 3, 8};
    fouler.movementRemaining = 6;

    Player& blocker = state.getPlayer(2);
    blocker.id = 2;
    blocker.teamSide = TeamSide::HOME;
    blocker.state = PlayerState::STANDING;
    blocker.position = {18, 5};
    blocker.stats = {6, 4, 3, 8};
    blocker.movementRemaining = 6;

    // 2 free HOME REPOSITION fillers -> n=8: 2 BLITZ, 1 BLOCK, 1 FOUL, 3
    // REPOSITION, 1 END_TURN. At n=8, raw uniform 1/8=0.125 is ABOVE both
    // the old 0.05 REPOSITION floor and BLOCK's 0.12 floor (neither binds),
    // so pre-patch FOUL and BLOCK are exactly equal (both raw uniform).
    for (int i = 0; i < 2; ++i) {
        int id = 3 + i;
        Player& p = state.getPlayer(id);
        p.id = id;
        p.teamSide = TeamSide::HOME;
        p.state = PlayerState::STANDING;
        p.position = {static_cast<int8_t>(20 + i), static_cast<int8_t>(1 + i)};
        p.stats = {6, 3, 3, 8};
        p.movementRemaining = 6;
    }

    PolicyNetwork zeroPolicy;
    MCTSConfig cfg;
    cfg.policy = &zeroPolicy;
    cfg.policyBlend = 0.0f;
    MacroMCTSSearch search(nullptr, cfg, 42);
    auto priors = search.expandRootPriorsForTest(state);
    ASSERT_GE(priors.size(), 6u);
    ASSERT_LE(priors.size(), 12u);

    float foulPrior = -1, blitzPrior = -1, blockPrior = -1;
    for (auto& [m, prior] : priors) {
        if (m.type == MacroType::FOUL) foulPrior = prior;
        if (m.type == MacroType::BLITZ) blitzPrior = prior;
        if (m.type == MacroType::BLOCK) blockPrior = prior;
    }
    ASSERT_GE(foulPrior, 0.0f);
    ASSERT_GE(blitzPrior, 0.0f);
    ASSERT_GE(blockPrior, 0.0f);
    // Post-patch: 0.08/0.20 = 0.400. Pre-patch (negative control): FOUL is
    // raw uniform (1/8 = 0.125, uncapped) -> ratio 0.625, outside tolerance.
    EXPECT_NEAR(foulPrior / blitzPrior, 0.08f / 0.20f, 0.02f);
    // Pre-patch: FOUL == BLOCK (both raw uniform, EXPECT_LT fails).
    // Post-patch: FOUL capped to 0.08 < BLOCK's uncapped ~0.125 -> passes.
    EXPECT_LT(foulPrior, blockPrior);
}

TEST(MacroMCTS, RepositionFloorNoOpAtSmallNodes) {
    GameState state;
    state.phase = GamePhase::PLAY;
    state.activeTeam = TeamSide::HOME;
    state.half = 1;
    state.homeTeam.turnNumber = 3;
    state.homeTeam.rerolls = 3;
    state.awayTeam.rerolls = 3;
    state.weather = Weather::NICE;

    Player& carrier = state.getPlayer(12);
    carrier.id = 12;
    carrier.teamSide = TeamSide::AWAY;
    carrier.state = PlayerState::STANDING;
    carrier.position = {5, 7};
    carrier.stats = {6, 3, 3, 8};
    carrier.movementRemaining = 6;
    state.ball = BallState::carried({5, 7}, 12);

    Player& away2 = state.getPlayer(13);
    away2.id = 13;
    away2.teamSide = TeamSide::AWAY;
    away2.state = PlayerState::STANDING;
    away2.position = {6, 9};
    away2.stats = {6, 3, 3, 8};
    away2.movementRemaining = 6;

    // 3 free HOME players, no FOUL candidate (no prone/stunned enemy) ->
    // n=6: 2 BLITZ, 3 REPOSITION, 1 END_TURN.
    for (int i = 0; i < 3; ++i) {
        int id = 1 + i;
        Player& p = state.getPlayer(id);
        p.id = id;
        p.teamSide = TeamSide::HOME;
        p.state = PlayerState::STANDING;
        p.position = {static_cast<int8_t>(20 + i), static_cast<int8_t>(1 + i)};
        p.stats = {6, 3, 3, 8};
        p.movementRemaining = 6;
    }

    PolicyNetwork zeroPolicy;
    MCTSConfig cfg;
    cfg.policy = &zeroPolicy;
    cfg.policyBlend = 0.0f;
    MacroMCTSSearch search(nullptr, cfg, 42);
    auto priors = search.expandRootPriorsForTest(state);
    ASSERT_LE(priors.size(), 12u);
    int n = static_cast<int>(priors.size());

    float repoPrior = -1, blitzPrior = -1;
    for (auto& [m, prior] : priors) {
        if (m.type == MacroType::REPOSITION) repoPrior = prior;
        if (m.type == MacroType::BLITZ) blitzPrior = prior;
    }
    ASSERT_GE(repoPrior, 0.0f);
    ASSERT_GE(blitzPrior, 0.0f);
    // At n<=12, raw uniform 1/n >= 0.0833 > 0.08, so the new floor never
    // binds -- priors must be identical to the pre-patch code (no-op).
    EXPECT_NEAR(repoPrior / blitzPrior, (1.0f / n) / 0.20f, 1e-4f);
}

TEST(MacroMCTS, OffensivePriorsUntouchedByDefensiveRebalance) {
    GameState state;
    state.phase = GamePhase::PLAY;
    state.activeTeam = TeamSide::HOME;
    state.half = 1;
    state.homeTeam.turnNumber = 3;
    state.homeTeam.rerolls = 3;
    state.awayTeam.rerolls = 3;
    state.weather = Weather::NICE;

    Player& carrier = state.getPlayer(1);
    carrier.id = 1;
    carrier.teamSide = TeamSide::HOME;
    carrier.state = PlayerState::STANDING;
    carrier.position = {12, 7};
    carrier.stats = {6, 3, 3, 8};
    carrier.movementRemaining = 6;
    state.ball = BallState::carried({12, 7}, 1);

    Player& prone = state.getPlayer(12);
    prone.id = 12;
    prone.teamSide = TeamSide::AWAY;
    prone.state = PlayerState::PRONE;
    prone.position = {18, 8};

    Player& fouler = state.getPlayer(2);
    fouler.id = 2;
    fouler.teamSide = TeamSide::HOME;
    fouler.state = PlayerState::STANDING;
    fouler.position = {18, 7};
    fouler.stats = {6, 3, 3, 8};
    fouler.movementRemaining = 6;

    Player& free1 = state.getPlayer(3);
    free1.id = 3;
    free1.teamSide = TeamSide::HOME;
    free1.state = PlayerState::STANDING;
    free1.position = {20, 2};
    free1.stats = {6, 3, 3, 8};
    free1.movementRemaining = 6;

    PolicyNetwork zeroPolicy;
    MCTSConfig cfg;
    cfg.policy = &zeroPolicy;
    cfg.policyBlend = 0.0f;
    MacroMCTSSearch search(nullptr, cfg, 42);
    auto priors = search.expandRootPriorsForTest(state);

    float repoPrior = -1, foulPrior = -1;
    for (auto& [m, prior] : priors) {
        if (m.type == MacroType::REPOSITION) repoPrior = prior;
        if (m.type == MacroType::FOUL) foulPrior = prior;
    }
    ASSERT_GE(repoPrior, 0.0f);
    ASSERT_GE(foulPrior, 0.0f);
    // onDef=false -> neither the REPOSITION floor nor the FOUL cap applies;
    // both stay raw uniform, identical pre- and post-patch.
    EXPECT_NEAR(repoPrior, foulPrior, 1e-6f);
}

// Item 7 prior-floor split: with two PICKUP candidates the secondary must
// carry HALF the primary's floor, so after renormalization the primary's
// root prior is exactly 2x the secondary's. Requires n >= 11 candidates so
// both floors (0.20 / 0.10) actually bind against the 1/n base.
// NEGATIVE CONTROL is two-stage (see protocol): before the generation
// patch this test fails at the ASSERT (no second PICKUP child exists);
// after generation but before the floor split it fails at EXPECT_NEAR
// with ratio == 1.0 -- the naive doubling this split is designed to avoid.
TEST(MacroMCTS, SecondaryPickupPriorIsHalfOfPrimary) {
    GameState state;
    state.phase = GamePhase::PLAY;
    state.activeTeam = TeamSide::HOME;
    state.half = 1;
    state.homeTeam.turnNumber = 2;   // turnsRemaining 7 > 3, scoreDiff 0 -> floor 0.20
    state.homeTeam.rerolls = 3;
    state.awayTeam.rerolls = 3;
    state.weather = Weather::NICE;
    state.ball = BallState::onGround({13, 7});

    // 10 free HOME players -> ~10 REPOSITION + 2 PICKUP + END_TURN, n >= 11.
    auto mk = [&](int id, Position pos) {
        Player& p = state.getPlayer(id);
        p.id = id;
        p.teamSide = TeamSide::HOME;
        p.state = PlayerState::STANDING;
        p.position = pos;
        p.stats = {6, 3, 3, 8};
        p.movementRemaining = 6;
        p.hasMoved = false;
        p.hasActed = false;
    };
    mk(1, {12, 7});                       // dist 1 -> score 27 (primary)
    mk(2, {10, 7});                       // dist 3 -> score 21 (secondary, gap 6)
    for (int i = 3; i <= 9; ++i) mk(i, {2, static_cast<int8_t>(2 * i - 5)});
    mk(10, {4, 7});                       // all i>=3: dist > MA+2 from the ball
    // One AWAY player far from everyone (no BLOCK/BLITZ/FOUL candidates).
    Player& away = state.getPlayer(12);
    away.id = 12;
    away.teamSide = TeamSide::AWAY;
    away.state = PlayerState::STANDING;
    away.position = {25, 13};
    away.stats = {6, 3, 3, 8};
    away.movementRemaining = 6;

    // Precondition: floors must bind (base 1/n < 0.10) and both pickers emit.
    std::vector<Macro> macros;
    getAvailableMacros(state, macros);
    ASSERT_GE(macros.size(), 11u);
    ASSERT_EQ(countMacroType(macros, MacroType::PICKUP), 2);

    PolicyNetwork pn;  // zero weights -- activates the heuristic floor block
    MCTSConfig cfg;
    cfg.policy = &pn;          // policyBlend stays 0.0 (production regime)
    cfg.maxIterations = 400;   // enough to visit every root child
    cfg.timeBudgetMs = 10000;
    MacroMCTSSearch search(nullptr, cfg, 42);
    search.search(state);

    float primary = -1.0f, secondary = -1.0f;
    for (const auto& cv : search.lastChildVisits()) {
        if (cv.macro.type != MacroType::PICKUP) continue;
        if (cv.macro.playerId == 1) primary = cv.prior;
        if (cv.macro.playerId == 2) secondary = cv.prior;
    }
    ASSERT_GT(primary, 0.0f);
    ASSERT_GT(secondary, 0.0f);
    EXPECT_NEAR(primary / secondary, 2.0f, 0.01f);
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
        // Visit fractions of top-K should sum to a large portion (but <1.0 if >topK children)
        float sum = 0;
        for (auto& v : dec.visits) sum += v.visitFraction;
        EXPECT_NEAR(sum, 1.0f, 0.3f);
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
