#include <gtest/gtest.h>
#include "bb/turn_planner.h"
#include "bb/macro_mcts.h"
#include "bb/game_state.h"
#include "bb/action_resolver.h"
#include <algorithm>

using namespace bb;

namespace {

// Hand-built loose-ball fixture (item 13 MVP): ball on the ground at
// {13,7}, HOME to act.
//   p1 -- the unambiguous picker: closest to the ball, highest AG. With p2
//         16 score points behind (> kSecondPickerMaxGap 15), generation
//         emits exactly ONE PICKUP candidate, keeping assertions
//         deterministic.
//   p2, p3 -- free teammates at different distances; with a loose ball,
//         REPOSITION generation targets free ball-adjacent squares for
//         them (item 11), i.e. exactly the "bring backup" behavior the
//         staged plan must sequence BEFORE the pickup roll.
//   p12 -- lone AWAY player far away: keeps macro generation realistic
//         (BLITZ candidates exist) without any tackle zone near the action.
// Rerolls 0 on both sides so pSuccess reflects the bare pickup roll
// (attemptRoll would otherwise fold a team reroll into the branch).
GameState makeLooseBallState(int pickerAgility = 4) {
    GameState state;
    state.phase = GamePhase::PLAY;
    state.activeTeam = TeamSide::HOME;
    state.half = 1;
    state.homeTeam.turnNumber = 4;  // mid-drive: no first-turn/urgency priors
    state.homeTeam.rerolls = 0;
    state.awayTeam.rerolls = 0;
    state.weather = Weather::NICE;
    state.ball = BallState::onGround({13, 7});

    auto mk = [&](int id, TeamSide side, Position pos, int8_t ag) {
        Player& p = state.getPlayer(id);
        p.id = id;
        p.teamSide = side;
        p.state = PlayerState::STANDING;
        p.position = pos;
        p.stats = {6, 3, ag, 8};
        p.movementRemaining = 6;
        p.hasMoved = false;
        p.hasActed = false;
    };
    mk(1, TeamSide::HOME, {11, 7}, static_cast<int8_t>(pickerAgility));
    mk(2, TeamSide::HOME, {9, 4}, 3);
    mk(3, TeamSide::HOME, {16, 10}, 3);
    mk(12, TeamSide::AWAY, {24, 13}, 3);
    return state;
}

MCTSConfig plannerConfig(bool enablePlanner = true) {
    MCTSConfig cfg;
    cfg.maxIterations = 60;
    cfg.timeBudgetMs = 0;
    cfg.stagedPickupPlanner = enablePlanner;
    return cfg;
}

bool actionIsAvailable(const GameState& state, const Action& a) {
    std::vector<Action> available;
    getAvailableActions(state, available);
    for (auto& av : available) {
        if (av.type == a.type && av.playerId == a.playerId &&
            av.targetId == a.targetId && av.target == a.target) {
            return true;
        }
    }
    return false;
}

} // anonymous namespace

// =============================================================
// Turn-goal classification (mirror of simulate()'s pacing arithmetic)
// =============================================================

TEST(TurnPlanner, ClassifyGoalPickupWhenBallLoose) {
    GameState state = makeLooseBallState();
    EXPECT_EQ(classifyTurnGoal(state), TurnGoal::PICKUP_BALL);
}

TEST(TurnPlanner, ClassifyGoalAdvanceWhenHeldOutOfRange) {
    GameState state = makeLooseBallState();
    // HOME p1 carries at x=11: endzone x=25, dist 14 > MA6+2.
    state.ball = BallState::carried({11, 7}, 1);
    EXPECT_EQ(classifyTurnGoal(state), TurnGoal::ADVANCE_BALL);
}

TEST(TurnPlanner, ClassifyGoalScoreWhenInReach) {
    GameState state = makeLooseBallState();
    Player& p1 = state.getPlayer(1);
    p1.position = {20, 7};  // dist 5 <= MA6 (+2 GFI)
    state.ball = BallState::carried({20, 7}, 1);
    EXPECT_EQ(classifyTurnGoal(state), TurnGoal::SCORE_BALL);
}

