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

// Pressure formation vs fast teams: compact, more players near LOS
// 3 LOS + 4 contain line (dx=-1/+1) + 3 second row (dx=-2/+2) + 1 sweeper (dx=-4/+4)
constexpr FormationPos HOME_PRESSURE_FORMATION[11] = {
    // 3 on LOS (x=12)
    {0, 4}, {0, 7}, {0, 10},
    // 4 contain line (x=11, filling gaps)
    {-1, 3}, {-1, 6}, {-1, 8}, {-1, 11},
    // 3 second row (x=10)
    {-2, 5}, {-2, 7}, {-2, 9},
    // 1 sweeper with Kick (x=8)
    {-4, 7},
};

constexpr FormationPos AWAY_PRESSURE_FORMATION[11] = {
    // 3 on LOS (x=13)
    {0, 4}, {0, 7}, {0, 10},
    // 4 contain line (x=14)
    {1, 3}, {1, 6}, {1, 8}, {1, 11},
    // 3 second row (x=15)
    {2, 5}, {2, 7}, {2, 9},
    // 1 sweeper with Kick (x=17)
    {4, 7},
};

// Deep receiver formation for receiving team: better ball pickup
// 4 LOS + 4 second row + 2 mid backfield + 1 deep receiver (specialist in slot 10)
constexpr FormationPos HOME_DEEP_RECEIVER_FORMATION[11] = {
    // 4 on LOS (x=12)
    {0, 5}, {0, 6}, {0, 7}, {0, 8},
    // 4 second row (x=11)
    {-1, 4}, {-1, 6}, {-1, 8}, {-1, 10},
    // 2 mid backfield (x=9)
    {-3, 5}, {-3, 9},
    // 1 deep receiver (x=7, slot 10 = specialist)
    {-5, 7},
};

constexpr FormationPos AWAY_DEEP_RECEIVER_FORMATION[11] = {
    // 4 on LOS (x=13)
    {0, 5}, {0, 6}, {0, 7}, {0, 8},
    // 4 second row (x=14)
    {1, 4}, {1, 6}, {1, 8}, {1, 10},
    // 2 mid backfield (x=16)
    {3, 5}, {3, 9},
    // 1 deep receiver (x=18, slot 10 = specialist)
    {5, 7},
};

void placeTeam(GameState& state, TeamSide side, const TeamRoster& roster,
               const FormationPos formation[11]) {
    int baseId = GameState::baseIdFor(side);
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
        p.positionName = roster.positionals[templateIdx].name;
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

// Build a squad: assign identity to every slot, then put the eleven
// available starters on the pitch. Package G, layer 2 (2026-08-10): slots
// 0..10 keep exactly the identities they had before substitutes existed, so
// the starting eleven is unchanged; the remaining SQUAD_SIZE-11 slots are the
// BENCH, drawn as linemen, and they take the formation places vacated by
// anyone KO'd or hurt. That keeps the shape intact instead of leaving a hole,
// which is what layer 1 alone did.
//
// resetHalfState: true at true half boundaries (game start, half-time) -- resets the
// turn clock and reroll allowance. false for a post-touchdown drive restart, which
// only re-places players/ball and must NOT grant a fresh 8-turn clock or reroll pool.
void buildTeam(GameState& state, TeamSide side, const TeamRoster& roster,
               const FormationPos formation[11], bool resetHalfState) {
    const int baseLOS = (side == TeamSide::HOME) ? 12 : 13;
    constexpr int SQUAD = GameState::SQUAD_SIZE;
    constexpr int STARTERS = GameState::STARTERS;

    // --- 1. Which template does each slot get? -------------------------
    // Specialists fill the back of the starting eleven (backfield/second
    // row), exactly as before; everything else, bench included, is a lineman.
    int templateOf[SQUAD];
    for (int i = 0; i < SQUAD; ++i) templateOf[i] = 0;
    {
        int specSlot = STARTERS - 1;   // start filling from the backfield
        for (int t = 1; t < roster.positionalCount && specSlot >= 0; ++t) {
            int qty = std::min((int)roster.positionals[t].quantity, STARTERS);
            for (int q = 0; q < qty && specSlot >= 0; ++q) {
                templateOf[specSlot--] = t;
            }
        }
    }

    // --- 2. Identity for every squad member, starters and bench alike ---
    for (int i = 0; i < SQUAD; ++i) {
        Player& p = state.getPlayer(GameState::squadId(side, i));
        const PlayerTemplate& tpl = roster.positionals[templateOf[i]];
        p.id = GameState::squadId(side, i);
        p.teamSide = side;
        p.stats = tpl.stats;
        p.skills = tpl.skills;
        p.positionName = tpl.name;
        p.movementRemaining = p.stats.movement;
        p.hasMoved = false;
        p.hasActed = false;
        p.usedBlitz = false;
        p.lostTacklezones = false;
        p.proUsedThisTurn = false;
    }

    // --- 3. Put the available starters in their usual places ------------
    // A player is available iff setupHalfOrDrive left him in Reserves; the
    // unavailable keep their KO/INJURED/DEAD/EJECTED state untouched.
    auto place = [&](Player& p, int formSlot) {
        p.state = PlayerState::STANDING;
        p.position = {static_cast<int8_t>(baseLOS + formation[formSlot].dx),
                      formation[formSlot].y};
    };
    // Available = in Reserves AND not held out by Sweltering Heat. The heat
    // flag is consumed here, so a collapsed player misses exactly one set-up.
    auto takeAvailability = [](Player& p) {
        bool ok = (p.state == PlayerState::OFF_PITCH) && !p.outNextSetup;
        p.outNextSetup = false;
        return ok;
    };
    int vacancies[STARTERS];
    int nVacant = 0;
    for (int i = 0; i < STARTERS; ++i) {
        Player& p = state.getPlayer(GameState::squadId(side, i));
        if (takeAvailability(p)) place(p, i);
        else vacancies[nVacant++] = i;      // his spot needs a substitute
    }

    // --- 4. Substitutes come on for the missing ------------------------
    for (int b = STARTERS, v = 0; b < SQUAD && v < nVacant; ++b) {
        Player& p = state.getPlayer(GameState::squadId(side, b));
        if (!takeAvailability(p)) continue;   // bench man also out
        place(p, vacancies[v++]);
    }

    // Set team state
    TeamState& ts = state.getTeamState(side);
    ts.side = side;
    ts.score = ts.score;  // preserve score across halves/drives
    ts.hasApothecary = roster.hasApothecary;
    if (resetHalfState) {
        ts.rerolls = 3;
        ts.turnNumber = 0;
        ts.apothecaryUsed = false;
    }
    ts.resetForNewTurn();
}

} // anonymous namespace

