#include <gtest/gtest.h>
#include "bb/action_resolver.h"
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
