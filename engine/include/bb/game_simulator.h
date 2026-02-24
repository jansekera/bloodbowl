#pragma once

#include "bb/game_state.h"
#include "bb/roster.h"
#include "bb/dice.h"
#include "bb/rules_engine.h"
#include "bb/feature_extractor.h"
#include "bb/policies.h"
#include <functional>

namespace bb {

struct GameResult {
    int homeScore = 0;
    int awayScore = 0;
    int totalActions = 0;
};

// Set up 11 players per team in formation, initialize team state
// kickingTeam selects defensive formation for kicking side (default AWAY = HOME receives)
void setupHalf(GameState& state, const TeamRoster& home, const TeamRoster& away,
               TeamSide kickingTeam = TeamSide::AWAY);

// Simplified kickoff: place ball with scatter, transition to PLAY
void simpleKickoff(GameState& state, DiceRollerBase& dice);

// Action selector: given a game state, return an action to execute
using ActionSelector = std::function<Action(const GameState&)>;

// Run a complete game with action selectors for each team
// useFullKickoff: if true, use resolveKickoff() with full kickoff events
GameResult simulateGame(const TeamRoster& home, const TeamRoster& away,
                        ActionSelector homePolicy, ActionSelector awayPolicy,
                        DiceRollerBase& dice, bool useFullKickoff = false);

// Logged game result with features for training
struct StateLog {
    float features[NUM_FEATURES];
    TeamSide perspective;
};

struct LoggedGameResult {
    GameResult result;
    std::vector<StateLog> states;
    std::vector<PolicyDecision> policyDecisions;  // MCTS visit distributions for policy training
};

LoggedGameResult simulateGameLogged(const TeamRoster& home, const TeamRoster& away,
                                    ActionSelector homePolicy, ActionSelector awayPolicy,
                                    DiceRollerBase& dice, bool useFullKickoff = false);

} // namespace bb