TEST(TurnPlanner, ClassifyGoalScoreOnLastTurnsUrgency) {
    GameState state = makeLooseBallState();
    Player& p1 = state.getPlayer(1);
    p1.position = {18, 7};        // dist 7: not reachable on movementRemaining 4
    p1.movementRemaining = 4;
    state.ball = BallState::carried({18, 7}, 1);
    state.homeTeam.turnNumber = 7; // turnsLeft = 2, dist 7 <= MA6+2 -> urgency
    EXPECT_EQ(classifyTurnGoal(state), TurnGoal::SCORE_BALL);
}

TEST(TurnPlanner, ClassifyGoalNoneWhenOpponentHolds) {
    GameState state = makeLooseBallState();
    state.ball = BallState::carried({24, 13}, 12);
    EXPECT_EQ(classifyTurnGoal(state), TurnGoal::NONE);
}

// =============================================================
// Staged plan construction: safe backups first, single PICKUP branch last
// =============================================================

TEST(TurnPlanner, PlanSequencesBackupsBeforePickup) {
    GameState state = makeLooseBallState();
    StagedTurnPlanner planner(nullptr, plannerConfig(), 42);
    StagedPlan plan = planner.build(state);

    ASSERT_TRUE(plan.valid);
    EXPECT_EQ(plan.goal, TurnGoal::PICKUP_BALL);
    EXPECT_EQ(plan.pickupMacro.type, MacroType::PICKUP);
    EXPECT_EQ(plan.pickupMacro.playerId, 1);

    // Item 11 closure: teammates' backup REPOSITIONs are IN the plan and by
    // construction precede the branch (safeMacros execute before pickupMacro).
    ASSERT_GE(plan.safeMacros.size(), 2u);
    EXPECT_GE(plan.backupCount, 2);
    for (const auto& m : plan.safeMacros) {
        EXPECT_EQ(m.type, MacroType::REPOSITION);
        // The picker acts exactly once, at the branch -- never in the safe stage.
        EXPECT_NE(m.playerId, 1);
        // Backup targets stand NEXT TO the loose ball, never on its square
        // (stepping on it would auto-trigger a real pickup roll -- item 11).
        EXPECT_NE(m.targetPos, state.ball.position);
    }
}

TEST(TurnPlanner, PlanValueIsTwoBranchCombination) {
    GameState state = makeLooseBallState();
    StagedTurnPlanner planner(nullptr, plannerConfig(), 42);
    StagedPlan plan = planner.build(state);

    ASSERT_TRUE(plan.valid);
    EXPECT_GE(plan.pSuccess, 0.0);
    EXPECT_LE(plan.pSuccess, 1.0);
    // AG4 picker, no rerolls, no TZ on the ball: both outcomes must be
    // realized in the branch sample.
    EXPECT_GT(plan.pSuccess, 0.0);
    EXPECT_LT(plan.pSuccess, 1.0);
    EXPECT_NEAR(plan.planValue,
                plan.pSuccess * plan.valueSuccess +
                    (1.0 - plan.pSuccess) * plan.valueFail,
                1e-12);
    // The fail branch carries the fixed penalty on top of a static eval that
    // is itself bounded by simulate()'s clamp to [-1, 1].
    EXPECT_LT(plan.valueFail, 1.0 + StagedTurnPlanner::FAIL_PENALTY + 1e-12);
    EXPECT_GT(plan.valueSuccess, plan.valueFail);
}

