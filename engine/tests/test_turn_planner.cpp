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
//   p4 -- a THIRD free teammate, far from the ball: with the safe-stage cap
//         (MAX_SAFE_BACKUPS=2, user constraint 2026-07-31) it must be the
//         one left out -- the plan sends the two nearest backups only.
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
    mk(4, TeamSide::HOME, {5, 12}, 3);
    mk(12, TeamSide::AWAY, {24, 13}, 3);
    return state;
}

MCTSConfig plannerConfig(bool enablePlanner = true) {
    MCTSConfig cfg;
    cfg.maxIterations = 60;
    cfg.timeBudgetMs = 0;
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

// --- Item13 step 2 (2026-08-07): cage-fill stage ---

TEST(TurnPlannerStep2, CageFillMacroInvalidWithoutHeldBall) {
    // Fail-pickup semantics: cage-fill macros carry requireHeldBall -- with
    // the ball loose or in enemy hands the whole stage is invalid and the
    // policy falls back to search().
    GameState state = makeLooseBallState();
    Macro m{MacroType::REPOSITION, 3, -1, {14, 8}};
    EXPECT_TRUE(stagedMacroStillValid(state, m));
    EXPECT_FALSE(stagedMacroStillValid(state, m, true));  // ball still loose

    state.ball = BallState::carried({24, 13}, 12);        // opponent recovered
    EXPECT_FALSE(stagedMacroStillValid(state, m, true));

    state.ball = BallState::carried({11, 7}, 1);          // our carrier
    EXPECT_TRUE(stagedMacroStillValid(state, m, true));
}

// --- Corridor + adoption floor (2026-08-07, z validace kroku 2) ---

