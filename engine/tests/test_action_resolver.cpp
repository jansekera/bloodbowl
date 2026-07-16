#include <gtest/gtest.h>
#include "bb/action_resolver.h"
#include "bb/rules_engine.h"
#include "bb/helpers.h"

using namespace bb;

static void placePlayer(GameState& gs, int id, Position pos, TeamSide side,
                         int ma = 6, int st = 3, int ag = 3, int av = 8) {
    Player& p = gs.getPlayer(id);
    p.state = PlayerState::STANDING;
    p.position = pos;
    p.stats = {static_cast<int8_t>(ma), static_cast<int8_t>(st),
               static_cast<int8_t>(ag), static_cast<int8_t>(av)};
    p.movementRemaining = ma;
}

TEST(ActionResolver, DispatchMove) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    gs.activeTeam = TeamSide::HOME;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);

    Action action{ActionType::MOVE, 1, -1, {11, 7}};
    FixedDiceRoller dice({});
    auto result = resolveAction(gs, action, dice, nullptr);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(gs.getPlayer(1).position, (Position{11, 7}));
}

TEST(ActionResolver, DispatchBlock) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    gs.activeTeam = TeamSide::HOME;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);

    Action action{ActionType::BLOCK, 1, 12, {11, 7}};
    // DD: push + knockdown. Armor: 3+3=6 ≤ 8
    FixedDiceRoller dice({6, 3, 3});
    auto result = resolveAction(gs, action, dice, nullptr);
    EXPECT_TRUE(result.success);
}

TEST(ActionResolver, DispatchFoul) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    gs.activeTeam = TeamSide::HOME;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    gs.getPlayer(12).state = PlayerState::PRONE;

    Action action{ActionType::FOUL, 1, 12, {11, 7}};
    FixedDiceRoller dice({3, 4, 3, 3}); // armor not broken, no doubles
    auto result = resolveAction(gs, action, dice, nullptr);
    EXPECT_TRUE(result.success);
}

TEST(ActionResolver, DispatchEndTurn) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    gs.activeTeam = TeamSide::HOME;
    gs.homeTeam.turnNumber = 1;

    Action action{ActionType::END_TURN, -1, -1, {-1, -1}};
    FixedDiceRoller dice({});
    auto result = resolveAction(gs, action, dice, nullptr);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(gs.activeTeam, TeamSide::AWAY);
    EXPECT_EQ(gs.awayTeam.turnNumber, 1);
}

TEST(ActionResolver, TurnoverAutoEndsTurn) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    gs.activeTeam = TeamSide::HOME;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);

    Action action{ActionType::BLOCK, 1, 12, {11, 7}};
    // Roll AD → turnover → auto end turn
    FixedDiceRoller dice({1, 3, 3}); // AD + armor roll
    auto result = executeAction(gs, action, dice, nullptr);
    EXPECT_TRUE(result.turnover);
    EXPECT_EQ(gs.activeTeam, TeamSide::AWAY); // turn switched
}

TEST(ActionResolver, TouchdownDetected) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    gs.activeTeam = TeamSide::HOME;
    placePlayer(gs, 1, {24, 7}, TeamSide::HOME);
    gs.ball = BallState::carried({24, 7}, 1);

    Action action{ActionType::MOVE, 1, -1, {25, 7}};
    FixedDiceRoller dice({});
    auto result = executeAction(gs, action, dice, nullptr);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(gs.phase, GamePhase::TOUCHDOWN);
    EXPECT_EQ(gs.homeTeam.score, 1);
}

TEST(ActionResolver, MoveStandUpThenMove) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    gs.activeTeam = TeamSide::HOME;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).state = PlayerState::PRONE;

    // Action: move to adjacent square (will stand up first)
    Action action{ActionType::MOVE, 1, -1, {11, 7}};
    FixedDiceRoller dice({});
    auto result = resolveAction(gs, action, dice, nullptr);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::STANDING);
    EXPECT_EQ(gs.getPlayer(1).position, (Position{11, 7}));
    EXPECT_EQ(gs.getPlayer(1).movementRemaining, 2); // 6-3(standup)-1(move)
}

TEST(ActionResolver, MoveStandUpOnly) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    gs.activeTeam = TeamSide::HOME;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).state = PlayerState::PRONE;

    // Action: move to own position (just stand up)
    Action action{ActionType::MOVE, 1, -1, {10, 7}};
    FixedDiceRoller dice({});
    auto result = resolveAction(gs, action, dice, nullptr);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::STANDING);
    EXPECT_EQ(gs.getPlayer(1).position, (Position{10, 7}));
}

TEST(ActionResolver, HalfOverDetected) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    gs.activeTeam = TeamSide::HOME;
    gs.homeTeam.turnNumber = 8;
    gs.awayTeam.turnNumber = 8;

    // End turn → away turn 9 → half over
    Action action{ActionType::END_TURN, -1, -1, {-1, -1}};
    FixedDiceRoller dice({});
    auto result = executeAction(gs, action, dice, nullptr);
    EXPECT_EQ(gs.phase, GamePhase::HALF_TIME);
}

