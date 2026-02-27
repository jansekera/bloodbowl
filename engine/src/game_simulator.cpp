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

// Defensive formation for kicking team: 2-deep columns (P..P..P pattern)
// 3 columns at y=4,7,10 — each 3 deep (LOS + 2 behind) — 2 sq gaps between
// 2 deep safeties covering the gaps
constexpr FormationPos HOME_DEFENSIVE_FORMATION[11] = {
    // 3 on LOS (x=12, wide spread)
    {0, 4}, {0, 7}, {0, 10},
    // 3 column fronts (x=11, behind LOS)
    {-1, 4}, {-1, 7}, {-1, 10},
    // 3 column backs (x=10, behind fronts)
    {-2, 4}, {-2, 7}, {-2, 10},
    // 2 deep safeties (x=7, covering gaps)
    {-5, 5}, {-5, 9},
};

constexpr FormationPos AWAY_DEFENSIVE_FORMATION[11] = {
    // 3 on LOS (x=13, wide spread)
    {0, 4}, {0, 7}, {0, 10},
    // 3 column fronts (x=14, behind LOS)
    {1, 4}, {1, 7}, {1, 10},
    // 3 column backs (x=15, behind fronts)
    {2, 4}, {2, 7}, {2, 10},
    // 2 deep safeties (x=18, covering gaps)
    {5, 5}, {5, 9},
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

void setupHalf(GameState& state, const TeamRoster& home, const TeamRoster& away,
               TeamSide kickingTeam) {
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

    // Kicking team uses defensive formation, receiving team uses standard
    const auto* homeForm = (kickingTeam == TeamSide::HOME)
        ? HOME_DEFENSIVE_FORMATION : HOME_FORMATION;
    const auto* awayForm = (kickingTeam == TeamSide::AWAY)
        ? AWAY_DEFENSIVE_FORMATION : AWAY_FORMATION;

    buildTeam(state, TeamSide::HOME, home, homeForm);
    buildTeam(state, TeamSide::AWAY, away, awayForm);

    // Give the kicking team's deep safety the Kick skill (halves kick scatter)
    // Slot 10 in defensive formation = deep safety position
    {
        int kickBaseId = (kickingTeam == TeamSide::HOME) ? 1 : 12;
        Player& safety = state.getPlayer(kickBaseId + 10);
        if (safety.isOnPitch()) {
            safety.skills.add(SkillName::Kick);
        }
    }

    // Ball off pitch until kickoff
    state.ball = BallState::offPitch();
    state.turnoverPending = false;
}

// Check if kicking team has a standing player with Kick skill
bool hasKickPlayer(const GameState& state, TeamSide kickingTeam) {
    bool found = false;
    state.forEachOnPitch(kickingTeam, [&](const Player& p) {
        if (p.state == PlayerState::STANDING && p.hasSkill(SkillName::Kick))
            found = true;
    });
    return found;
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

    // Kick target: deep in receiving half (3 sq from endzone)
    int kickX = (state.kickingTeam == TeamSide::HOME) ? 22 : 3;
    int kickY = 7;

    // Scatter: D6 for distance, D8 for direction
    int dist = dice.rollD6();
    // Kick skill: halve scatter distance (round up)
    if (hasKickPlayer(state, state.kickingTeam)) {
        dist = (dist + 1) / 2;  // ceil(dist/2)
    }
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
    setupHalf(state, home, away, state.kickingTeam);
    doKickoff();

    std::vector<Action> actions;
    int totalActions = 0;

    while (state.phase != GamePhase::GAME_OVER && totalActions < MAX_ACTIONS) {
        // Handle touchdown → setup + kickoff
        if (state.phase == GamePhase::TOUCHDOWN) {
            state.kickingTeam = opponent(state.kickingTeam);
            setupHalf(state, home, away, state.kickingTeam);
            doKickoff();
            continue;
        }

        // Handle half time
        if (state.phase == GamePhase::HALF_TIME) {
            state.half = 2;
            state.kickingTeam = opponent(state.kickingTeam);
            setupHalf(state, home, away, state.kickingTeam);
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
    setupHalf(state, home, away, state.kickingTeam);
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
            setupHalf(state, home, away, state.kickingTeam);
            doKickoff();
            continue;
        }

        if (state.phase == GamePhase::HALF_TIME) {
            state.half = 2;
            state.kickingTeam = opponent(state.kickingTeam);
            setupHalf(state, home, away, state.kickingTeam);
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