namespace {

// Shared by setupHalf (true half boundaries) and setupDrive (post-touchdown
// restart): places both teams in formation and resets the ball/kickoff state.
// isNewHalf additionally resets each team's turn clock and reroll pool --
// must be false for setupDrive, or every touchdown grants both teams a fresh
// 8-turn half (see project_bloodbowl_audit_findings_20260703 finding 2).
void setupHalfOrDrive(GameState& state, const TeamRoster& home, const TeamRoster& away,
                      TeamSide kickingTeam, bool isNewHalf, DiceRollerBase* dice) {
    // Package G, layer 1 (2026-08-10): casualties must SURVIVE the end of a
    // drive. This loop used to reset EVERY player to OFF_PITCH with no branch
    // for KO/INJURED/DEAD, and buildTeam then stood them all up again -- so
    // each drive began with eleven healthy players a side and the dead came
    // back. Measured consequence: DEAD/game 0.00 across 3200 games, and an
    // attrition exchange of 11:1 in the dwarves' favour that showed up as a
    // 0.2-player difference at the final whistle.
    //
    // CRP injury table: 8-9 KO'd -- "At the next kick-off, before you set up
    // any players, roll for each of your players that have been KO'd. On a
    // roll of 1-3 he must remain in the KO'd box (...). On a roll of 4-6 you
    // must return the player to the Reserves box." 10-12 Casualty -- "The
    // player must miss the rest of the match."
    // Sweltering Heat is resolved at the END of the drive that just finished,
    // so it looks at who was still on the pitch. A collapsed player misses the
    // NEXT set-up only -- no recovery roll, unlike a KO.
    if (state.weather == Weather::SWELTERING_HEAT && dice) {
        for (auto& p : state.players) {
            if (p.isOnPitch() && dice->rollD6() == 1) p.outNextSetup = true;
        }
    }

    for (auto& p : state.players) {
        // KO recovery happens BEFORE anyone is set up, per the rules above.
        if (p.state == PlayerState::KO && dice) {
            if (dice->rollD6() >= 4) p.state = PlayerState::OFF_PITCH;
        }
        // Out for the rest of the match: casualties, deaths, sendings-off,
        // and any KO that failed its recovery roll. Keep the state, keep them
        // off the pitch -- do NOT hand them back to buildTeam.
        if (p.state == PlayerState::KO || p.state == PlayerState::INJURED ||
            p.state == PlayerState::DEAD || p.state == PlayerState::EJECTED) {
            p.position = {-1, -1};
            p.hasMoved = false;
            p.hasActed = false;
            p.usedBlitz = false;
            p.lostTacklezones = false;
            p.proUsedThisTurn = false;
            continue;
        }
        // NB: outNextSetup (heat) is deliberately NOT cleared here --
        // buildTeam reads it to hold the player out of THIS set-up and
        // clears it there, so he sits out exactly one drive.
        p.state = PlayerState::OFF_PITCH;   // = in Reserves, available
        p.position = {-1, -1};
        p.hasMoved = false;
        p.hasActed = false;
        p.usedBlitz = false;
        p.lostTacklezones = false;
        p.proUsedThisTurn = false;
    }

    // Classify receiver speed for roster-aware decisions
    const TeamRoster& receivingRoster = (kickingTeam == TeamSide::HOME) ? away : home;
    RosterSpeed recvSpeed = classifyRosterSpeed(receivingRoster);
    state.receiverSpeed = recvSpeed;

    // Kicking team: pressure vs fast, 2-deep columns vs slow/mixed
    const FormationPos* homeKickForm = HOME_DEFENSIVE_FORMATION;
    const FormationPos* awayKickForm = AWAY_DEFENSIVE_FORMATION;
    if (recvSpeed == RosterSpeed::FAST) {
        homeKickForm = HOME_PRESSURE_FORMATION;
        awayKickForm = AWAY_PRESSURE_FORMATION;
    }

    // Receiving team always uses deep receiver formation
    const auto* homeForm = (kickingTeam == TeamSide::HOME)
        ? homeKickForm : HOME_DEEP_RECEIVER_FORMATION;
    const auto* awayForm = (kickingTeam == TeamSide::AWAY)
        ? awayKickForm : AWAY_DEEP_RECEIVER_FORMATION;

    buildTeam(state, TeamSide::HOME, home, homeForm, isNewHalf);
    buildTeam(state, TeamSide::AWAY, away, awayForm, isNewHalf);

    // Give the kicking team's slot 10 player the Kick skill (sweeper/deep safety)
    {
        int kickBaseId = GameState::baseIdFor(kickingTeam);
        Player& safety = state.getPlayer(kickBaseId + 10);
        if (safety.isOnPitch()) {
            safety.skills.add(SkillName::Kick);
        }
    }

    // Ball off pitch until kickoff
    state.ball = BallState::offPitch();
    state.turnoverPending = false;
}

} // anonymous namespace

