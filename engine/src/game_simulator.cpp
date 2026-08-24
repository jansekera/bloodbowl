#include "bb/game_simulator.h"
#include "bb/cage_advance.h"
#include "bb/action_resolver.h"
#include "bb/ball_handler.h"
#include "bb/kickoff_handler.h"
#include "bb/helpers.h"
#include "bb/turn_handler.h"
#include <algorithm>
#include <vector>

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

    // --- 3. Who is actually available -----------------------------------
    // Available = in Reserves AND not held out by Sweltering Heat. The heat
    // flag is consumed here, so a collapsed player misses exactly one set-up.
    auto takeAvailability = [](Player& p) {
        bool ok = (p.state == PlayerState::OFF_PITCH) && !p.outNextSetup;
        p.outNextSetup = false;
        return ok;
    };
    int available[SQUAD];
    int nAvail = 0;
    for (int i = 0; i < SQUAD; ++i) {
        Player& p = state.getPlayer(GameState::squadId(side, i));
        bool ok = takeAvailability(p);          // called once for everyone:
        if (ok && nAvail < STARTERS) {          // it clears the heat flag
            available[nAvail++] = i;
        }
    }

    // --- 4. Which formation slot does each of them stand in? -------------
    // Slots carry ROLES; who fills them must not be decided by where a
    // positional happens to sit in the roster list. Until 2026-08-11 a
    // starter simply stood in the slot with his own squad index, and for the
    // dwarves that inverted the two placements that matter most: the deepest
    // slot -- the one nearest where a deep kick lands -- went to a Longbeard
    // (MA4, AG2, no Sure Hands) while both Runners ended up on or beside the
    // line of scrimmage. Measured on the 07-30 corpus: median Runner-to-ball
    // distance at turn 1 was 7.5 squares and 14 of 40 Runners could not reach
    // the ball at all, so the pickup fell to whoever was standing deep -- and
    // a Longbeard carrying advances 1.50 squares a turn against a Runner's
    // 3.41 (project_bloodbowl_setup_slot_rootcause_20260811).
    //
    // Two rules, both read off the slot's depth rather than off any race
    // label: the deepest slot takes the best ball handler, and the line of
    // scrimmage never takes one.
    //
    // Assignment runs over whoever is AVAILABLE, substitutes included, not
    // over the nominal starting eleven. Package G gave casualties persistence
    // and the squad a bench on 2026-08-10, so a KO'd Runner is now a normal
    // occurrence -- and slotting a bench lineman into the hole he left would
    // put a Longbeard back in the deep slot, reintroducing the very defect
    // the role rules exist to prevent. Identity (templateOf) is deliberately
    // untouched: it must stay stable across drives, or a KO'd player would
    // come back as somebody else.
    int formSlotOf[SQUAD];
    for (int i = 0; i < SQUAD; ++i) formSlotOf[i] = -1;
    {
        auto handlerScore = [&](int k) {
            const PlayerTemplate& t = roster.positionals[templateOf[available[k]]];
            return (t.skills.has(SkillName::SureHands) ? 100 : 0)
                 + t.stats.agility * 10 + t.stats.movement;
        };
        // Bodies on the line: armour first, Block next, and a handler is
        // pushed out of contention entirely -- he is worth more deep than he
        // is worth absorbing the opponent's opening blocks.
        auto lineScore = [&](int k) {
            const PlayerTemplate& t = roster.positionals[templateOf[available[k]]];
            return t.stats.armour * 10 + (t.skills.has(SkillName::Block) ? 5 : 0)
                 - (t.skills.has(SkillName::SureHands) ? 100 : 0);
        };

        int maxDepth = 0;
        for (int s2 = 0; s2 < STARTERS; ++s2) {
            maxDepth = std::max(maxDepth, std::abs(static_cast<int>(formation[s2].dx)));
        }

        bool slotTaken[STARTERS] = {};
        std::vector<bool> manTaken(nAvail, false);

        auto claim = [&](int k, int slot) {
            formSlotOf[available[k]] = slot;
            manTaken[k] = true;
            slotTaken[slot] = true;
        };
        auto bestMan = [&](auto score) {
            int best = -1;
            for (int k = 0; k < nAvail; ++k) {
                if (manTaken[k]) continue;
                if (best < 0 || score(k) > score(best)) best = k;
            }
            return best;
        };

        for (int s2 = 0; s2 < STARTERS; ++s2) {   // deepest slots first
            if (std::abs(static_cast<int>(formation[s2].dx)) != maxDepth) continue;
            int k = bestMan(handlerScore);
            if (k >= 0) claim(k, s2);
        }
        for (int s2 = 0; s2 < STARTERS; ++s2) {   // then the line of scrimmage
            if (slotTaken[s2] || formation[s2].dx != 0) continue;
            int k = bestMan(lineScore);
            if (k >= 0) claim(k, s2);
        }
        for (int s2 = 0; s2 < STARTERS; ++s2) {   // everyone else keeps order
            if (slotTaken[s2]) continue;
            int k = bestMan([](int) { return 0; });
            if (k >= 0) claim(k, s2);
        }
    }

    // --- 5. Put them on the pitch ---------------------------------------
    for (int k = 0; k < nAvail; ++k) {
        Player& p = state.getPlayer(GameState::squadId(side, available[k]));
        int slot = formSlotOf[available[k]];
        if (slot < 0) continue;
        p.state = PlayerState::STANDING;
        p.position = {static_cast<int8_t>(baseLOS + formation[slot].dx),
                      formation[slot].y};
        p.playedThisDrive = true;
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
    // The drive that just ended is over, so a secret weapon that was ON THE
    // PITCH for it is sent off now -- CRP: "Once a drive ends that this player
    // has played in at any point."
    //
    // The drive that just ended is over, so a secret weapon who took the field
    // for it is sent off -- CRP: "Once a drive ends that this player has
    // played in at any point."
    //
    // Read from a recorded fact, not inferred from his current state. The
    // tempting inference is that a KO'd secret weapon must have played,
    // because playing an earlier drive would already have sent him off -- but
    // a bribe breaks exactly that chain: it cancels the ejection and leaves
    // him in the game, so he can sit out the NEXT drive in the KO box and be
    // sent off for a drive he never took the field in (user, 2026-08-11).
    // Bribes are not implemented yet; the flag costs nothing and does not
    // depend on that staying true.
    for (auto& p : state.players) {
        if (p.hasSkill(SkillName::SecretWeapon) && p.playedThisDrive) {
            p.state = PlayerState::EJECTED;
            p.position = {-1, -1};
        }
    }
    for (auto& p : state.players) p.playedThisDrive = false;
    // Take Root (l. 8575): "his MA is considered 0 UNTIL A DRIVE ENDS" --
    // tohle je ten konec drivu, at uz po TD nebo o poloćase.
    for (auto& p : state.players) p.rooted = false;

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

    // Give the kicking team's sweeper/deep safety the Kick skill. This used
    // to read squad slot 10, which was the deepest man only because identity
    // and formation slot were the same index; now that starters are placed by
    // role (buildTeam step 2b) the deep man is found by looking, which is what
    // the rule meant all along. CRP also forbids the kicker standing on the
    // line of scrimmage or in a wide zone, and the deepest slot is neither.
    {
        int losX = (kickingTeam == TeamSide::HOME) ? 12 : 13;
        Player* safety = nullptr;
        int bestDepth = -1;
        state.forEachOnPitch(kickingTeam, [&](const Player& p) {
            int depth = std::abs(p.position.x - losX);
            if (depth > bestDepth) {
                bestDepth = depth;
                safety = &state.getPlayer(p.id);
            }
        });
        if (safety) safety->skills.add(SkillName::Kick);
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
// BB2016 l. 304-307: los pred zapasem. Do 24.08.2026 se nehazel VUBEC --
// `openingKickingTeam` byla konstanta AWAY, takze domaci zahajovali 1. pulku
// a hoste 2. pulku v 18 000 z 18 000 her kriz. korpusu. Merenim vyslo, ze to
// neni kosmetika: v zrcadlovych utkanich (stejna rasa na obou stranach, tedy
// rozdil ma byt sum) vyhravali hoste o +4,87σ (human), +4,68σ (orc), +2,84σ
// (skaven), +2,61σ (wood-elf). Jediny trpaslik byl imunni (-0,40σ) -- na TD
// potrebuje ~7 kol, takze posledni drive pulky neumi promenit.
TossElection defaultTossElection(const TeamRoster& roster) {
    // Krehke a rychle soupisky volí UTOK, vsechny ostatni OBRANU -- zduvodneni
    // je v hlavicce. Rychlost soupisky uz umime klasifikovat (⌀ MA jedenactky).
    return (classifyRosterSpeed(roster) == RosterSpeed::FAST)
               ? TossElection::RECEIVE : TossElection::KICK;
}

TeamSide rollOpeningKickingTeam(DiceRollerBase& dice, TossElection winnerElects) {
    // (1) los: D6, 1-3 vyhrava HOME, 4-6 AWAY
    const TeamSide tossWinner = (dice.rollD6() <= 3) ? TeamSide::HOME : TeamSide::AWAY;
    // (2) vitéz losu volí; kdo NEkope, ten prijima
    return (winnerElects == TossElection::RECEIVE) ? opponent(tossWinner) : tossWinner;
}

TeamSide rollOpeningKickingTeam(DiceRollerBase& dice,
                                const TeamRoster& home, const TeamRoster& away) {
    const TeamSide tossWinner = (dice.rollD6() <= 3) ? TeamSide::HOME : TeamSide::AWAY;
    const TossElection e =
        defaultTossElection(tossWinner == TeamSide::HOME ? home : away);
    return (e == TossElection::RECEIVE) ? opponent(tossWinner) : tossWinner;
}

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
    // Kick skill, BB2016 l. 8211-8213: "you may choose to halve the number of
    // squares that the ball scatters on kick-off, ROUNDING ANY FRACTIONS DOWN
    // (i.e., 1 = 0, 2-3 = 1, 4-5 = 2, 6 = 3)". Do 24.08.2026 se zaokrouhlovalo
    // NAHORU, takze u tri hodu ze sesti (1, 3, 5) mic uletel o pole dal, nez ma.
    if (hasKickPlayer(state, state.kickingTeam)) {
        dist = dist / 2;  // floor -- pravidlo dava i tabulku, viz vys
    }
    int dir = dice.rollD8();
    Position scatter = scatterDirection(dir);
    int landX = kickX + scatter.x * dist;
    int landY = kickY + scatter.y * dist;

    // ⛔ F10 (24.08.2026): tady se dosud mic ORIZL na hriste (`clamp`), takze
    // vykop nikdy neodletel ven ani do vlastni poloviny -- a TOUCHBACK proto
    // v teto ceste NEEXISTOVAL. `resolveKickoff` (plna cesta) ho ma, ale korpus
    // bezi na teto, zjednodusene. BB2016 l. 275-283.
    Position landPos{static_cast<int8_t>(landX), static_cast<int8_t>(landY)};

    // Polovina PRIJIMAJICIHO: HOME brani nizka x (LOS 12/13), AWAY vysoka.
    auto inReceivingHalf = [&](Position p) {
        return receiving == TeamSide::AWAY ? p.x >= 13 : p.x <= 12;
    };

    // l. 281-283: "the receiving coach is awarded a 'touchback' and must give
    // the ball to ANY PLAYER IN HIS TEAM". Kteremu, to je VOLBA trenéra, ne
    // pravidlo -- davame ho nejhlubsimu stojicimu hráči (prednostne se Sure
    // Hands), tedy tomu, kdo je nejdal od LOS a nejmene ohrozeny.
    auto awardTouchback = [&]() {
        int losX = (state.kickingTeam == TeamSide::HOME) ? 12 : 13;
        Player* pick = nullptr;
        int bestScore = -1;
        state.forEachOnPitch(receiving, [&](const Player& p) {
            if (p.state != PlayerState::STANDING) return;
            if (p.hasSkill(SkillName::NoHands)) return;
            int score = std::abs(p.position.x - losX) +
                        (p.hasSkill(SkillName::SureHands) ? 100 : 0);
            if (score > bestScore) { bestScore = score; pick = &state.getPlayer(p.id); }
        });
        if (pick) {
            state.ball = BallState::carried(pick->position, pick->id);
        } else {
            state.ball = BallState::onGround(Position{
                static_cast<int8_t>(std::clamp(landX, 0, 25)),
                static_cast<int8_t>(std::clamp(landY, 0, 14))});
        }
    };

    // Put the ball down BEFORE anyone tries to catch it. Until 2026-08-11 it
    // was placed only in the else-branch, so a kick landing on a standing
    // receiver who then dropped it left the ball where setupHalf had put it:
    // off the pitch, at (-1,-1). resolveCatch does nothing with the ball when
    // it fails -- the comment here claimed it bounced, and it does not -- so
    // the drive carried on with no ball anywhere.
    //
    // Seen once in 120 games (g0040): an entire second half of 108 moves, 19
    // blocks, three fouls and a casualty, played without a ball. The full
    // kickoff path (resolveKickoff) always had this right; only the simplified
    // one was wrong, and the simplified one is what the corpora run on.
    if (!landPos.isOnPitch() || !inReceivingHalf(landPos)) {
        // l. 280-282: "If the ball scatters or bounces off the pitch OR INTO
        // THE KICKING TEAM'S HALF, the receiving coach is awarded a touchback."
        awardTouchback();
    } else {
        state.ball = BallState::onGround(landPos);

        Player* catcher = state.getPlayerAtPosition(landPos);
        if (catcher && catcher->teamSide == receiving &&
            catcher->state == PlayerState::STANDING) {
            if (!resolveCatch(state, catcher->id, dice, 0, nullptr)) {
                resolveBounce(state, landPos, dice, 0, nullptr);
            }
        } else if (!catcher) {
            // l. 277-278: "If the ball lands in an empty square it will BOUNCE
            // ONE MORE SQUARE." Ten odraz tu dosud chybel uplne -- mic po
            // vykopu proste lezel v poli dopadu.
            resolveBounce(state, landPos, dice, 0, nullptr);
        }

        // A odraz muze mic dostat ven nebo do kopajici poloviny -- pak je to
        // touchback taky (tataz veta l. 280-282).
        // ⚠️ Aproximace: nas `resolveBounce` po vyletu z hriste rovnou vhazuje
        // z davu, takze mezistav neodpovida pravidlum; koncovy stav ano.
        if (!state.ball.isHeld && !inReceivingHalf(state.ball.position)) {
            awardTouchback();
        }
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
    // BB2016 l. 304-307: los rozhoduje, kdo kope jako prvni. Pojmenovane, aby
    // si vetev half-time nize odvodila kopajiciho v H2 z OTVIRACIHO losu, ne
    // z toho, kdo nahodou kopal posledni drive H1 (l. 1016-1017).
    const TeamSide openingKickingTeam = rollOpeningKickingTeam(dice, home, away);
    state.half = 1;
    state.kickingTeam = openingKickingTeam;
    setupHalf(state, home, away, state.kickingTeam, &dice);
    doKickoff();

    std::vector<Action> actions;
    int totalActions = 0;

    while (state.phase != GamePhase::GAME_OVER && totalActions < MAX_ACTIONS) {
        // The half can end without anyone taking an action. checkHalfOver was
        // only ever consulted at the end of executeAction, but the kickoff
        // advances the receiving team's turn number by itself
        // (kickoff_handler.cpp) -- so a touchdown scored on turn 8 was
        // followed by a kickoff that bumped the receiver to turn 9, and the
        // ninth turn was played out before anything asked whether the half
        // was over. Measured on the 08-11 corpus: 9 such turns across 120
        // games. Riot can do the same by pushing the marker forward.
        //
        // This matters more than its frequency: every schedule in the dwarf
        // drill is arithmetic over exactly eight turns.
        if (state.phase == GamePhase::PLAY && checkHalfOver(state)) {
            state.phase = (state.half >= 2) ? GamePhase::GAME_OVER
                                            : GamePhase::HALF_TIME;
            continue;
        }

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

    // K9b (08-18): odpor v koridoru se počítá KAŽDÉ kolo, nezávisle na tom,
    // jestli běžel plánovač klece. Jen pro naše kolo a jen když držíme míč --
    // jinak predikát nedává smysl a zapisuje se -1 (N/A), ne nula.
    if (state.ball.isHeld && state.ball.carrierId > 0) {
        const Player& car = state.getPlayer(state.ball.carrierId);
        if (car.teamSide == state.activeTeam) {
            turn.corridorResistance =
                static_cast<int8_t>(corridorResistance(state, car, state.activeTeam));
            turn.corridorStrength =
                static_cast<int16_t>(corridorStrength(state, car, state.activeTeam));
            // Tempo -- viz cage_advance.h. Razítkuje se KAŽDÉ kolo, protože
            // plánovač, kde to dosud žilo, v produkci neběží.
            TempoSnapshot t = tempoSnapshot(state, car, state.activeTeam);
            turn.requiredPace = t.required;
            turn.achievablePace = t.achievable;
            turn.distToEndzone = t.distToEndzone;
        } else {
            turn.corridorResistance = -1;
            turn.corridorStrength = -1;
        }
    }

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
    // Los stejne jako v simulateGame(); viz komentar tam.
    const TeamSide openingKickingTeam = rollOpeningKickingTeam(dice, home, away);
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
        resetTurnPlanRecord();
    }

    while (state.phase != GamePhase::GAME_OVER && totalActions < MAX_ACTIONS) {
        // The half can end without anyone taking an action. checkHalfOver was
        // only ever consulted at the end of executeAction, but the kickoff
        // advances the receiving team's turn number by itself
        // (kickoff_handler.cpp) -- so a touchdown scored on turn 8 was
        // followed by a kickoff that bumped the receiver to turn 9, and the
        // ninth turn was played out before anything asked whether the half
        // was over. Measured on the 08-11 corpus: 9 such turns across 120
        // games. Riot can do the same by pushing the marker forward.
        //
        // This matters more than its frequency: every schedule in the dwarf
        // drill is arithmetic over exactly eight turns.
        if (state.phase == GamePhase::PLAY && checkHalfOver(state)) {
            state.phase = (state.half >= 2) ? GamePhase::GAME_OVER
                                            : GamePhase::HALF_TIME;
            continue;
        }

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
            resetTurnPlanRecord();   // the planner writes it during this turn

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
            // The planner runs once per team-turn, inside the policy call
            // above; take the record the first time it appears.
            if (!curLog.plan.written) curLog.plan = currentTurnPlanRecord();
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
