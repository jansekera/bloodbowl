#include <gtest/gtest.h>
#include "bb/macro_mcts.h"
#include "bb/game_state.h"
#include "bb/game_simulator.h"
#include "bb/roster.h"
#include "bb/value_function.h"
#include "bb/action_resolver.h"
#include "bb/policy_network.h"
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

// =============================================================
// Item 10 risk-deferral fixtures: two real turn-start states mined and
// deeply validated by diag_risk_sequencing_harness.cpp (worktree
// .claude/worktrees/agent-afb72a985ab5cf7f3/scratchpad/risk_sequencing_20260724/,
// project_bloodbowl_item10_risk_sequencing_result_20260724) -- ported here
// (not re-derived) so the exact validated Q-guard defer/no-defer behavior
// is pinned by a real regression test, not just a diagnostic run.
// =============================================================

struct RiskSnap { int id, x, y, st; };

GameState makeRiskSeqState(const char* homeRace, const char* awayRace, TeamSide active,
                           int half, int turn, int homeScore, int awayScore,
                           int ballX, int ballY, int carrierId,
                           const std::vector<RiskSnap>& snaps,
                           const std::vector<int>& koIds) {
    GameState s;
    const TeamRoster* home = getDevelopedRoster(homeRace, 1200);
    const TeamRoster* away = getDevelopedRoster(awayRace, 1200);
    setupHalf(s, *home, *away, TeamSide::AWAY);

    s.half = half;
    s.phase = GamePhase::PLAY;
    s.activeTeam = active;
    s.weather = Weather::NICE;
    s.kickingTeam = TeamSide::AWAY;
    s.turnoverPending = false;
    s.currentActivationId = -1;

    s.homeTeam.score = homeScore; s.awayTeam.score = awayScore;
    s.homeTeam.turnNumber = turn; s.awayTeam.turnNumber = turn;
    s.homeTeam.rerolls = 2; s.awayTeam.rerolls = 2;
    s.homeTeam.rerollUsedThisTurn = s.awayTeam.rerollUsedThisTurn = false;
    s.homeTeam.blitzUsedThisTurn = s.awayTeam.blitzUsedThisTurn = false;
    s.homeTeam.passUsedThisTurn = s.awayTeam.passUsedThisTurn = false;
    s.homeTeam.foulUsedThisTurn = s.awayTeam.foulUsedThisTurn = false;

    for (auto& p : s.players) {
        p.state = PlayerState::OFF_PITCH;
        p.position = {0, 0};
        p.hasMoved = p.hasActed = p.usedBlitz = false;
        p.lostTacklezones = p.proUsedThisTurn = false;
    }
    for (const auto& sn : snaps) {
        Player& p = s.getPlayer(sn.id);
        p.state = (sn.st == 0) ? PlayerState::STANDING
                : (sn.st == 1) ? PlayerState::PRONE : PlayerState::STUNNED;
        p.position = {static_cast<int8_t>(sn.x), static_cast<int8_t>(sn.y)};
        p.movementRemaining = p.stats.movement;
    }
    for (int id : koIds) s.getPlayer(id).state = PlayerState::KO;

    s.ball.position = {static_cast<int8_t>(ballX), static_cast<int8_t>(ballY)};
    if (carrierId > 0) {
        s.ball.isHeld = true;
        s.ball.carrierId = carrierId;
    } else {
        s.ball.isHeld = false;
        s.ball.carrierId = -1;
    }
    return s;
}

// S3 (diag_risk_sequencing_harness.cpp states_risk_seq.inc): a DODGE-first
// real turnover state where 5 REPOSITION alternatives sit within Q_MARGIN
// of the risky BLITZ's Q -- Stage 5 measured turnover 32.5%->7.0%, value
// +0.0424 (+6.2 SE), the cleanest validated Q-guard win in the corpus.
GameState makeS3State() {
    return makeRiskSeqState("wood-elf", "human", TeamSide::AWAY, 1, 4, 0, 0, 15, 7, 10,
        {{1,13,8,0},{2,15,12,0},{3,16,7,0},{4,14,8,0},{5,14,9,0},{6,15,6,0},{7,14,11,0},{8,14,10,0},
         {9,13,10,0},{10,15,7,0},{11,10,10,0},{13,14,6,1},{14,11,11,1},{15,13,5,1},{16,16,6,1},
         {17,16,9,0},{18,13,11,1},{19,16,10,0},{20,19,11,0},{21,17,7,1},{22,19,9,0}},
        {12});
}