TEST(TurnPlanner, PickupProbabilityTracksAgility) {
    // Solo-picker fixture: with teammates present, a LOW-agility p1 would
    // lose the picker slot to a closer/higher-AG teammate (generation scores
    // AG*10 - dist*3) and the comparison would silently measure two
    // different players. Keep only the picker + one far-away opponent so the
    // branch probability reflects the bare roll of the SAME player.
    auto makeSolo = [](int ag) {
        GameState state;
        state.phase = GamePhase::PLAY;
        state.activeTeam = TeamSide::HOME;
        state.half = 1;
        state.homeTeam.turnNumber = 4;
        state.homeTeam.rerolls = 0;
        state.awayTeam.rerolls = 0;
        state.weather = Weather::NICE;
        state.ball = BallState::onGround({13, 7});
        Player& p1 = state.getPlayer(1);
        p1.id = 1;
        p1.teamSide = TeamSide::HOME;
        p1.state = PlayerState::STANDING;
        p1.position = {11, 7};
        p1.stats = {6, 3, static_cast<int8_t>(ag), 8};
        p1.movementRemaining = 6;
        Player& opp = state.getPlayer(12);
        opp.id = 12;
        opp.teamSide = TeamSide::AWAY;
        opp.state = PlayerState::STANDING;
        opp.position = {24, 13};
        opp.stats = {6, 3, 3, 8};
        opp.movementRemaining = 6;
        return state;
    };
    GameState ag5 = makeSolo(5);
    GameState ag1 = makeSolo(1);
    StagedTurnPlanner planner5(nullptr, plannerConfig(), 42);
    StagedTurnPlanner planner1(nullptr, plannerConfig(), 42);
    StagedPlan plan5 = planner5.build(ag5);
    StagedPlan plan1 = planner1.build(ag1);

    ASSERT_TRUE(plan5.valid);
    ASSERT_TRUE(plan1.valid);
    // Bare-roll expectations (rerolls 0, pickup target = clamp(6-AG, 2, 6)):
    // AG5 -> 2+ (~0.83), AG1 -> 5+ (~0.33). A 0.2 separation floor keeps
    // this robust to BRANCH_K=64 sampling noise at any seed.
    EXPECT_GT(plan5.pSuccess, plan1.pSuccess + 0.2);
}

TEST(TurnPlanner, NoPlanForNonPickupGoals) {
    GameState state = makeLooseBallState();
    state.ball = BallState::carried({11, 7}, 1);  // ADVANCE goal
    StagedTurnPlanner planner(nullptr, plannerConfig(), 42);
    StagedPlan plan = planner.build(state);
    EXPECT_EQ(plan.goal, TurnGoal::ADVANCE_BALL);
    EXPECT_FALSE(plan.valid);
    EXPECT_TRUE(plan.safeMacros.empty());
}

// =============================================================
// Planned-macro semantic re-validation (deviation detection)
// =============================================================

TEST(TurnPlanner, StagedMacroValidationChecks) {
    GameState state = makeLooseBallState();

    Macro repo{MacroType::REPOSITION, 2, -1, {12, 6}};  // free ball-adjacent square
    EXPECT_TRUE(stagedMacroStillValid(state, repo));

    // Target square occupied by a teammate -> deviation.
    Macro repoOccupied{MacroType::REPOSITION, 2, -1, {11, 7}};  // p1 stands here
    EXPECT_FALSE(stagedMacroStillValid(state, repoOccupied));

    // Target square IS the loose ball -> never valid (item 11).
    Macro repoOnBall{MacroType::REPOSITION, 2, -1, {13, 7}};
    EXPECT_FALSE(stagedMacroStillValid(state, repoOnBall));

    Macro pickup{MacroType::PICKUP, 1, -1, {13, 7}};
    EXPECT_TRUE(stagedMacroStillValid(state, pickup));

    // Ball got picked up in the meantime -> PICKUP is a deviation.
    GameState held = state.clone();
    held.ball = BallState::carried({11, 7}, 1);
    EXPECT_FALSE(stagedMacroStillValid(held, pickup));

    // Player already spent their activation -> deviation.
    GameState acted = state.clone();
    acted.getPlayer(2).hasActed = true;
    EXPECT_FALSE(stagedMacroStillValid(acted, repo));

    // Macro types outside the MVP plan alphabet are conservatively invalid.
    Macro blitz{MacroType::BLITZ, 2, 12, {-1, -1}};
    EXPECT_FALSE(stagedMacroStillValid(state, blitz));
}

// =============================================================
// MacroMCTSPolicy integration (config-gated, default off)
// =============================================================

