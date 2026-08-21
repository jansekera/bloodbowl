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

// Item 7: the BLITZ approach walk historically picked each step by raw
// distance to the target alone -- the only movement path in the engine
// with zero tackle-zone awareness (every macro's movement routes through
// scoreMoveAction's TZ penalty). Reproduces the 21.07 wrong-direction-
// dodge shape: the guard at (12,6) puts tackle zones on the straight-line
// steps (11,6)/(11,7), while the equally-close (11,8)->(12,8) route is
// dodge-free. The old picker stepped into (11,6) (first equally-close
// square in getAdjacent order) and had to dodge out; the fixed picker
// must take the safe row and never roll a dodge.
TEST(ActionResolver, BlitzApproachAvoidsTacklezonesOnEquallyCloseSteps) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    gs.activeTeam = TeamSide::HOME;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    placePlayer(gs, 12, {13, 7}, TeamSide::AWAY);
    placePlayer(gs, 13, {12, 6}, TeamSide::AWAY);  // TZ on (11,6),(11,7),(12,7)

    Action action{ActionType::BLITZ, 1, 12, {13, 7}};
    // ST3 vs ST3 = 1 die; 3 = PUSHED. No dodge roll may be requested --
    // FixedDiceRoller throws if the approach burns dice it shouldn't need.
    FixedDiceRoller dice({3, 3, 3});
    std::vector<GameEvent> events;
    auto result = resolveAction(gs, action, dice, &events);

    EXPECT_TRUE(result.success);
    // The approach must run along the safe row (11,8)->(12,8) -- the old
    // raw-distance walk went (11,6)->(12,7) through the guard's tackle
    // zones. (Final position is NOT asserted: the post-block follow-up
    // legitimately moves the blitzer again after a push.)
    for (auto& ev : events) {
        EXPECT_NE(ev.type, GameEvent::Type::DODGE)
            << "blitz approach rolled a dodge despite a dodge-free route";
        if (ev.type == GameEvent::Type::PLAYER_MOVE && ev.playerId == 1) {
            EXPECT_NE(ev.to, (Position{11, 6}))
                << "approach stepped into the guard's tackle zone";
            EXPECT_NE(ev.to, (Position{11, 7}))
                << "approach stepped into the guard's tackle zone";
        }
    }
}

// ---------------------------------------------------------------------------
// VSTÁVÁNÍ (P45, 21.08.) — BB2016 rules_bb2016.txt ř. 670-695.
// Vstávání JE pohyb: zaplatím 3 a jdu dál; pod 3 MA hod 4+, a pak už jen GFI.
// Vstát v TZ jde bez hodu, dodge se platí až při odchodu.
// A po vstání NESMÍ přijít Block ("you may not move when you take a Block
// Action", ř. 675).
// ---------------------------------------------------------------------------

TEST(StandUp, Under3MA_Rolls4PlusAndSucceeds) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    gs.activeTeam = TeamSide::HOME;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME, /*ma=*/2);   // Treeman
    gs.getPlayer(1).state = PlayerState::PRONE;

    Action action{ActionType::MOVE, 1, -1, {10, 7}};
    FixedDiceRoller dice({4});                               // 4+ => uspěje
    auto result = resolveAction(gs, action, dice, nullptr);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::STANDING);
    // "he may not move further squares unless he Goes For It" => zbytek 0
    EXPECT_EQ(gs.getPlayer(1).movementRemaining, 0);
}

TEST(StandUp, Under3MA_FailureIsNotTurnover) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    gs.activeTeam = TeamSide::HOME;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME, /*ma=*/2);
    gs.getPlayer(1).state = PlayerState::PRONE;

    Action action{ActionType::MOVE, 1, -1, {10, 7}};
    FixedDiceRoller dice({3});                               // 3 => neuspěje
    auto result = resolveAction(gs, action, dice, nullptr);
    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::PRONE);
    // ř. 694-695: "Failure to stand successfully for any reason is not a turnover."
    EXPECT_FALSE(result.turnover);
}

TEST(StandUp, Under3MA_NextStepIsGfi) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    gs.activeTeam = TeamSide::HOME;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME, /*ma=*/2);
    gs.getPlayer(1).state = PlayerState::PRONE;

    FixedDiceRoller dice({4, 2});          // 4 = vstal, 2 = GFI (2+)
    Action stand{ActionType::MOVE, 1, -1, {10, 7}};
    ASSERT_TRUE(resolveAction(gs, stand, dice, nullptr).success);
    ASSERT_EQ(gs.getPlayer(1).movementRemaining, 0);

    Action step{ActionType::MOVE, 1, -1, {11, 7}};
    auto result = resolveAction(gs, step, dice, nullptr);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(gs.getPlayer(1).position, (Position{11, 7}));
    EXPECT_LT(gs.getPlayer(1).movementRemaining, 0);   // šel na GFI
}

TEST(StandUp, InTackleZoneNeedsNoRoll) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    gs.activeTeam = TeamSide::HOME;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME, /*ma=*/6);
    gs.getPlayer(1).state = PlayerState::PRONE;
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);          // soused v TZ

    Action action{ActionType::MOVE, 1, -1, {10, 7}};
    FixedDiceRoller dice({});                              // žádná kostka nepadne
    auto result = resolveAction(gs, action, dice, nullptr);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::STANDING);
    EXPECT_EQ(gs.getPlayer(1).movementRemaining, 3);        // 6-3
}

TEST(StandUp, AfterStandingUpBlockIsNotOffered) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    gs.activeTeam = TeamSide::HOME;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME, /*ma=*/6);
    gs.getPlayer(1).state = PlayerState::PRONE;
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);

    Action stand{ActionType::MOVE, 1, -1, {10, 7}};
    FixedDiceRoller dice({});
    ASSERT_TRUE(resolveAction(gs, stand, dice, nullptr).success);

    std::vector<Action> acts;
    getAvailableActions(gs, acts);
    for (const auto& a : acts) {
        EXPECT_FALSE(a.type == ActionType::BLOCK && a.playerId == 1)
            << "ř. 675: kdo vstal, nesmí vzít Block akci";
    }
}

TEST(StandUp, MovedPlayerIsNotOfferedBlock) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    gs.activeTeam = TeamSide::HOME;
    placePlayer(gs, 1, {9, 7}, TeamSide::HOME, /*ma=*/6);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);

    Action step{ActionType::MOVE, 1, -1, {10, 7}};
    FixedDiceRoller dice({});
    ASSERT_TRUE(resolveAction(gs, step, dice, nullptr).success);

    std::vector<Action> acts;
    getAvailableActions(gs, acts);
    for (const auto& a : acts) {
        EXPECT_FALSE(a.type == ActionType::BLOCK && a.playerId == 1)
            << "pohyb + blok je BLITZ, a ten je jeden za kolo";
    }
}