// S7 (diag_risk_sequencing_harness.cpp states_risk_seq.inc): a PICKUP-first
// midgame turnover state where the risky PICKUP has a large genuine Q edge
// (~0.25) over every safe alternative -- Stage 5 measured turnover
// unchanged (17.0%->17.0%), value delta exactly 0 SE: the Q-guard must
// refuse to defer here, reproducing production exactly (this is the
// negative control that rules out the naive "always defer risky" rule,
// which lost -25.9 SE on this same state).
GameState makeS7State() {
    return makeRiskSeqState("human", "orc", TeamSide::HOME, 2, 6, 1, 0, 5, 8, -1,
        {{1,6,2,0},{2,11,6,1},{3,13,8,1},{4,12,7,1},{5,10,6,1},{6,3,4,0},{7,8,2,0},{8,8,7,1},
         {9,5,10,0},{10,5,7,0},{11,5,5,0},{12,14,4,1},{13,15,10,0},{14,6,9,0},{15,10,7,0},
         {16,13,3,1},{17,15,8,0},{18,11,7,0},{19,13,7,0},{20,14,8,0},{21,14,7,0},{22,4,9,1}},
        {});
}

MCTSConfig makeRiskSeqConfig(PolicyNetwork* zeroPolicy, bool riskDeferral) {
    MCTSConfig cfg;
    cfg.maxIterations = 100;
    cfg.timeBudgetMs = 0;
    cfg.explorationC = 1.0;
    cfg.dirichletAlpha = 0.0f;
    cfg.vfBlend = 0.0f;
    cfg.nRollouts = 1;
    cfg.policy = zeroPolicy;      // non-null only to activate heuristic prior
    cfg.policyBlend = 0.0f;       // floors (matches production replay config);
                                   // blend=0 means the (zero) weights are never used.
    cfg.riskDeferral = riskDeferral;
    return cfg;
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
// Item 10 (project_bloodbowl_item10_risk_sequencing_result_20260724):
// Q-guarded risk-sequencing defer, config_.riskDeferral (off by default).
// =============================================================

TEST(MacroMCTS, RiskDeferralOffMatchesBaselineOnS3) {
    // Baseline regression guard: riskDeferral=false must reproduce exactly
    // production's undeferred pick (BLITZ) on S3 -- the feature is fully
    // inert unless explicitly enabled.
    GameState state = makeS3State();
    PolicyNetwork zeroPolicy;
    MCTSConfig cfg = makeRiskSeqConfig(&zeroPolicy, /*riskDeferral=*/false);
    MacroMCTSSearch search(nullptr, cfg, 42);
    Macro result = search.search(state);
    EXPECT_EQ(result.type, MacroType::BLITZ);
}

TEST(MacroMCTS, RiskDeferralNoLongerNeededOnS3AfterItem14) {
    // S3 was originally the cleanest validated Q-guard win in the corpus
    // (Stage 5, 200 paired seeds: with riskDeferral=true the search used to
    // defer away from BLITZ to a safe REPOSITION, turnover 32.5%->7.0%,
    // value +6.2 SE). That Stage-5 measurement predates item 14
    // (project_bloodbowl_item14_blitzer_selection_ignores_path_risk_20260728):
    // expandBlitz picked S3's blitzer by dice+raw-distance alone, with zero
    // regard for the mover's agility/Dodge or tackle-zone density along the
    // approach -- the same root cause diagnosed on S2. With item 14's fix
    // (estimateBlitzFailChance, picking the lowest combined block+approach
    // fail probability), S3's BLITZ itself now resolves to a materially
    // safer blitzer/approach, so its true Q no longer sits within Q_MARGIN
    // of the safe alternatives -- the guard correctly finds nothing left to
    // defer from. This is item 14 fixing the root cause item 10 was
    // mitigating downstream, not a Q-guard regression: riskDeferral=true
    // must now match the undeferred BLITZ pick, same as riskDeferral=false.
    //
    // The Stage-5 numbers above are now stale for S3 specifically; item 10's
    // Q-guard mechanism itself is unaffected (still exercised by S7 below,
    // and by RiskDeferralOffMatchesBaselineOnS3's regression guard) and a
    // fresh Stage-5-style re-measurement is queued separately, not required
    // to land this fix.
    //
    // ⭐ 31.08.2026, M13: TENHLE TEST JE TED DUKAZ, ZE VYPINAC FUNGUJE.
    // Behem dne M13 (lezici smi deklarovat akci) prostor tahu na S3 zmenila
    // tak, ze Q-guard zase odkladal a vracel REPOSITION -- S3 ma v aktivnim
    // tymu SEST lezicich. Na zadost uzivatele ("zkus to rozdelit na dve mereni
    // kazde zvlast") sedi M13 od 31.08. za `setProneActionArm`, default OFF.
    // ⇒ S VYPNUTYM RAMENEM se S3 vraci PRESNE k puvodni volbe BLITZ.
    //   Kdyby tenhle test zacal padat, znamena to, ze do produkce proteklo
    //   neco, co melo zustat za vypinacem -- a to je cennejsi hlidka nez
    //   puvodni tvrzeni o item 14.
    GameState state = makeS3State();
    PolicyNetwork zeroPolicy;
    MCTSConfig cfg = makeRiskSeqConfig(&zeroPolicy, /*riskDeferral=*/true);
    MacroMCTSSearch search(nullptr, cfg, 42);
    Macro result = search.search(state);
    EXPECT_EQ(result.type, MacroType::BLITZ)
        << "S3 se nevratil k puvodni volbe => neco proteklo mimo vypinac";
}

TEST(MacroMCTS, RiskDeferralRefusesToDeferOnS7BigQEdge) {
    // S7: the risky PICKUP has a genuine ~0.25 Q edge over every safe
    // alternative -- the state that exposed naive "always defer risky" as
    // overcorrecting (-25.9 SE). The Q-guard must recognize the margin and
    // execute the risky pick anyway, exactly reproducing production
    // (Stage 5: turnover unchanged 17.0%->17.0%, value delta 0 SE).
    GameState state = makeS7State();
    PolicyNetwork zeroPolicy;
    MCTSConfig cfgOff = makeRiskSeqConfig(&zeroPolicy, /*riskDeferral=*/false);
    MacroMCTSSearch searchOff(nullptr, cfgOff, 42);
    Macro baseline = searchOff.search(state);

    MCTSConfig cfgOn = makeRiskSeqConfig(&zeroPolicy, /*riskDeferral=*/true);
    MacroMCTSSearch searchOn(nullptr, cfgOn, 42);
    Macro guarded = searchOn.search(state);

    EXPECT_EQ(guarded.type, baseline.type);
    EXPECT_EQ(guarded.playerId, baseline.playerId);
    EXPECT_EQ(guarded.targetId, baseline.targetId);
}

// S3/S7 above are real mined game states -- faithful but dense (21 players,
// cryptic coordinates). This is a small hand-built fixture demonstrating the
// same Q-guard mechanism transparently: exactly 2 candidate macros exist for
// HOME, one deliberately lopsided, one deliberately risk-free.
//
// Player 1 (ST1) is already adjacent to player 12 (ST6): defST(6) > 2*attST(1)
// gives the DEFENDER 3 dice (getBlockDiceInfo), i.e. they pick the
// worst-for-attacker of 3 rolls. Without Block, ATTACKER_DOWN or BOTH_DOWN
// (2/6 faces each die) force a turnover -- P(no bad face in 3 dice) =
// (4/6)^3 ~= 0.30, so BLITZ/BLOCK's real turnover chance here is ~70%,
// nowhere near a close call, comfortably clearing RISK_DEFER_RISKY_PTO.
// Player 2 is isolated with no adjacent enemies and nothing but open squares
// to move into -- its REPOSITION is a genuine zero-dice, zero-dodge action,
// nowhere near RISK_DEFER_SAFE_PTO's ceiling. Player 12 carries the ball
// (held, not on the ground) so PICKUP never enters the candidate set and
// the risky BLITZ has a real upside (knocking the carrier down) -- the root
// has exactly END_TURN/BLITZ/REPOSITION, nothing else to confound which
// macro production would pick. The carrier is deliberately placed far from
// AWAY's own endzone (low `proximity` in simulate()'s heuristic) so that
// upside is small enough to land the risky pick's Q within
// RISK_DEFER_Q_MARGIN of the safe REPOSITION's -- dice probabilities are
// quantized (1/6, 1/36, ...) and can't be tuned to hit an exact margin, but
// this board-position term is continuous and can.
TEST(MacroMCTS, RiskDeferralDefersOnHandBuiltMismatch) {
    GameState state;
    state.phase = GamePhase::PLAY;
    state.activeTeam = TeamSide::HOME;
    state.half = 1;
    state.homeTeam.turnNumber = 1;
    state.homeTeam.rerolls = 0;
    state.awayTeam.rerolls = 0;
    state.homeTeam.blitzUsedThisTurn = false;
    state.weather = Weather::NICE;

    Player& weakBlitzer = state.getPlayer(1);
    weakBlitzer.id = 1;
    weakBlitzer.teamSide = TeamSide::HOME;
    weakBlitzer.state = PlayerState::STANDING;
    weakBlitzer.position = {14, 7};
    weakBlitzer.stats = {6, 1, 3, 8};
    weakBlitzer.movementRemaining = 6;

    // Retuned 2026-08-30 (TRETI PRELADENI): volny hrac presunut z {5,3} na
    // {13,3}. Hranu tentokrat posunula oprava M12 -- `ADVANCE` uz nerezignuje,
    // kdyz je zavrena primka a volno vedle, takze v rolloutech SOUPERUV nosic
    // neuvizne a riskantni BLITZ ztratil na hodnote: zaklad zacal volit
    // REPOSITION, tedy tyz tah, ktery ma vybrat teprve STRAZCE, a test prestal
    // rozlisovat.
    // ⚠️ Knoflik `proximity` (x-ova poloha dvojice) tentokrat NEZABRAL -- zkouseno
    //    x = 8, 10, 12, 17, 20, vsechny padaly. Zabralo az priblizeni VOLNEHO
    //    hrace k deji, ktere zmensi hodnotu REPOSITION jako alternativy.
    //    Aserce se NEOSLABILA, presunul se fixture -- jak zada komentar nize.
    //
    // Retuned 2026-08-24 from x=12/13 to x=14/15. Druhe preladeni z tehoz
    // duvodu jako to prvni: oprava pravidla posunula hranu. N8 (Both Down uz
    // obrance NEODSUNUJE a utocnik ho NENASLEDUJE, l. 514-519) ubrala blitzu
    // pozicni zisk, cimz se Q mezera zvetsila a strazce prestal odkladat.
    // Posun PRYC od AWAY endzony ji vraci na hranu -- tymz `proximity` knoflikem,
    // ktery popisuje komentar nize.
    // ⚠️ Tenhle test JE takhle krehky ZAMERNE (testuje strazce, ne geometrii),
    // ale znamena to, ze **kazda pravidlova oprava ho muze shodit** -- a je to
    // signal k preladeni, ne k oslabeni aserce.
    //
    // Retuned 2026-08-21 from x=15/16 to x=12/13. The fixture sits on a knife
    // edge by construction, and the P45 stand-up fix moved the edge: a blitzer
    // who fails the ~70 % roll used to lie there for the rest of the drive
    // (nobody in the engine ever stood up), so the risky BLITZ carried a
    // permanent body loss the search could see. Now he can get back up, the
    // downside shrank, and the undeferred baseline stopped picking BLITZ.
    // Moving the pair toward AWAY's endzone restores the Q gap via the same
    // `proximity` knob this comment already describes -- the guard, not the
    // geometry, is what the test is about.
    //
    // Positioned far from AWAY's own endzone (x=0) -- low `proximity` in
    // simulate()'s heuristic means dropping the ball here is a much smaller
    // swing than dropping it near their scoring end, a continuous knob for
    // narrowing the Q gap that dice-count stats can't provide (dice
    // probabilities are quantized: 1/6, 1/36, etc., not freely tunable).
    Player& strongDefender = state.getPlayer(12);
    strongDefender.id = 12;
    strongDefender.teamSide = TeamSide::AWAY;
    strongDefender.state = PlayerState::STANDING;
    strongDefender.position = {15, 7};
    strongDefender.stats = {6, 6, 3, 9};
    strongDefender.movementRemaining = 6;

    Player& freePlayer = state.getPlayer(2);
    freePlayer.id = 2;
    freePlayer.teamSide = TeamSide::HOME;
    freePlayer.state = PlayerState::STANDING;
    freePlayer.position = {13, 3};
    freePlayer.stats = {6, 3, 3, 8};
    freePlayer.movementRemaining = 6;

    // AWAY's own defender carries the ball -- gives the risky BLITZ a real
    // upside (knocking down the carrier is a genuine strategic win, not
    // just removing a body), so unguarded production actually picks it
    // despite the ~70% fail chance, and the guard's deferral is doing real
    // work rather than agreeing with what plain visit-based search would
    // have picked anyway.
    state.ball.isHeld = true;
    state.ball.carrierId = 12;
    state.ball.position = strongDefender.position;

    PolicyNetwork zeroPolicy;
    MCTSConfig cfgOff = makeRiskSeqConfig(&zeroPolicy, /*riskDeferral=*/false);
    MacroMCTSSearch searchOff(nullptr, cfgOff, 42);
    Macro baseline = searchOff.search(state);
    ASSERT_EQ(baseline.type, MacroType::BLITZ)
        << "fixture precondition: undeferred production must pick the risky BLITZ";

    MCTSConfig cfgOn = makeRiskSeqConfig(&zeroPolicy, /*riskDeferral=*/true);
    MacroMCTSSearch searchOn(nullptr, cfgOn, 42);
    Macro guarded = searchOn.search(state);
    EXPECT_EQ(guarded.type, MacroType::REPOSITION);
    EXPECT_EQ(guarded.playerId, 2);
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
