#include "bb/game_simulator.h"
#include "bb/action_resolver.h"
#include "bb/ball_handler.h"
#include "bb/kickoff_handler.h"
#include "bb/helpers.h"
#include "bb/turn_handler.h"
#include <algorithm>

namespace bb {

namespace {

// Standard formation positions relative to LOS
// Home: facing right (scores at x=25), LOS at x=12
// Away: facing left (scores at x=0), LOS at x=13

struct FormationPos { int8_t dx; int8_t y; };

// 4 on LOS, 4 in second row, 3 in backfield = 11 players
constexpr FormationPos HOME_FORMATION[11] = {
    // LOS (4 players at x=12)
    {0, 5}, {0, 6}, {0, 7}, {0, 8},
    // Second row (4 players at x=11)
    {-1, 4}, {-1, 6}, {-1, 8}, {-1, 10},
    // Backfield (3 players at x=9)
    {-3, 3}, {-3, 7}, {-3, 11},
};

constexpr FormationPos AWAY_FORMATION[11] = {
    // LOS (4 players at x=13)
    {0, 5}, {0, 6}, {0, 7}, {0, 8},
    // Second row (4 players at x=14)
    {1, 4}, {1, 6}, {1, 8}, {1, 10},
    // Backfield (3 players at x=16)
    {3, 3}, {3, 7}, {3, 11},
};

void placeTeam(GameState& state, TeamSide side, const TeamRoster& roster,
               const FormationPos formation[11]) {
    int baseId = (side == TeamSide::HOME) ? 1 : 12;
    int baseLOS = (side == TeamSide::HOME) ? 12 : 13;
    int idx = 0;

    // Fill 11 player slots from roster positionals
    int templateIdx = 0;
    int templateUsed = 0;

    for (int i = 0; i < 11 && templateIdx < roster.positionalCount; ++i) {
        Player& p = state.getPlayer(baseId + i);
        p.id = baseId + i;
        p.teamSide = side;
        p.state = PlayerState::STANDING;
        p.position = {
            static_cast<int8_t>(baseLOS + formation[i].dx),
            formation[i].y
        };
        p.stats = roster.positionals[templateIdx].stats;
        p.skills = roster.positionals[templateIdx].skills;
        p.movementRemaining = p.stats.movement;
        p.hasMoved = false;
        p.hasActed = false;
        p.usedBlitz = false;
        p.lostTacklezones = false;
        p.proUsedThisTurn = false;

        templateUsed++;
        if (templateUsed >= roster.positionals[templateIdx].quantity ||
            templateUsed >= (templateIdx == 0 ? 11 : roster.positionals[templateIdx].quantity)) {
            // Move to next positional once we've used enough of current type
            // For the first type (lineman), fill remaining slots
            templateIdx++;
            templateUsed = 0;
        }
    }

    // Set team state
    TeamState& ts = state.getTeamState(side);
    ts.side = side;
    ts.rerolls = 3;  // Standard starting rerolls
    ts.hasApothecary = roster.hasApothecary;
    ts.apothecaryUsed = false;
}

// Build a standard 11-player team: fill specialized positions first, then linemen
void buildTeam(GameState& state, TeamSide side, const TeamRoster& roster,
               const FormationPos formation[11]) {
    int baseId = (side == TeamSide::HOME) ? 1 : 12;
    int baseLOS = (side == TeamSide::HOME) ? 12 : 13;

    // First pass: assign specialized positionals (non-linemen, index > 0)
    int slot = 0;
    // Start with special positionals to fill key positions
    // Blitzers in backfield/second row, catchers in backfield, thrower in back
    // For simplicity: fill from back of formation (backfield first) with specialists

    // Collect how many of each positional to place
    struct Placement { int templateIdx; int count; };
    Placement placements[8];
    int nPlacements = 0;

    // Specialists first (indices 1+)
    for (int t = 1; t < roster.positionalCount; ++t) {
        int qty = std::min((int)roster.positionals[t].quantity, 11);
        if (qty > 0) {
            placements[nPlacements++] = {t, qty};
        }
    }

    // Fill from end of formation (backfield) with specialists, rest with linemen
    int placed = 0;
    int specSlot = 10; // start filling from backfield

    // Place specialists in the "best" positions (backfield/second row)
    for (int p = 0; p < nPlacements && specSlot >= 0; ++p) {
        for (int q = 0; q < placements[p].count && specSlot >= 0; ++q) {
            Player& player = state.getPlayer(baseId + specSlot);
            player.id = baseId + specSlot;
            player.teamSide = side;
            player.state = PlayerState::STANDING;
            player.position = {
                static_cast<int8_t>(baseLOS + formation[specSlot].dx),
                formation[specSlot].y
            };
            player.stats = roster.positionals[placements[p].templateIdx].stats;
            player.skills = roster.positionals[placements[p].templateIdx].skills;
            player.movementRemaining = player.stats.movement;
            player.hasMoved = false;
            player.hasActed = false;
            player.usedBlitz = false;
            player.lostTacklezones = false;
            player.proUsedThisTurn = false;
            specSlot--;
            placed++;
        }
    }

    // Fill remaining slots (0 to specSlot) with linemen (template index 0)
    for (int i = 0; i <= specSlot; ++i) {
        Player& player = state.getPlayer(baseId + i);
        player.id = baseId + i;
        player.teamSide = side;
        player.state = PlayerState::STANDING;
        player.position = {
            static_cast<int8_t>(baseLOS + formation[i].dx),
            formation[i].y
        };
        player.stats = roster.positionals[0].stats;
        player.skills = roster.positionals[0].skills;
        player.movementRemaining = player.stats.movement;
        player.hasMoved = false;
        player.hasActed = false;
        player.usedBlitz = false;
        player.lostTacklezones = false;
        player.proUsedThisTurn = false;
    }

    // Set team state
    TeamState& ts = state.getTeamState(side);
    ts.side = side;
    ts.score = ts.score;  // preserve score across halves
    ts.rerolls = 3;
    ts.turnNumber = 0;
    ts.hasApothecary = roster.hasApothecary;
    ts.apothecaryUsed = false;
    ts.resetForNewTurn();
}

} // anonymous namespace

void setupHalf(GameState& state, const TeamRoster& home, const TeamRoster& away) {
    // Reset all players to off-pitch
    for (auto& p : state.players) {
        p.state = PlayerState::OFF_PITCH;
        p.position = {-1, -1};
        p.hasMoved = false;
        p.hasActed = false;
        p.usedBlitz = false;
        p.lostTacklezones = false;
        p.proUsedThisTurn = false;
    }

    // Place teams
    buildTeam(state, TeamSide::HOME, home, HOME_FORMATION);
    buildTeam(state, TeamSide::AWAY, away, AWAY_FORMATION);

    // Ball off pitch until kickoff
    state.ball = BallState::offPitch();
    state.turnoverPending = false;
}

void simpleKickoff(GameState& state, DiceRollerBase& dice) {
    // Determine receiving team (opposite of kicking)
    TeamSide receiving = opponent(state.kickingTeam);
    state.activeTeam = receiving;

    // Reset turn counters for both teams
    state.getTeamState(TeamSide::HOME).turnNumber = 0;
    state.getTeamState(TeamSide::AWAY).turnNumber = 0;

    // Advance to first turn for receiving team
    TeamState& recvTeam = state.getTeamState(receiving);
    recvTeam.turnNumber = 1;
    recvTeam.resetForNewTurn();
    state.resetPlayersForNewTurn(receiving);

    // Kick target: center of receiving half
    int kickX = (state.kickingTeam == TeamSide::HOME) ? 18 : 7;
    int kickY = 7;

    // Scatter: D6 for distance, D8 for direction
    int dist = dice.rollD6();
    int dir = dice.rollD8();
    Position scatter = scatterDirection(dir);
    int landX = kickX + scatter.x * dist;
    int landY = kickY + scatter.y * dist;

    // Clamp to pitch
    landX = std::clamp(landX, 0, 25);
    landY = std::clamp(landY, 0, 14);

    Position landPos{static_cast<int8_t>(landX), static_cast<int8_t>(landY)};

    // Check if a player is at landing position
    Player* catcher = state.getPlayerAtPosition(landPos);
    if (catcher && catcher->teamSide == receiving &&
        catcher->state == PlayerState::STANDING) {
        // Attempt catch
        if (resolveCatch(state, catcher->id, dice, 0, nullptr)) {
            // Ball caught
        }
        // If catch fails, ball bounces (handled by resolveCatch/bounce)
    } else {
        // Ball on ground
        state.ball = BallState::onGround(landPos);
    }

    state.phase = GamePhase::PLAY;

    // Roll weather
    state.weather = weatherFromRoll(dice.roll2D6());
}

GameResult simulateGame(const TeamRoster& home, const TeamRoster& away,
                        ActionSelector homePolicy, ActionSelector awayPolicy,
                        DiceRollerBase& dice, bool useFullKickoff) {
    GameState state;
    GameResult result;

    constexpr int MAX_ACTIONS = 5000;

    auto doKickoff = [&]() {
        if (useFullKickoff) {
            resolveKickoff(state, dice, nullptr);
        } else {
            simpleKickoff(state, dice);
        }
    };

    // First half
    state.half = 1;
    state.kickingTeam = TeamSide::AWAY;  // Home receives first
    setupHalf(state, home, away);
    doKickoff();

    std::vector<Action> actions;
    int totalActions = 0;

    while (state.phase != GamePhase::GAME_OVER && totalActions < MAX_ACTIONS) {
        // Handle touchdown → setup + kickoff
        if (state.phase == GamePhase::TOUCHDOWN) {
            state.kickingTeam = opponent(state.kickingTeam);
            setupHalf(state, home, away);
            doKickoff();
            continue;
        }

        // Handle half time
        if (state.phase == GamePhase::HALF_TIME) {
            state.half = 2;
            state.kickingTeam = opponent(state.kickingTeam);
            setupHalf(state, home, away);
            doKickoff();
            continue;
        }

        // Get available actions
        actions.clear();
        getAvailableActions(state, actions);

        if (actions.empty()) {
            // No actions available — force end turn
            Action endTurn;
            endTurn.type = ActionType::END_TURN;
            executeAction(state, endTurn, dice, nullptr);
            totalActions++;
            continue;
        }

        // Select action using appropriate policy
        ActionSelector& policy = (state.activeTeam == TeamSide::HOME)
                                    ? homePolicy : awayPolicy;
        Action chosen = policy(state);

        // Execute
        executeAction(state, chosen, dice, nullptr);
        totalActions++;
    }

    result.homeScore = state.homeTeam.score;
    result.awayScore = state.awayTeam.score;
    result.totalActions = totalActions;

    return result;
}

LoggedGameResult simulateGameLogged(const TeamRoster& home, const TeamRoster& away,
                                    ActionSelector homePolicy, ActionSelector awayPolicy,
                                    DiceRollerBase& dice, bool useFullKickoff) {
    GameState state;
    LoggedGameResult logged;

    constexpr int MAX_ACTIONS = 5000;

    auto doKickoff = [&]() {
        if (useFullKickoff) {
            resolveKickoff(state, dice, nullptr);
        } else {
            simpleKickoff(state, dice);
        }
    };

    // First half
    state.half = 1;
    state.kickingTeam = TeamSide::AWAY;
    setupHalf(state, home, away);
    doKickoff();

    std::vector<Action> actions;
    int totalActions = 0;
    TeamSide lastActiveTeam = state.activeTeam;
    int lastTurnNumber = state.getTeamState(state.activeTeam).turnNumber;

    // Capture initial state features
    {
        StateLog log;
        log.perspective = state.activeTeam;
        extractFeatures(state, log.perspective, log.features);
        logged.states.push_back(log);
    }

    while (state.phase != GamePhase::GAME_OVER && totalActions < MAX_ACTIONS) {
        if (state.phase == GamePhase::TOUCHDOWN) {
            state.kickingTeam = opponent(state.kickingTeam);
            setupHalf(state, home, away);
            doKickoff();
            continue;
        }

        if (state.phase == GamePhase::HALF_TIME) {
            state.half = 2;
            state.kickingTeam = opponent(state.kickingTeam);
            setupHalf(state, home, away);
            doKickoff();
            continue;
        }

        // Check if turn changed — log features at turn boundaries
        TeamSide curTeam = state.activeTeam;
        int curTurn = state.getTeamState(curTeam).turnNumber;
        if (curTeam != lastActiveTeam || curTurn != lastTurnNumber) {
            StateLog log;
            log.perspective = curTeam;
            extractFeatures(state, log.perspective, log.features);
            logged.states.push_back(log);
            lastActiveTeam = curTeam;
            lastTurnNumber = curTurn;
        }

        actions.clear();
        getAvailableActions(state, actions);

        if (actions.empty()) {
            Action endTurn;
            endTurn.type = ActionType::END_TURN;
            executeAction(state, endTurn, dice, nullptr);
            totalActions++;
            continue;
        }

        ActionSelector& policy = (state.activeTeam == TeamSide::HOME)
                                    ? homePolicy : awayPolicy;
        Action chosen = policy(state);
        executeAction(state, chosen, dice, nullptr);
        totalActions++;
    }

    logged.result.homeScore = state.homeTeam.score;
    logged.result.awayScore = state.awayTeam.score;
    logged.result.totalActions = totalActions;

    return logged;
}

} // namespace bb