TEST(ActionResolver, GameOverSecondHalf) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    gs.half = 2;
    gs.activeTeam = TeamSide::HOME;
    gs.homeTeam.turnNumber = 8;
    gs.awayTeam.turnNumber = 8;

    Action action{ActionType::END_TURN, -1, -1, {-1, -1}};
    FixedDiceRoller dice({});
    auto result = executeAction(gs, action, dice, nullptr);
    EXPECT_EQ(gs.phase, GamePhase::GAME_OVER);
}

// --- One-activation-per-player close-out (hasActed double-activation fix) ---
// Negative control for the bug where a successful MOVE never set hasActed,
// letting a player be reactivated for BLOCK/PASS/FOUL later in the same turn
// after other players acted in between (evidence/fable_hasacted_bug_20260715.md).
// This test FAILS against the pre-fix engine.
TEST(ActionResolver, InterleavedReactivationClosedOut) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    gs.activeTeam = TeamSide::HOME;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    placePlayer(gs, 2, {5, 5}, TeamSide::HOME);
    placePlayer(gs, 12, {12, 7}, TeamSide::AWAY); // adjacent to p1's destination

    FixedDiceRoller dice({});

    // Player 1 completes a successful move (ends adjacent to opponent 12).
    Action move1{ActionType::MOVE, 1, -1, {11, 7}};
    auto r1 = executeAction(gs, move1, dice, nullptr);
    ASSERT_TRUE(r1.success);
    EXPECT_FALSE(gs.getPlayer(1).hasActed); // activation still open

    // A DIFFERENT player acts: player 1's activation must be closed out.
    Action move2{ActionType::MOVE, 2, -1, {5, 6}};
    auto r2 = executeAction(gs, move2, dice, nullptr);
    ASSERT_TRUE(r2.success);
    EXPECT_TRUE(gs.getPlayer(1).hasActed); // pre-fix: false (the bug)

    // Player 1 must no longer be offered any action (pre-fix: free BLOCK on 12).
    std::vector<Action> actions;
    getAvailableActions(gs, actions);
    int p1Actions = 0;
    for (const auto& a : actions) {
        if (a.playerId == 1) p1Actions++;
    }
    EXPECT_EQ(p1Actions, 0);
}

// Positive control: multi-step movement and continuous same-player sequences
// stay legal — the close-out only fires on an actor SWITCH.
TEST(ActionResolver, SamePlayerMultiStepMoveStaysOpen) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    gs.activeTeam = TeamSide::HOME;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);

    FixedDiceRoller dice({});
    Action step1{ActionType::MOVE, 1, -1, {11, 7}};
    Action step2{ActionType::MOVE, 1, -1, {12, 7}};
    ASSERT_TRUE(executeAction(gs, step1, dice, nullptr).success);
    ASSERT_TRUE(executeAction(gs, step2, dice, nullptr).success);

    EXPECT_FALSE(gs.getPlayer(1).hasActed);
    EXPECT_EQ(gs.currentActivationId, 1);

    // Player 1 is still offered actions (continuous activation).
    std::vector<Action> actions;
    getAvailableActions(gs, actions);
    int p1Actions = 0;
    for (const auto& a : actions) {
        if (a.playerId == 1) p1Actions++;
    }
    EXPECT_GT(p1Actions, 0);
}

// The tracker resets at the turn boundary: an opponent acting on the next turn
// must not close out (or be linked to) the previous turn's mover.
TEST(ActionResolver, ActivationTrackerResetsOnEndTurn) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    gs.activeTeam = TeamSide::HOME;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    placePlayer(gs, 12, {15, 7}, TeamSide::AWAY);

    FixedDiceRoller dice({});
    Action move1{ActionType::MOVE, 1, -1, {11, 7}};
    ASSERT_TRUE(executeAction(gs, move1, dice, nullptr).success);
    EXPECT_EQ(gs.currentActivationId, 1);

    Action endTurn{ActionType::END_TURN, -1, -1, {-1, -1}};
    executeAction(gs, endTurn, dice, nullptr);
    EXPECT_EQ(gs.currentActivationId, -1);

    // Away player acts: no spurious close-out of home player 1.
    Action move12{ActionType::MOVE, 12, -1, {16, 7}};
    ASSERT_TRUE(executeAction(gs, move12, dice, nullptr).success);
    EXPECT_FALSE(gs.getPlayer(1).hasActed);
    EXPECT_EQ(gs.currentActivationId, 12);
}

TEST(ActionResolver, BlitzAdjacentBlock) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    gs.activeTeam = TeamSide::HOME;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);

    Action action{ActionType::BLITZ, 1, 12, {11, 7}};
    // DD: push + knockdown. Armor: 3+3=6 ≤ 8
    FixedDiceRoller dice({6, 3, 3});
    auto result = resolveAction(gs, action, dice, nullptr);
    EXPECT_TRUE(result.success);
    EXPECT_TRUE(gs.homeTeam.blitzUsedThisTurn);
}