void setupHalf(GameState& state, const TeamRoster& home, const TeamRoster& away,
               TeamSide kickingTeam, DiceRollerBase* dice) {
    setupHalfOrDrive(state, home, away, kickingTeam, /*isNewHalf=*/true, dice);
}

void setupDrive(GameState& state, const TeamRoster& home, const TeamRoster& away,
                TeamSide kickingTeam, DiceRollerBase* dice) {
    setupHalfOrDrive(state, home, away, kickingTeam, /*isNewHalf=*/false, dice);
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

    // Advance to the receiving team's NEXT turn (2026-07-10 fix: do not
    // reset turnNumber here -- at a true half boundary setupHalf() already
    // zeroed both teams' turnNumber before doKickoff() runs, so ++ still
    // yields 1; after a post-TD kickoff mid-half, setupDrive() deliberately
    // PRESERVES turnNumber (676bb50), and this function used to stomp that
    // right back to 0/1, silently reviving the "every TD grants a fresh
    // 8-turn clock" bug the 676bb50 fix was meant to close. The kicking
    // team's own turnNumber is left untouched -- it's advanced by the
    // normal turn-end flow, not by kickoff.
    TeamState& recvTeam = state.getTeamState(receiving);
    recvTeam.turnNumber++;
    recvTeam.resetForNewTurn();
    state.resetPlayersForNewTurn(receiving);

    // Kick target: short vs fast, deep vs slow/mixed
    int kickX;
    if (state.receiverSpeed == RosterSpeed::FAST) {
        kickX = (state.kickingTeam == TeamSide::HOME) ? 18 : 7;
    } else {
        kickX = (state.kickingTeam == TeamSide::HOME) ? 22 : 3;
    }
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
    // No coin toss (yet): the opening kick is fixed. Named so the half-time
    // branch below can derive the H2 kicker from the opening, not from
    // whoever happened to kick the last H1 drive.
    const TeamSide openingKickingTeam = TeamSide::AWAY;  // Home receives first
    state.half = 1;
    state.kickingTeam = openingKickingTeam;
    setupHalf(state, home, away, state.kickingTeam, &dice);
    doKickoff();

    std::vector<Action> actions;
    int totalActions = 0;

    while (state.phase != GamePhase::GAME_OVER && totalActions < MAX_ACTIONS) {
        // Handle touchdown → setup + kickoff
        if (state.phase == GamePhase::TOUCHDOWN) {
            // The scoring team kicks off next, not simply "whoever didn't kick last".
            state.kickingTeam = state.getPlayer(state.ball.carrierId).teamSide;
            setupDrive(state, home, away, state.kickingTeam, &dice);
            doKickoff();
            continue;
        }

        // Handle half time
        if (state.phase == GamePhase::HALF_TIME) {
            state.half = 2;
            // The second half reverses the OPENING kickoff roles: the H1
            // receiver kicks. Deriving this from the current kickingTeam
            // (i.e. from the last H1 drive) handed the H2 ball back to
            // whichever team scored last in H1.
            state.kickingTeam = opponent(openingKickingTeam);
            setupHalf(state, home, away, state.kickingTeam, &dice);
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

// Helper: take a snapshot of the board state for replay
static TurnLog captureTurnSnapshot(const GameState& state) {
    TurnLog turn;
    turn.half = state.half;
    turn.turnNumber = state.getTeamState(state.activeTeam).turnNumber;
    turn.activeTeam = state.activeTeam;
    turn.homeScore = state.homeTeam.score;
    turn.awayScore = state.awayTeam.score;

    // Board (players + ball) — shared with policy-decision logging
    BoardSnapshot board = captureBoardSnapshot(state);
    turn.homePlayers = std::move(board.homePlayers);
    turn.awayPlayers = std::move(board.awayPlayers);
    turn.ballX = board.ballX;
    turn.ballY = board.ballY;
    turn.ballHeld = board.ballHeld;
    turn.ballCarrierId = board.ballCarrierId;
    turn.weather = state.weather;

    return turn;
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
    // Same fixed opening as simulateGame(); see the comment there.
    const TeamSide openingKickingTeam = TeamSide::AWAY;
    state.half = 1;
    state.kickingTeam = openingKickingTeam;
    setupHalf(state, home, away, state.kickingTeam, &dice);
    doKickoff();

    std::vector<Action> actions;
    std::vector<GameEvent> turnEvents;
    int totalActions = 0;
    TeamSide lastActiveTeam = state.activeTeam;
    int lastTurnNumber = state.getTeamState(state.activeTeam).turnNumber;

    // Capture initial state features + first turn snapshot
    {
        StateLog log;
        log.perspective = state.activeTeam;
        extractFeatures(state, log.perspective, log.features);
        logged.states.push_back(log);

        logged.turnLogs.push_back(captureTurnSnapshot(state));
    }

    while (state.phase != GamePhase::GAME_OVER && totalActions < MAX_ACTIONS) {
        if (state.phase == GamePhase::TOUCHDOWN) {
            // Mark touchdown in current turn log
            if (!logged.turnLogs.empty()) {
                logged.turnLogs.back().touchdown = true;
            }
            // The scoring team kicks off next, not simply "whoever didn't kick last".
            state.kickingTeam = state.getPlayer(state.ball.carrierId).teamSide;
            setupDrive(state, home, away, state.kickingTeam, &dice);
            doKickoff();
            continue;
        }

        if (state.phase == GamePhase::HALF_TIME) {
            state.half = 2;
            // H2 reverses the OPENING kickoff roles, not the last H1 drive;
            // see the comment in simulateGame().
            state.kickingTeam = opponent(openingKickingTeam);
            setupHalf(state, home, away, state.kickingTeam, &dice);
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

            // Save previous turn events and start new turn log
            logged.turnLogs.push_back(captureTurnSnapshot(state));
            turnEvents.clear();

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

        // Execute with event capture
        turnEvents.clear();
        executeAction(state, chosen, dice, &turnEvents);

        // Append events to current turn log
        if (!logged.turnLogs.empty()) {
            auto& curLog = logged.turnLogs.back();
            for (auto& ev : turnEvents) {
                curLog.events.push_back(ev);
                if (ev.type == GameEvent::Type::TURNOVER) curLog.turnover = true;
                if (ev.type == GameEvent::Type::TOUCHDOWN) curLog.touchdown = true;
            }
        }

        totalActions++;
    }

    logged.result.homeScore = state.homeTeam.score;
    logged.result.awayScore = state.awayTeam.score;
    logged.result.totalActions = totalActions;

    return logged;
}

} // namespace bb
