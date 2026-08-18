#pragma once

#include "bb/game_state.h"
#include "bb/turn_plan_record.h"
#include "bb/game_event.h"
#include "bb/roster.h"
#include "bb/dice.h"
#include "bb/rules_engine.h"
#include "bb/feature_extractor.h"
#include "bb/policies.h"
#include "bb/board_snapshot.h"
#include <functional>
#include <string>

namespace bb {

struct GameResult {
    int homeScore = 0;
    int awayScore = 0;
    int totalActions = 0;
};

// Set up 11 players per team in formation, initialize team state
// kickingTeam selects defensive formation for kicking side (default AWAY = HOME receives)
// dice is needed for the KO recovery roll at each kick-off (package G);
// pass nullptr in tests that do not care -- KO'd players then simply stay out.
void setupHalf(GameState& state, const TeamRoster& home, const TeamRoster& away,
               TeamSide kickingTeam = TeamSide::AWAY, DiceRollerBase* dice = nullptr);

// Same player/ball placement as setupHalf, for restarting play after a touchdown
// mid-half -- does NOT reset each team's turn clock or reroll pool (unlike
// setupHalf, which is reserved for true half boundaries: game start, half-time).
void setupDrive(GameState& state, const TeamRoster& home, const TeamRoster& away,
                TeamSide kickingTeam = TeamSide::AWAY, DiceRollerBase* dice = nullptr);

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

// Turn-level replay log
struct TurnLog {
    int half = 0;
    int turnNumber = 0;
    TeamSide activeTeam = TeamSide::HOME;
    int homeScore = 0;
    int awayScore = 0;

    // Board snapshot at start of turn
    std::vector<PlayerSnapshot> homePlayers;
    std::vector<PlayerSnapshot> awayPlayers;
    int8_t ballX = -1, ballY = -1;
    bool ballHeld = false;
    int ballCarrierId = -1;

    // Weather at the start of the turn (can change mid-game via the
    // kickoff table's Changing Weather result)
    Weather weather = Weather::NICE;

    // Events that happened during this turn
    std::vector<GameEvent> events;

    // What the turn planner decided this turn (bb/turn_plan_record.h).
    // plan.written == false means no planner ran -- the turn was pure search().
    TurnPlanRecord plan;

    // Standing opponents in the corridor ahead of OUR carrier at the start of
    // this turn -- "how hard is the path forward".
    //
    // ⛔ SEPARATE FROM plan.resistance ON PURPOSE (2026-08-18, K9b). The same
    // number lives in TurnPlanRecord, but ONLY when the cage planner ran, and
    // the gate is OFF in production (NOT_CONSULTED in 100% of turns), so that
    // copy is always 0. Worse, the simulator re-assigns curLog.plan from the
    // thread-local record on every action until `written` flips, so anything
    // stamped there at snapshot time would be overwritten by zeros. This field
    // is a property of the BOARD, not of the planner, and is filled every turn.
    // -1 = not applicable (we do not hold the ball).
    int8_t corridorResistance = -1;

    // Summary flags
    bool turnover = false;
    bool touchdown = false;
};

struct LoggedGameResult {
    GameResult result;
    std::vector<StateLog> states;
    std::vector<PolicyDecision> policyDecisions;  // MCTS visit distributions for policy training
    std::vector<TurnLog> turnLogs;  // Turn-by-turn replay data
};

LoggedGameResult simulateGameLogged(const TeamRoster& home, const TeamRoster& away,
                                    ActionSelector homePolicy, ActionSelector awayPolicy,
                                    DiceRollerBase& dice, bool useFullKickoff = false);

} // namespace bb