TEST(TurnPlanner, ConfigDefaultIsOff) {
    MCTSConfig cfg;
    EXPECT_FALSE(cfg.stagedPickupPlanner);
}

TEST(TurnPlannerPolicy, BackupActsBeforePickerWhenEnabled) {
    GameState state = makeLooseBallState();
    MacroMCTSPolicy policy(nullptr, plannerConfig(), 42);

    Action first = policy(state);
    // The staged plan's first macro is a backup REPOSITION -- the picker
    // (p1) must NOT be the first player to act (that would be exactly the
    // item 11 gap: pickup attempted before any backup arrives).
    EXPECT_TRUE(actionIsAvailable(state, first));
    EXPECT_EQ(first.type, ActionType::MOVE);
    EXPECT_NE(first.playerId, 1);
}

TEST(TurnPlannerPolicy, FullTurnReachesPickupAfterBackups) {
    GameState state = makeLooseBallState();
    MacroMCTSPolicy policy(nullptr, plannerConfig(), 7);
    DiceRoller dice(123);

    // Drive the policy through the whole team-turn, recording which players
    // act in which order. The picker's first action must come after at least
    // one backup teammate has already moved.
    std::vector<int> actingOrder;
    for (int step = 0; step < 60; ++step) {
        if (state.phase != GamePhase::PLAY || state.activeTeam != TeamSide::HOME) break;
        Action a = policy(state);
        ASSERT_TRUE(actionIsAvailable(state, a)) << "step " << step;
        if (a.playerId > 0 &&
            (actingOrder.empty() || actingOrder.back() != a.playerId)) {
            actingOrder.push_back(a.playerId);
        }
        executeAction(state, a, dice, nullptr);
        if (a.type == ActionType::END_TURN) break;
    }

    auto pickerIt = std::find(actingOrder.begin(), actingOrder.end(), 1);
    ASSERT_NE(pickerIt, actingOrder.end()) << "picker never activated";
    EXPECT_NE(pickerIt, actingOrder.begin())
        << "picker acted first -- backups were not sequenced before the branch";
}

TEST(TurnPlannerPolicy, FallsBackToSearchOnDeviation) {
    GameState state = makeLooseBallState();
    MacroMCTSPolicy policy(nullptr, plannerConfig(), 42);

    Action first = policy(state);
    ASSERT_TRUE(actionIsAvailable(state, first));

    // Unmodeled deviation mid-turn: the opponent suddenly holds the ball
    // (nothing in the staged plan models this). The policy must not crash
    // and must keep returning actions that are actually available -- i.e.
    // the existing per-macro search() fallback takes over.
    state.ball = BallState::carried({24, 13}, 12);
    for (int i = 0; i < 3; ++i) {
        Action a = policy(state);
        EXPECT_TRUE(actionIsAvailable(state, a)) << "call " << i;
        if (a.type == ActionType::END_TURN) break;
        DiceRoller dice(99 + i);
        executeAction(state, a, dice, nullptr);
        if (state.phase != GamePhase::PLAY || state.activeTeam != TeamSide::HOME) break;
    }
}

TEST(TurnPlannerPolicy, DisabledPlannerMatchesSearchPath) {
    // With the gate off the policy must behave exactly as before: pure
    // search. Same seed, same state, planner-off vs a never-planning config
    // (planner enabled but ball held -> goal ADVANCE, no plan) both go
    // through search_ -- smoke-level equivalence: first action identical.
    GameState held = makeLooseBallState();
    held.ball = BallState::carried({11, 7}, 1);

    MacroMCTSPolicy off(nullptr, plannerConfig(false), 42);
    MacroMCTSPolicy onButInert(nullptr, plannerConfig(true), 42);

    GameState s1 = held.clone();
    GameState s2 = held.clone();
    Action a1 = off(s1);
    Action a2 = onButInert(s2);
    EXPECT_EQ(a1.type, a2.type);
    EXPECT_EQ(a1.playerId, a2.playerId);
    EXPECT_EQ(a1.target, a2.target);
}
