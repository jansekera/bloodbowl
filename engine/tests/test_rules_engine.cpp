#include <gtest/gtest.h>
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

static int countActionsOfType(const std::vector<Action>& actions, ActionType type) {
    int c = 0;
    for (auto& a : actions) if (a.type == type) c++;
    return c;
}

TEST(RulesEngine, EndTurnAlwaysAvailable) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    std::vector<Action> actions;
    getAvailableActions(gs, actions);
    EXPECT_EQ(countActionsOfType(actions, ActionType::END_TURN), 1);
}

TEST(RulesEngine, NoActionsWhenNotPlayPhase) {
    GameState gs;
    gs.phase = GamePhase::GAME_OVER;
    std::vector<Action> actions;
    getAvailableActions(gs, actions);
    EXPECT_EQ(actions.size(), 0);
}

TEST(RulesEngine, MoveEnumeration) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    gs.activeTeam = TeamSide::HOME;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);

    std::vector<Action> actions;
    getAvailableActions(gs, actions);

    int moveCount = countActionsOfType(actions, ActionType::MOVE);
    EXPECT_EQ(moveCount, 8); // 8 adjacent squares all empty
}

TEST(RulesEngine, MoveBlockedByOccupied) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    gs.activeTeam = TeamSide::HOME;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    placePlayer(gs, 2, {11, 7}, TeamSide::HOME); // blocks one direction

    std::vector<Action> actions;
    getAvailableActions(gs, actions);

    // Count move actions for player 1 only
    int moveCount = 0;
    for (auto& a : actions) {
        if (a.type == ActionType::MOVE && a.playerId == 1) moveCount++;
    }
    EXPECT_EQ(moveCount, 7);
}

TEST(RulesEngine, BlockTargets) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    gs.activeTeam = TeamSide::HOME;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);

    std::vector<Action> actions;
    getAvailableActions(gs, actions);

    int blockCount = countActionsOfType(actions, ActionType::BLOCK);
    EXPECT_EQ(blockCount, 1);
}

TEST(RulesEngine, BlockNotAvailableForProneEnemy) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    gs.activeTeam = TeamSide::HOME;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    gs.getPlayer(12).state = PlayerState::PRONE;

    std::vector<Action> actions;
    getAvailableActions(gs, actions);

    int blockCount = countActionsOfType(actions, ActionType::BLOCK);
    EXPECT_EQ(blockCount, 0);
}

TEST(RulesEngine, BlitzOncePerTurn) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    gs.activeTeam = TeamSide::HOME;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    placePlayer(gs, 12, {15, 7}, TeamSide::AWAY); // not adjacent

    std::vector<Action> actions;
    getAvailableActions(gs, actions);
    int blitzCount = countActionsOfType(actions, ActionType::BLITZ);
    EXPECT_GE(blitzCount, 1);

    // After using blitz
    gs.homeTeam.blitzUsedThisTurn = true;
    getAvailableActions(gs, actions);
    blitzCount = countActionsOfType(actions, ActionType::BLITZ);
    EXPECT_EQ(blitzCount, 0);
}

TEST(RulesEngine, FoulTargets) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    gs.activeTeam = TeamSide::HOME;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    gs.getPlayer(12).state = PlayerState::PRONE;

    std::vector<Action> actions;
    getAvailableActions(gs, actions);

    int foulCount = countActionsOfType(actions, ActionType::FOUL);
    EXPECT_EQ(foulCount, 1);
}

TEST(RulesEngine, FoulOncePerTurn) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    gs.activeTeam = TeamSide::HOME;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    gs.getPlayer(12).state = PlayerState::PRONE;

    gs.homeTeam.foulUsedThisTurn = true;
    std::vector<Action> actions;
    getAvailableActions(gs, actions);

    int foulCount = countActionsOfType(actions, ActionType::FOUL);
    EXPECT_EQ(foulCount, 0);
}

TEST(RulesEngine, ActedPlayerCannotAct) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    gs.activeTeam = TeamSide::HOME;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).hasActed = true;

    std::vector<Action> actions;
    getAvailableActions(gs, actions);

    int moveCount = countActionsOfType(actions, ActionType::MOVE);
    EXPECT_EQ(moveCount, 0);
}

TEST(RulesEngine, PronePlayerCanStandUp) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    gs.activeTeam = TeamSide::HOME;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).state = PlayerState::PRONE;

    std::vector<Action> actions;
    getAvailableActions(gs, actions);

    // Should have a MOVE action to own position (stand up)
    bool found = false;
    for (auto& a : actions) {
        if (a.type == ActionType::MOVE && a.playerId == 1 &&
            a.target == (Position{10, 7})) {
            found = true;
        }
    }
    EXPECT_TRUE(found);
}

TEST(RulesEngine, NoMovementLeftNoMoveActions) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    gs.activeTeam = TeamSide::HOME;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).movementRemaining = -2; // used all GFI already

    std::vector<Action> actions;
    getAvailableActions(gs, actions);

    int moveCount = countActionsOfType(actions, ActionType::MOVE);
    EXPECT_EQ(moveCount, 0);
}
