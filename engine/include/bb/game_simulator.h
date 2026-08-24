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

// Pre-match coin toss -- BB2016 l. 304-307: "Flip the Blood Bowl coin or roll
// a D6 to see which coach will choose who will set up first. The team that sets
// up first is called the kicking team." Returns the team that kicks off to open
// the FIRST half; the second half reverses it (l. 1016-1017).
//
// ⚠️ Volba vitéze losu je DOKTRINA, ne pravidlo -- pravidla jen rikaji, ze
// vitéz volí. Merenim 24.08. (18 000 her) vyslo, ze prijmout DRUHOU pulku je
// vyhoda (+4,9σ human, +4,7σ orc), takze "volim kop" je racionalni volba pro
// rychle tymy; trpaslik je k tomu lhostejny (-0,4σ). Dokud to neni rozhodnuto,
// vitéz losu PRIJIMA (naivni volba) -- symetrii to nelamé, protoze vitéz losu
// je nahodny.
// V nasi terminologii: RECEIVE = volim UTOK (mam mic), KICK = volim OBRANU.
enum class TossElection { RECEIVE, KICK };

// Co vitéz losu volí. Rozhodnuto 24.08.2026 podle bbtactics.com/kicking-receiving
// a merení na krizovem korpusu, a je to RASOVE, ne globalne:
//   * KREHKE/RYCHLE soupisky (elf, skaven, jesterky, gobliní, pulcici) volí UTOK
//     -- prvni kolo zapasu, tri "bloky zdarma" na LOS, rychly TD (2-3 kola)
//     a souperi se upre posledni kolo zapasu;
//   * VSECHNY OSTATNI, a hlavne BASHOVE a vysokoAV (trpaslik, chaos dwarf, ork)
//     volí OBRANU -- brani s kompletnim soupisem, KO nadelane souperi maji jen
//     JEDNU sanci se vratit, a utoci se ve druhe pulce proti oslabenemu tymu
//     (2-1 grind). Nase mereni 24.08. ukazalo, ze druha pulka je vyhoda.
TossElection defaultTossElection(const TeamRoster& roster);

// Pre-match coin toss -- BB2016 l. 304-307: "Flip the Blood Bowl coin or roll
// a D6 to see which coach will choose who will set up first. The team that sets
// up first is called the kicking team." Returns the team that kicks off to open
// the FIRST half; the second half reverses it (l. 1016-1017).
TeamSide rollOpeningKickingTeam(DiceRollerBase& dice,
                                const TeamRoster& home, const TeamRoster& away);
// Testovaci varianta s vynucenou volbou.
TeamSide rollOpeningKickingTeam(DiceRollerBase& dice,
                                TossElection winnerElects = TossElection::RECEIVE);

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

    // Sesterská veličina k corridorResistance (2026-08-21): SOUČET ST týchž
    // těl. Odpor jako počet nerozlišil čtyři soupeře (1,78/1,93/1,89/1,89),
    // přestože se v nich trpaslíkovo skórování liší 4,3x. -1 = N/A.
    int16_t corridorStrength = -1;

    // Tempo jako vlastnost desky (2026-08-21). `plan.*` je v produkci celé
    // nula (NOT_CONSULTED ve 100 % kol), takže dosud NEEXISTOVALO měření
    // toho, jestli vůbec stíháme dojít. -1 = N/A (nedržíme míč v našem kole).
    float requiredPace = -1.0f;
    float achievablePace = -1.0f;
    int8_t distToEndzone = -1;

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
