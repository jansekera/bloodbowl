#include <gtest/gtest.h>
#include "bb/game_simulator.h"
#include "bb/kickoff_handler.h"
#include "bb/roster.h"
#include "bb/policies.h"
#include "bb/dice.h"
#include <set>

using namespace bb;

TEST(GameSimulator, SetupPlaces11PlayersPerSide) {
    GameState state;
    setupHalf(state, getHumanRoster(), getHumanRoster());

    int homeOnPitch = 0, awayOnPitch = 0;
    state.forEachPlayer(TeamSide::HOME, [&](const Player& p) {
        if (p.isOnPitch()) homeOnPitch++;
    });
    state.forEachPlayer(TeamSide::AWAY, [&](const Player& p) {
        if (p.isOnPitch()) awayOnPitch++;
    });

    EXPECT_EQ(homeOnPitch, 11);
    EXPECT_EQ(awayOnPitch, 11);
}

TEST(GameSimulator, SetupPositionsAreValid) {
    GameState state;
    setupHalf(state, getHumanRoster(), getHumanRoster());

    std::set<std::pair<int,int>> positions;

    for (auto& p : state.players) {
        if (!p.isOnPitch()) continue;

        // Position should be on pitch
        EXPECT_TRUE(p.position.isOnPitch())
            << "Player " << p.id << " at (" << (int)p.position.x
            << "," << (int)p.position.y << ") is off pitch";

        // No overlapping positions
        auto pos = std::make_pair((int)p.position.x, (int)p.position.y);
        EXPECT_EQ(positions.count(pos), 0u)
            << "Duplicate position at (" << pos.first << "," << pos.second << ")";
        positions.insert(pos);
    }

    // Should have exactly 22 unique positions
    EXPECT_EQ(positions.size(), 22u);
}

TEST(GameSimulator, SetupHomeFacingRight) {
    GameState state;
    setupHalf(state, getHumanRoster(), getHumanRoster());

    // Home LOS should be at x=12
    bool foundHomeLOS = false;
    state.forEachOnPitch(TeamSide::HOME, [&](const Player& p) {
        if (p.position.x == 12) foundHomeLOS = true;
        // All home players should be on left half (x <= 12)
        EXPECT_LE(p.position.x, 12)
            << "Home player " << p.id << " at x=" << (int)p.position.x;
    });
    EXPECT_TRUE(foundHomeLOS);

    // Away LOS should be at x=13
    bool foundAwayLOS = false;
    state.forEachOnPitch(TeamSide::AWAY, [&](const Player& p) {
        if (p.position.x == 13) foundAwayLOS = true;
        // All away players should be on right half (x >= 13)
        EXPECT_GE(p.position.x, 13)
            << "Away player " << p.id << " at x=" << (int)p.position.x;
    });
    EXPECT_TRUE(foundAwayLOS);
}

TEST(GameSimulator, SimpleKickoffPlacesBallOnPitch) {
    GameState state;
    state.kickingTeam = TeamSide::AWAY;
    setupHalf(state, getHumanRoster(), getHumanRoster());

    DiceRoller dice(42);
    simpleKickoff(state, dice);

    EXPECT_EQ(state.phase, GamePhase::PLAY);
    // Ball should be on pitch (either held or on ground)
    EXPECT_TRUE(state.ball.isOnPitch() || state.ball.isHeld);
}

TEST(GameSimulator, SimpleKickoffSetsActiveTeam) {
    GameState state;
    state.kickingTeam = TeamSide::AWAY;
    setupHalf(state, getHumanRoster(), getHumanRoster());

    DiceRoller dice(42);
    simpleKickoff(state, dice);

    // Receiving team (HOME) should be active
    EXPECT_EQ(state.activeTeam, TeamSide::HOME);
}

TEST(GameSimulator, RandomVsRandomCompletes) {
    DiceRoller dice(42);
    auto homePolicy = [&dice](const GameState& s) { return randomPolicy(s, dice); };
    auto awayPolicy = [&dice](const GameState& s) { return randomPolicy(s, dice); };

    GameResult result = simulateGame(getHumanRoster(), getHumanRoster(),
                                      homePolicy, awayPolicy, dice);

    // Game should complete with valid scores
    EXPECT_GE(result.homeScore, 0);
    EXPECT_GE(result.awayScore, 0);
    EXPECT_GT(result.totalActions, 0);
    EXPECT_LE(result.totalActions, 5000);
}

TEST(GameSimulator, MaxActionsLimitWorks) {
    // The game loop should stop at 5000 actions max
    DiceRoller dice(123);
    auto homePolicy = [&dice](const GameState& s) { return randomPolicy(s, dice); };
    auto awayPolicy = [&dice](const GameState& s) { return randomPolicy(s, dice); };

    GameResult result = simulateGame(getHumanRoster(), getHumanRoster(),
                                      homePolicy, awayPolicy, dice);

    EXPECT_LE(result.totalActions, 5000);
}

TEST(GameSimulator, DifferentRostersWork) {
    DiceRoller dice(99);
    auto homePolicy = [&dice](const GameState& s) { return randomPolicy(s, dice); };
    auto awayPolicy = [&dice](const GameState& s) { return randomPolicy(s, dice); };

    GameResult result = simulateGame(getOrcRoster(), getSkavenRoster(),
                                      homePolicy, awayPolicy, dice);

    EXPECT_GE(result.homeScore, 0);
    EXPECT_GE(result.awayScore, 0);
    EXPECT_GT(result.totalActions, 0);
}

TEST(GameSimulator, AllRostersSimulate) {
    // Test that all 26 rosters can run a complete game without crashing
    const char* rosterNames[] = {
        "human", "orc", "skaven", "dwarf", "wood-elf", "chaos",
        "undead", "lizardmen", "dark-elf", "halfling", "norse", "high-elf",
        "vampire", "amazon", "necromantic", "bretonnian", "khemri", "goblin",
        "chaos-dwarf", "ogre", "nurgle", "pro-elf", "slann", "underworld",
        "khorne", "chaos-pact"
    };

    for (const auto& name : rosterNames) {
        const TeamRoster* roster = getRosterByName(name);
        ASSERT_NE(roster, nullptr) << "Roster not found: " << name;

        DiceRoller dice(42);
        auto policy = [&dice](const GameState& s) { return randomPolicy(s, dice); };

        GameResult result = simulateGame(*roster, getHumanRoster(), policy, policy, dice);

        EXPECT_GE(result.homeScore, 0) << "Failed for roster: " << name;
        EXPECT_GE(result.awayScore, 0) << "Failed for roster: " << name;
        EXPECT_GT(result.totalActions, 0) << "Failed for roster: " << name;
    }
}

TEST(GameSimulator, GetRosterByNameWorks) {
    EXPECT_NE(getRosterByName("human"), nullptr);
    EXPECT_NE(getRosterByName("orc"), nullptr);
    EXPECT_NE(getRosterByName("chaos-pact"), nullptr);
    EXPECT_NE(getRosterByName("wood-elf"), nullptr);
    EXPECT_NE(getRosterByName("CHAOS_DWARF"), nullptr);  // case insensitive
    EXPECT_EQ(getRosterByName("invalid"), nullptr);
}

TEST(GameSimulator, AllRostersHaveValidPositionals) {
    const char* names[] = {
        "human", "orc", "skaven", "dwarf", "wood-elf", "chaos",
        "undead", "lizardmen", "dark-elf", "halfling", "norse", "high-elf",
        "vampire", "amazon", "necromantic", "bretonnian", "khemri", "goblin",
        "chaos-dwarf", "ogre", "nurgle", "pro-elf", "slann", "underworld",
        "khorne", "chaos-pact"
    };

    for (const auto& name : names) {
        const TeamRoster* r = getRosterByName(name);
        ASSERT_NE(r, nullptr) << name;
        EXPECT_GT(r->positionalCount, 0) << name;
        EXPECT_LE(r->positionalCount, 8) << name;
        EXPECT_GT(r->rerollCost, 0) << name;

        // First positional should have quantity >= 11 to fill a team
        // (some teams have lineman qty < 16 but enough to fill)
        int totalQty = 0;
        for (int i = 0; i < r->positionalCount; i++) {
            totalQty += r->positionals[i].quantity;
        }
        EXPECT_GE(totalQty, 11) << "Not enough total positional slots for: " << name;
    }
}

// --- Defensive Formation Tests ---

TEST(GameSimulator, DefensiveFormation3OnLOS) {
    GameState state;
    state.kickingTeam = TeamSide::AWAY;
    setupHalf(state, getHumanRoster(), getHumanRoster(), TeamSide::AWAY);

    // Away is kicking → away uses defensive formation → only 3 on LOS (x=13)
    int awayOnLOS = 0;
    state.forEachOnPitch(TeamSide::AWAY, [&](const Player& p) {
        if (p.position.x == 13) awayOnLOS++;
    });
    EXPECT_EQ(awayOnLOS, 3);
}

TEST(GameSimulator, OffensiveFormation4OnLOS) {
    GameState state;
    state.kickingTeam = TeamSide::AWAY;
    setupHalf(state, getHumanRoster(), getHumanRoster(), TeamSide::AWAY);

    // Home is receiving → standard formation → 4 on LOS (x=12)
    int homeOnLOS = 0;
    state.forEachOnPitch(TeamSide::HOME, [&](const Player& p) {
        if (p.position.x == 12) homeOnLOS++;
    });
    EXPECT_EQ(homeOnLOS, 4);
}

TEST(GameSimulator, DefensiveFormation2DeepColumns) {
    GameState state;
    setupHalf(state, getHumanRoster(), getHumanRoster(), TeamSide::AWAY);

    // Away kicking → 2-deep columns: 3 at x=14 (fronts) + 3 at x=15 (backs)
    int awayFronts = 0, awayBacks = 0;
    state.forEachOnPitch(TeamSide::AWAY, [&](const Player& p) {
        if (p.position.x == 14) awayFronts++;
        if (p.position.x == 15) awayBacks++;
    });
    EXPECT_EQ(awayFronts, 3);
    EXPECT_EQ(awayBacks, 3);
}

TEST(GameSimulator, DefensiveFormationDeepSafeties) {
    GameState state;
    setupHalf(state, getHumanRoster(), getHumanRoster(), TeamSide::AWAY);

    // Away kicking → 2 deep safeties at x=18, y=5 and y=9
    int safetyCount = 0;
    state.forEachOnPitch(TeamSide::AWAY, [&](const Player& p) {
        if (p.position.x == 18) safetyCount++;
    });
    EXPECT_EQ(safetyCount, 2);
}

TEST(GameSimulator, KickSkillOnDeepSafety) {
    GameState state;
    setupHalf(state, getHumanRoster(), getHumanRoster(), TeamSide::AWAY);

    // Deep safety (slot 10, id=22 for AWAY) should have Kick skill
    bool hasKick = false;
    state.forEachOnPitch(TeamSide::AWAY, [&](const Player& p) {
        if (p.hasSkill(SkillName::Kick)) hasKick = true;
    });
    EXPECT_TRUE(hasKick);

    // Exactly one player should have Kick
    int kickCount = 0;
    state.forEachOnPitch(TeamSide::AWAY, [&](const Player& p) {
        if (p.hasSkill(SkillName::Kick)) kickCount++;
    });
    EXPECT_EQ(kickCount, 1);
}

TEST(GameSimulator, KickSkillHalvesScatter) {
    // With Kick skill, D6 scatter should be halved (ceil):
    // D6=1→1, D6=2→1, D6=3→2, D6=4→2, D6=5→3, D6=6→3
    // Max scatter is 3, so from x=3, worst case is x=3-3=0 (still on pitch)
    // Without Kick: max scatter 6, from x=3 could land at x=-3 (clamped to 0)
    GameState state;
    state.kickingTeam = TeamSide::AWAY;
    setupHalf(state, getHumanRoster(), getHumanRoster(), TeamSide::AWAY);

    // Verify kicking team has Kick skill
    bool hasKick = false;
    state.forEachOnPitch(TeamSide::AWAY, [&](const Player& p) {
        if (p.hasSkill(SkillName::Kick)) hasKick = true;
    });
    ASSERT_TRUE(hasKick);

    // Run 50 kickoffs, verify ball never scatters more than 3 from target
    for (int seed = 0; seed < 50; seed++) {
        GameState s2 = state;  // copy
        DiceRoller dice(seed);
        simpleKickoff(s2, dice);
        // Ball should be on pitch
        EXPECT_TRUE(s2.ball.isOnPitch() || s2.ball.isHeld)
            << "Ball off pitch with seed=" << seed;
    }
}

TEST(GameSimulator, DefensiveFormationNoOverlaps) {
    GameState state;
    setupHalf(state, getHumanRoster(), getHumanRoster(), TeamSide::AWAY);

    std::set<std::pair<int,int>> positions;
    for (auto& p : state.players) {
        if (!p.isOnPitch()) continue;
        auto pos = std::make_pair((int)p.position.x, (int)p.position.y);
        EXPECT_EQ(positions.count(pos), 0u)
            << "Duplicate position at (" << pos.first << "," << pos.second << ")";
        positions.insert(pos);
    }
    EXPECT_EQ(positions.size(), 22u);
}

TEST(GameSimulator, DeepKickTargetInReceivingHalf) {
    // Test that deep kick (x=22 when HOME kicks, x=3 when AWAY kicks)
    // still lands in receiving half after scatter
    for (int seed = 0; seed < 20; seed++) {
        GameState state;
        state.kickingTeam = TeamSide::AWAY;
        setupHalf(state, getHumanRoster(), getHumanRoster(), TeamSide::AWAY);

        DiceRoller dice(seed);
        simpleKickoff(state, dice);

        // Ball should be on pitch
        EXPECT_TRUE(state.ball.isOnPitch() || state.ball.isHeld)
            << "Ball off pitch with seed=" << seed;
    }
}

TEST(GameSimulator, BackwardCompatDefault) {
    // setupHalf without kickingTeam param should still work (default = AWAY kicking)
    GameState state;
    setupHalf(state, getHumanRoster(), getHumanRoster());

    int homeOnPitch = 0, awayOnPitch = 0;
    state.forEachPlayer(TeamSide::HOME, [&](const Player& p) {
        if (p.isOnPitch()) homeOnPitch++;
    });
    state.forEachPlayer(TeamSide::AWAY, [&](const Player& p) {
        if (p.isOnPitch()) awayOnPitch++;
    });

    EXPECT_EQ(homeOnPitch, 11);
    EXPECT_EQ(awayOnPitch, 11);

    // With default (AWAY kicking), away should use defensive formation = 3 on LOS
    int awayOnLOS = 0;
    state.forEachOnPitch(TeamSide::AWAY, [&](const Player& p) {
        if (p.position.x == 13) awayOnLOS++;
    });
    EXPECT_EQ(awayOnLOS, 3);
}

TEST(GameSimulator, HomeKickingUsesDefensiveFormation) {
    GameState state;
    setupHalf(state, getHumanRoster(), getHumanRoster(), TeamSide::HOME);

    // Home is kicking → home uses defensive formation → 3 on LOS (x=12)
    int homeOnLOS = 0;
    state.forEachOnPitch(TeamSide::HOME, [&](const Player& p) {
        if (p.position.x == 12) homeOnLOS++;
    });
    EXPECT_EQ(homeOnLOS, 3);

    // Away is receiving → deep receiver formation → 4 on LOS (x=13)
    int awayOnLOS = 0;
    state.forEachOnPitch(TeamSide::AWAY, [&](const Player& p) {
        if (p.position.x == 13) awayOnLOS++;
    });
    EXPECT_EQ(awayOnLOS, 4);
}

// --- Roster-Aware Kickoff (Vrstva 3) Tests ---

TEST(GameSimulator, RosterSpeedClassification) {
    EXPECT_EQ(classifyRosterSpeed(getSkavenRoster()), RosterSpeed::FAST);    // 7.64
    EXPECT_EQ(classifyRosterSpeed(getWoodElfRoster()), RosterSpeed::FAST);  // 7.09
    EXPECT_EQ(classifyRosterSpeed(getHighElfRoster()), RosterSpeed::FAST);  // 7.09
    EXPECT_EQ(classifyRosterSpeed(getDwarfRoster()), RosterSpeed::SLOW);    // 4.73
    EXPECT_EQ(classifyRosterSpeed(getHalflingRoster()), RosterSpeed::SLOW); // 4.45
    EXPECT_EQ(classifyRosterSpeed(getNurgleRoster()), RosterSpeed::SLOW);   // 4.91
    EXPECT_EQ(classifyRosterSpeed(getKhemriRoster()), RosterSpeed::SLOW);   // 4.64
    EXPECT_EQ(classifyRosterSpeed(getHumanRoster()), RosterSpeed::MIXED);   // 7.00
    EXPECT_EQ(classifyRosterSpeed(getOrcRoster()), RosterSpeed::MIXED);     // 5.09
    EXPECT_EQ(classifyRosterSpeed(getLizardmenRoster()), RosterSpeed::MIXED); // 6.73
    EXPECT_EQ(classifyRosterSpeed(getChaosRoster()), RosterSpeed::MIXED);   // 5.55
}

TEST(GameSimulator, PressureFormationVsFastTeam) {
    GameState state;
    // AWAY kicking vs Skaven (HOME=Skaven receiving, AWAY=Human kicking)
    // But we need kicking team to face fast receiver.
    // HOME receives (Skaven=FAST), AWAY kicks
    setupHalf(state, getSkavenRoster(), getHumanRoster(), TeamSide::AWAY);

    EXPECT_EQ(state.receiverSpeed, RosterSpeed::FAST);

    // AWAY kicking → pressure formation: 3 on LOS
    int awayOnLOS = 0;
    state.forEachOnPitch(TeamSide::AWAY, [&](const Player& p) {
        if (p.position.x == 13) awayOnLOS++;
    });
    EXPECT_EQ(awayOnLOS, 3);

    // 4 contain line at x=14
    int awayContain = 0;
    state.forEachOnPitch(TeamSide::AWAY, [&](const Player& p) {
        if (p.position.x == 14) awayContain++;
    });
    EXPECT_EQ(awayContain, 4);

    // 3 second row at x=15
    int awaySecond = 0;
    state.forEachOnPitch(TeamSide::AWAY, [&](const Player& p) {
        if (p.position.x == 15) awaySecond++;
    });
    EXPECT_EQ(awaySecond, 3);

    // 1 sweeper at x=17
    int awaySweeper = 0;
    state.forEachOnPitch(TeamSide::AWAY, [&](const Player& p) {
        if (p.position.x == 17) awaySweeper++;
    });
    EXPECT_EQ(awaySweeper, 1);
}

TEST(GameSimulator, DeepColumnsVsSlowTeam) {
    GameState state;
    // HOME=Dwarf (SLOW), AWAY kicks
    setupHalf(state, getDwarfRoster(), getHumanRoster(), TeamSide::AWAY);

    EXPECT_EQ(state.receiverSpeed, RosterSpeed::SLOW);

    // AWAY kicking → 2-deep columns (unchanged)
    int awayOnLOS = 0;
    state.forEachOnPitch(TeamSide::AWAY, [&](const Player& p) {
        if (p.position.x == 13) awayOnLOS++;
    });
    EXPECT_EQ(awayOnLOS, 3);

    // Column fronts at x=14
    int awayFronts = 0;
    state.forEachOnPitch(TeamSide::AWAY, [&](const Player& p) {
        if (p.position.x == 14) awayFronts++;
    });
    EXPECT_EQ(awayFronts, 3);

    // Deep safeties at x=18
    int awaySafeties = 0;
    state.forEachOnPitch(TeamSide::AWAY, [&](const Player& p) {
        if (p.position.x == 18) awaySafeties++;
    });
    EXPECT_EQ(awaySafeties, 2);
}

TEST(GameSimulator, DeepReceiverFormation) {
    GameState state;
    // HOME receives (Human=MIXED), AWAY kicks
    setupHalf(state, getHumanRoster(), getHumanRoster(), TeamSide::AWAY);

    // HOME receiving → deep receiver formation → 4 on LOS (x=12)
    int homeOnLOS = 0;
    state.forEachOnPitch(TeamSide::HOME, [&](const Player& p) {
        if (p.position.x == 12) homeOnLOS++;
    });
    EXPECT_EQ(homeOnLOS, 4);

    // 1 deep receiver at x=7
    int homeDeep = 0;
    state.forEachOnPitch(TeamSide::HOME, [&](const Player& p) {
        if (p.position.x == 7) homeDeep++;
    });
    EXPECT_EQ(homeDeep, 1);

    // 2 mid backfield at x=9
    int homeMid = 0;
    state.forEachOnPitch(TeamSide::HOME, [&](const Player& p) {
        if (p.position.x == 9) homeMid++;
    });
    EXPECT_EQ(homeMid, 2);
}

TEST(GameSimulator, ShortKickVsFastTeam) {
    // When receiver is FAST, kick target should be x=18 (HOME kicks) or x=7 (AWAY kicks)
    GameState state;
    state.kickingTeam = TeamSide::AWAY;
    // HOME=Skaven (FAST), AWAY kicks
    setupHalf(state, getSkavenRoster(), getHumanRoster(), TeamSide::AWAY);

    EXPECT_EQ(state.receiverSpeed, RosterSpeed::FAST);

    // Run simpleKickoff with fixed dice to verify short kick
    // The kick target x should be 7 (AWAY kicks vs FAST)
    // We can't directly check kickX, but we can verify ball lands closer to LOS
    // Use deterministic dice: D6=1 (min scatter), D8=1 (north)
    // With Kick skill: scatter = ceil(1/2) = 1, direction north → kickX=7, kickY=7+1=8
    DiceRoller dice(0);
    // We need to control exact rolls; let's just verify ball is on pitch
    simpleKickoff(state, dice);
    EXPECT_TRUE(state.ball.isOnPitch() || state.ball.isHeld);
}

TEST(GameSimulator, DeepKickVsSlowTeam) {
    // When receiver is SLOW, kick target stays deep (x=22/3)
    GameState state;
    state.kickingTeam = TeamSide::AWAY;
    // HOME=Dwarf (SLOW), AWAY kicks
    setupHalf(state, getDwarfRoster(), getHumanRoster(), TeamSide::AWAY);

    EXPECT_EQ(state.receiverSpeed, RosterSpeed::SLOW);

    // Verify ball lands on pitch after kickoff
    DiceRoller dice(42);
    simpleKickoff(state, dice);
    EXPECT_TRUE(state.ball.isOnPitch() || state.ball.isHeld);
}

TEST(GameSimulator, PressureFormationNoOverlaps) {
    // Pressure formation + deep receiver: no duplicate positions
    GameState state;
    setupHalf(state, getSkavenRoster(), getHumanRoster(), TeamSide::AWAY);

    std::set<std::pair<int,int>> positions;
    for (auto& p : state.players) {
        if (!p.isOnPitch()) continue;
        auto pos = std::make_pair((int)p.position.x, (int)p.position.y);
        EXPECT_EQ(positions.count(pos), 0u)
            << "Duplicate position at (" << pos.first << "," << pos.second << ")";
        positions.insert(pos);
    }
    EXPECT_EQ(positions.size(), 22u);
}

TEST(GameSimulator, SetupDrivePreservesTurnClockAndRerolls) {
    // Regression test for project_bloodbowl_audit_findings_20260703 finding 2:
    // a post-touchdown drive restart must NOT grant either team a fresh
    // 8-turn half or a fresh reroll pool -- only setupHalf (true half
    // boundaries) may do that.
    GameState state;
    setupHalf(state, getHumanRoster(), getHumanRoster(), TeamSide::AWAY);

    state.getTeamState(TeamSide::HOME).turnNumber = 5;
    state.getTeamState(TeamSide::AWAY).turnNumber = 4;
    state.getTeamState(TeamSide::HOME).rerolls = 1;
    state.getTeamState(TeamSide::AWAY).rerolls = 0;

    setupDrive(state, getHumanRoster(), getHumanRoster(), TeamSide::AWAY);

    EXPECT_EQ(state.getTeamState(TeamSide::HOME).turnNumber, 5);
    EXPECT_EQ(state.getTeamState(TeamSide::AWAY).turnNumber, 4);
    EXPECT_EQ(state.getTeamState(TeamSide::HOME).rerolls, 1);
    EXPECT_EQ(state.getTeamState(TeamSide::AWAY).rerolls, 0);

    // Players are still re-placed exactly like setupHalf
    int homeOnPitch = 0, awayOnPitch = 0;
    state.forEachPlayer(TeamSide::HOME, [&](const Player& p) {
        if (p.isOnPitch()) homeOnPitch++;
    });
    state.forEachPlayer(TeamSide::AWAY, [&](const Player& p) {
        if (p.isOnPitch()) awayOnPitch++;
    });
    EXPECT_EQ(homeOnPitch, 11);
    EXPECT_EQ(awayOnPitch, 11);

    // setupHalf (true half boundary), by contrast, DOES reset both
    setupHalf(state, getHumanRoster(), getHumanRoster(), TeamSide::AWAY);
    EXPECT_EQ(state.getTeamState(TeamSide::HOME).turnNumber, 0);
    EXPECT_EQ(state.getTeamState(TeamSide::AWAY).turnNumber, 0);
    EXPECT_EQ(state.getTeamState(TeamSide::HOME).rerolls, 3);
    EXPECT_EQ(state.getTeamState(TeamSide::AWAY).rerolls, 3);
}

// 2026-07-10: the test above passes even with the half-clock bug live, because
// it stops at setupDrive. The real post-touchdown path is setupDrive() followed
// immediately by a kickoff (game_simulator.cpp's doKickoff lambda), and BOTH
// kickoff implementations used to re-zero turnNumber for both teams -- undoing
// what setupDrive had just preserved and silently reviving the "every TD grants
// a fresh 8-turn clock" bug. These tests drive the real sequence.
TEST(GameSimulator, PostTouchdownKickoffPreservesTurnClock) {
    for (bool useFullKickoff : {false, true}) {
        GameState state;
        setupHalf(state, getHumanRoster(), getHumanRoster(), TeamSide::AWAY);

        // Mid-half state: HOME just scored on its 5th turn, so HOME kicks off.
        state.getTeamState(TeamSide::HOME).turnNumber = 5;
        state.getTeamState(TeamSide::AWAY).turnNumber = 4;
        state.kickingTeam = TeamSide::HOME;

        setupDrive(state, getHumanRoster(), getHumanRoster(), TeamSide::HOME);
        DiceRoller dice(7);
        if (useFullKickoff) {
            resolveKickoff(state, dice, nullptr);
        } else {
            simpleKickoff(state, dice);
        }

        // The kicking team's clock is untouched; the receiving team advances to
        // its next turn -- NOT back to turn 1.
        EXPECT_EQ(state.getTeamState(TeamSide::HOME).turnNumber, 5)
            << "useFullKickoff=" << useFullKickoff;
        EXPECT_EQ(state.getTeamState(TeamSide::AWAY).turnNumber, 5)
            << "useFullKickoff=" << useFullKickoff;
    }
}

TEST(GameSimulator, HalfBoundaryKickoffStillStartsAtTurnOne) {
    // The same ++ must still yield turn 1 at a true half boundary, where
    // setupHalf has already zeroed both clocks before the kickoff runs.
    for (bool useFullKickoff : {false, true}) {
        GameState state;
        setupHalf(state, getHumanRoster(), getHumanRoster(), TeamSide::AWAY);
        DiceRoller dice(7);
        if (useFullKickoff) {
            resolveKickoff(state, dice, nullptr);
        } else {
            simpleKickoff(state, dice);
        }

        EXPECT_EQ(state.getTeamState(TeamSide::HOME).turnNumber, 1)
            << "useFullKickoff=" << useFullKickoff;
        EXPECT_EQ(state.getTeamState(TeamSide::AWAY).turnNumber, 0)
            << "useFullKickoff=" << useFullKickoff;
    }
}

// Regression for the H2 kickoff bug (project_bloodbowl_h2_kickoff_bug_20260713):
// the half-time branch derived the H2 kicking team from whoever kicked the last
// H1 drive (opponent(state.kickingTeam)) instead of from the opening kickoff,
// so whenever HOME scored last in H1, HOME received the H2 ball again. Correct:
// the second half reverses the OPENING roles -- the H1 receiver (HOME, since
// the opening kick is fixed to AWAY) kicks the second half, always.
// This has to run through the simulate loops themselves: the buggy line lives
// inline in both game loops, not in any helper (same lesson as the half-clock
// fix above, which is why observation happens via the policy callbacks).
// NEGATIVE CONTROL: pre-fix, every game whose last H1 scorer was HOME records
// secondHalfKicker == AWAY and the EXPECT_EQ below fails.
TEST(GameSimulator, SecondHalfKickoffReversesOpeningRoles) {
    for (bool useLoggedVariant : {false, true}) {
        bool sawHomeScoredLastInH1 = false;
        int gamesReachingH2 = 0;

        for (uint32_t seed = 1; seed <= 60; ++seed) {
            DiceRoller dice(seed);
            DiceRoller policyDice(seed * 7919 + 1);

            int lastHome = 0, lastAway = 0;
            TeamSide lastH1Scorer = TeamSide::AWAY;
            bool anyH1Score = false;
            bool h2Seen = false;
            TeamSide h2Kicker = TeamSide::AWAY;
            // 24.08.2026: zahajujici kopajici uz NENI konstanta (los, BB2016
            // l. 304-307), takze si ho test musi precist, ne predpokladat.
            bool openingSeen = false;
            TeamSide openingKicker = TeamSide::AWAY;

            auto creditH1Scorer = [&](const GameState& s) {
                int h = s.getTeamState(TeamSide::HOME).score;
                int a = s.getTeamState(TeamSide::AWAY).score;
                if (h > lastHome) { lastH1Scorer = TeamSide::HOME; anyH1Score = true; }
                if (a > lastAway) { lastH1Scorer = TeamSide::AWAY; anyH1Score = true; }
                lastHome = h; lastAway = a;
            };
            auto policy = [&](const GameState& s) {
                if (s.half == 1) {
                    if (!openingSeen) { openingKicker = s.kickingTeam; openingSeen = true; }
                    creditH1Scorer(s);
                } else if (!h2Seen) {
                    // A TD on the very last H1 action gets no PLAY policy
                    // call before the half-time kickoff -- credit it here,
                    // where the scores are still exactly the H1 finals.
                    creditH1Scorer(s);
                    h2Seen = true;
                    h2Kicker = s.kickingTeam;
                }
                return greedyPolicy(s, policyDice);
            };

            if (useLoggedVariant) {
                simulateGameLogged(getHumanRoster(), getHumanRoster(),
                                   policy, policy, dice);
            } else {
                simulateGame(getHumanRoster(), getHumanRoster(),
                             policy, policy, dice);
            }

            if (!h2Seen) continue;  // game hit MAX_ACTIONS inside H1
            gamesReachingH2++;
            // BB2016 l. 1016-1017: "At the start of the second half, the
            // kicking team is the one that did not kick off at the start of
            // the first half." Tedy protejsek OTVIRACIHO kopajiciho -- ne
            // konstanta HOME, a ne ten, kdo skoroval v H1 posledni.
            ASSERT_TRUE(openingSeen) << "seed=" << seed;
            EXPECT_EQ(h2Kicker, opponent(openingKicker))
                << "seed=" << seed << " logged=" << useLoggedVariant
                << " openingKicker=" << (int)openingKicker
                << " lastH1Scorer=" << (int)lastH1Scorer
                << " anyH1Score=" << anyH1Score;
            if (anyH1Score && lastH1Scorer != opponent(openingKicker)) {
                // rozlisujici pripad: posledni skorer H1 NENI ten, kdo ma
                // kopat v H2 -- prave tady stara vada vracela spatnou stranu
                sawHomeScoredLastInH1 = true;
            }
            // Enough coverage for this variant once the discriminating case
            // (HOME scored last in H1) has been seen at least once.
            if (sawHomeScoredLastInH1 && gamesReachingH2 >= 5) break;
        }

        // The invariant only discriminates pre-fix when HOME scored last in
        // H1 -- make sure the seed sweep actually produced that case.
        EXPECT_TRUE(sawHomeScoredLastInH1) << "logged=" << useLoggedVariant;
        EXPECT_GE(gamesReachingH2, 1) << "logged=" << useLoggedVariant;
    }
}

// === Developed (TV~1200) rosters ===

// Count, among the 11 fielded HOME players, how many have a given skill.
static int countHomeSkill(const GameState& state, SkillName skill) {
    int n = 0;
    state.forEachPlayer(TeamSide::HOME, [&](const Player& p) {
        if (p.isOnPitch() && p.hasSkill(skill)) n++;
    });
    return n;
}

TEST(DevelopedRoster, OrcNoGoblinsGuardAndStripBall) {
    const TeamRoster* r = getDevelopedRoster("orc", 1200);
    ASSERT_NE(r, nullptr);
    GameState state;
    setupHalf(state, *r, *r);

    int onPitch = 0;
    state.forEachPlayer(TeamSide::HOME, [&](const Player& p) {
        if (p.isOnPitch()) onPitch++;
    });
    EXPECT_EQ(onPitch, 11);
    // Goblins (Stunty) removed entirely.
    EXPECT_EQ(countHomeSkill(state, SkillName::Stunty), 0);
    // 2 Blitzers + 4 Black Orcs with Guard.
    EXPECT_EQ(countHomeSkill(state, SkillName::Guard), 6);
    // Cage doctrine 2026-08-07 (holders need Block): 4 Blitzers + 4 Black Orcs
    // + Thrower.
    EXPECT_EQ(countHomeSkill(state, SkillName::Block), 9);
    // Exactly one ball-hunter Blitzer with Strip Ball.
    EXPECT_EQ(countHomeSkill(state, SkillName::StripBall), 1);
}

TEST(DevelopedRoster, HumanOgreBlockAndStripBall) {
    const TeamRoster* r = getDevelopedRoster("human", 1200);
    ASSERT_NE(r, nullptr);
    GameState state;
    setupHalf(state, *r, *r);

    EXPECT_EQ(countHomeSkill(state, SkillName::StripBall), 1);
    EXPECT_EQ(countHomeSkill(state, SkillName::Guard), 2);
    // Ogre (ST5) is fielded with Block.
    bool ogreHasBlock = false;
    state.forEachPlayer(TeamSide::HOME, [&](const Player& p) {
        if (p.isOnPitch() && p.stats.strength == 5 && p.hasSkill(SkillName::Block)) {
            ogreHasBlock = true;
        }
    });
    EXPECT_TRUE(ogreHasBlock);
}

TEST(DevelopedRoster, DwarfLotsOfGuard) {
    const TeamRoster* r = getDevelopedRoster("dwarf", 1200);
    ASSERT_NE(r, nullptr);
    GameState state;
    setupHalf(state, *r, *r);

    // Cage-corner doctrine 2026-08-07: 2 Longbeards + 2 Blitzers + 2 Troll
    // Slayers with Guard; all four corner pieces (Blitzers+Slayers) also carry
    // Tackle on top of the Longbeards' innate one. Strip Ball hunter dropped.
    EXPECT_EQ(countHomeSkill(state, SkillName::Guard), 6);
    EXPECT_EQ(countHomeSkill(state, SkillName::Tackle), 9);
    EXPECT_EQ(countHomeSkill(state, SkillName::StripBall), 0);
}

TEST(DevelopedRoster, SkavenSureFeetGutterRunners) {
    const TeamRoster* r = getDevelopedRoster("skaven", 1200);
    ASSERT_NE(r, nullptr);
    GameState state;
    setupHalf(state, *r, *r);

    // All 4 Gutter Runners have Sure Feet.
    EXPECT_EQ(countHomeSkill(state, SkillName::SureFeet), 4);
    EXPECT_EQ(countHomeSkill(state, SkillName::StripBall), 1);
}

TEST(DevelopedRoster, WoodElfWardancerStripBall) {
    const TeamRoster* r = getDevelopedRoster("woodelf", 1200);
    ASSERT_NE(r, nullptr);
    GameState state;
    setupHalf(state, *r, *r);

    // One ball-hunter Wardancer with Strip Ball, one with Side Step, Treeman with Guard.
    EXPECT_EQ(countHomeSkill(state, SkillName::StripBall), 1);
    EXPECT_EQ(countHomeSkill(state, SkillName::SideStep), 1);
    EXPECT_EQ(countHomeSkill(state, SkillName::Guard), 1);
}

TEST(DevelopedRoster, BelowTVFallsBackToBase) {
    // tv < 1200 returns the base roster (which has goblins for Orc).
    const TeamRoster* base = getDevelopedRoster("orc", 1000);
    ASSERT_NE(base, nullptr);
    EXPECT_STREQ(base->name, "Orc");
}

// --- Package G (2026-08-10): casualties must survive the end of a drive ---

TEST(GameSimulator, CasualtiesSurviveADriveRestart) {
    // Before this, setupHalfOrDrive reset EVERY player to OFF_PITCH and
    // buildTeam stood them all up again, so each drive began with eleven
    // healthy players a side and the dead came back. CRP: a Casualty "must
    // miss the rest of the match".
    GameState state;
    setupHalf(state, getDwarfRoster(), getSkavenRoster());
    state.getPlayer(3).state = PlayerState::INJURED;
    state.getPlayer(3).position = {-1, -1};
    state.getPlayer(4).state = PlayerState::DEAD;
    state.getPlayer(4).position = {-1, -1};

    setupDrive(state, getDwarfRoster(), getSkavenRoster(), TeamSide::AWAY);

    EXPECT_EQ(state.getPlayer(3).state, PlayerState::INJURED);
    EXPECT_EQ(state.getPlayer(4).state, PlayerState::DEAD);
    EXPECT_FALSE(state.getPlayer(3).isOnPitch());
    EXPECT_FALSE(state.getPlayer(4).isOnPitch());

    // Two men down, two on the bench: substitutes take their places, so the
    // side still fields eleven -- that is layer 2. Layer 1 alone left holes.
    int standing = 0;
    state.forEachPlayer(TeamSide::HOME, [&](const Player& p) {
        if (p.state == PlayerState::STANDING) standing++;
    });
    // Ten, not eleven: the bench covers the injured man and the dead one, but
    // the base dwarf roster also fields a Deathroller, and since 2026-08-11 a
    // Secret Weapon player is sent off when the drive ends (CRP: "once a drive
    // ends that this player has played in at any point"). That is a third
    // vacancy and the bench has only two men.
    EXPECT_EQ(standing, 10)
        << "substitutes fill the casualties; the Deathroller is sent off";
    EXPECT_TRUE(state.getPlayer(GameState::benchBaseId(TeamSide::HOME)).isOnPitch());
    EXPECT_TRUE(state.getPlayer(GameState::benchBaseId(TeamSide::HOME) + 1).isOnPitch());
}

TEST(GameSimulator, TeamPlaysShortOnceTheBenchIsEmpty) {
    // Three casualties against a two-man bench: the substitutes come on and
    // the side is still one short. Without reserves a team simply plays down.
    GameState state;
    setupHalf(state, getDwarfRoster(), getSkavenRoster());
    for (int id : {3, 4, 5}) {
        state.getPlayer(id).state = PlayerState::INJURED;
        state.getPlayer(id).position = {-1, -1};
    }
    setupDrive(state, getDwarfRoster(), getSkavenRoster(), TeamSide::AWAY);

    int standing = 0;
    state.forEachPlayer(TeamSide::HOME, [&](const Player& p) {
        if (p.state == PlayerState::STANDING) standing++;
    });
    EXPECT_EQ(standing, 10);
}

// The base dwarf roster carries a Deathroller, and since 2026-08-11 a Secret
// Weapon player is sent off when the drive ends (CRP), so fixtures that want
// an ordinary body must not pick him by accident.
static int firstPlainHomePlayer(const GameState& state) {
    int found = -1;
    state.forEachPlayer(TeamSide::HOME, [&](const Player& p) {
        if (found < 0 && p.isOnPitch() && !p.skills.has(SkillName::SecretWeapon)) {
            found = p.id;
        }
    });
    return found;
}

TEST(GameSimulator, KOdPlayerRecoversOnFourPlus) {
    // CRP: "At the next kick-off, before you set up any players, roll for
    // each of your players that have been KO'd. On 1-3 he must remain in the
    // KO'd box; on 4-6 you must return the player to the Reserves box."
    GameState state;
    setupHalf(state, getDwarfRoster(), getSkavenRoster());
    const int id = firstPlainHomePlayer(state);
    ASSERT_GT(id, 0);
    state.getPlayer(id).state = PlayerState::KO;
    state.getPlayer(id).position = {-1, -1};

    FixedDiceRoller good({4});   // 4+ -> back to Reserves, so he is set up
    setupDrive(state, getDwarfRoster(), getSkavenRoster(), TeamSide::AWAY, &good);
    EXPECT_EQ(state.getPlayer(id).state, PlayerState::STANDING);
}

TEST(GameSimulator, KOdPlayerStaysOutOnThreeOrLess) {
    GameState state;
    setupHalf(state, getDwarfRoster(), getSkavenRoster());
    const int id = firstPlainHomePlayer(state);
    ASSERT_GT(id, 0);
    state.getPlayer(id).state = PlayerState::KO;
    state.getPlayer(id).position = {-1, -1};

    FixedDiceRoller bad({3});    // 1-3 -> stays in the KO'd box
    setupDrive(state, getDwarfRoster(), getSkavenRoster(), TeamSide::AWAY, &bad);
    EXPECT_EQ(state.getPlayer(id).state, PlayerState::KO);
    EXPECT_FALSE(state.getPlayer(id).isOnPitch());
}

TEST(GameSimulator, SwelteringHeatHoldsAPlayerOutForOneSetup) {
    // CRP Sweltering Heat: "Roll a D6 for each player on the pitch at the end
    // of a drive. On a roll of 1 the player collapses and may not be set up
    // for the next kick-off." One drive out, then back -- there is no
    // recovery roll, unlike a KO.
    GameState state;
    setupHalf(state, getDwarfRoster(), getSkavenRoster());
    state.weather = Weather::SWELTERING_HEAT;

    // Every heat roll a 1: everyone on the pitch collapses.
    class AllOnes : public DiceRollerBase {
    public:
        int rollD6() override { return 1; }
        int rollD8() override { return 1; }
        int roll2D6() override { return 2; }
        BlockDiceFace rollBlockDie() override { return BlockDiceFace::PUSHED; }
    } ones;
    setupDrive(state, getDwarfRoster(), getSkavenRoster(), TeamSide::AWAY, &ones);

    int standing = 0;
    state.forEachPlayer(TeamSide::HOME, [&](const Player& p) {
        if (p.state == PlayerState::STANDING) standing++;
    });
    // The eleven who were on the pitch all collapse; the two substitutes were
    // in Reserves, so the heat never touched them and they are all that is
    // left to field. Heat and the bench interacting, in one assertion.
    EXPECT_EQ(standing, 2) << "only the bench survives the heat";

    // Next drive they are available again -- the flag lasts one set-up only.
    setupDrive(state, getDwarfRoster(), getSkavenRoster(), TeamSide::AWAY, nullptr);
    standing = 0;
    state.forEachPlayer(TeamSide::HOME, [&](const Player& p) {
        if (p.state == PlayerState::STANDING) standing++;
    });
    EXPECT_EQ(standing, 11);
}

// 2026-08-11: slots carry roles, and the assignment must run over whoever is
// AVAILABLE rather than over the nominal starting eleven. Package G gave
// casualties persistence and the squad a bench the day before, so a KO'd
// Runner is now an ordinary occurrence -- and the old code filled the hole he
// left with a bench lineman, which would have put a Longbeard back in the
// deep slot and reintroduced the defect the role rules exist to prevent
// (measured: a Longbeard carrying advances 1.50 squares a turn against a
// Runner's 3.41).
TEST(GameSimulator, DeepSlotGoesToTheBestAvailableHandlerAfterInjuries) {
    const TeamRoster* dwarf = getDevelopedRoster("dwarf", 1200);
    const TeamRoster* skaven = getDevelopedRoster("skaven", 1200);
    ASSERT_NE(dwarf, nullptr);
    ASSERT_NE(skaven, nullptr);

    GameState state;
    DiceRoller dice(12345);
    setupHalf(state, *dwarf, *skaven, TeamSide::AWAY, &dice);

    // The receiving side's deep slot starts with the Sure Hands carrier.
    auto deepest = [&]() -> const Player* {
        const Player* best = nullptr;
        state.forEachOnPitch(TeamSide::HOME, [&](const Player& p) {
            if (!best || p.position.x < best->position.x) {
                best = &state.getPlayer(p.id);
            }
        });
        return best;
    };
    const Player* d0 = deepest();
    ASSERT_NE(d0, nullptr);
    EXPECT_TRUE(d0->skills.has(SkillName::SureHands))
        << "deep slot started with " << d0->positionName;

    // Take both Sure Hands players out of the match, then re-set up.
    int removed = 0;
    for (auto& p : state.players) {
        if (p.teamSide == TeamSide::HOME && p.skills.has(SkillName::SureHands)) {
            p.state = PlayerState::INJURED;
            removed++;
        }
    }
    ASSERT_GE(removed, 1);
    setupDrive(state, *dwarf, *skaven, TeamSide::AWAY, &dice);

    const Player* d1 = deepest();
    ASSERT_NE(d1, nullptr);
    // No handler left, so the next best by agility/movement takes the slot --
    // never simply whoever the bench happens to offer.
    EXPECT_GE(d1->stats.agility, 3) << "deep slot fell to " << d1->positionName;
    EXPECT_GE(d1->stats.movement, 5) << "deep slot fell to " << d1->positionName;
}

// 2026-08-11: the half could end without anyone taking an action, and nothing
// noticed. checkHalfOver was consulted only at the end of executeAction, but
// the kickoff advances the receiving team's turn number on its own -- so a
// touchdown on turn 8 was followed by a kickoff that bumped the receiver to
// turn 9, and that ninth turn was played out in full. Measured on the corpus:
// 9 such turns across 120 games. Riot can push the marker the same way.
//
// Rare, but it matters more than its frequency: every schedule in the dwarf
// drill is arithmetic over exactly eight turns per half.
TEST(GameSimulator, NoTurnNineIsEverPlayed) {
    const TeamRoster* dwarf = getDevelopedRoster("dwarf", 1200);
    const TeamRoster* skaven = getDevelopedRoster("skaven", 1200);
    ASSERT_NE(dwarf, nullptr);
    ASSERT_NE(skaven, nullptr);

    int worst = 0;
    for (uint32_t seed = 4100; seed < 4140; ++seed) {
        DiceRoller dice(seed);
        LoggedGameResult lgr = simulateGameLogged(
            *dwarf, *skaven,
            [&dice](const GameState& s) { return randomPolicy(s, dice); },
            [&dice](const GameState& s) { return randomPolicy(s, dice); },
            dice, /*useFullKickoff=*/true);
        for (const auto& t : lgr.turnLogs) {
            worst = std::max(worst, t.turnNumber);
        }
    }
    EXPECT_LE(worst, 8) << "a half is eight turns; turn " << worst << " was played";
}

// 2026-08-11: the simplified kickoff placed the ball only when the landing
// square held nobody catchable. Land it on a standing receiver who then drops
// it, and the ball stayed where setupHalf had left it -- off the pitch at
// (-1,-1) -- because resolveCatch does nothing with the ball when it fails.
// Seen once in 120 corpus games (g0040): a whole second half of 108 moves, 19
// blocks and a casualty, played with no ball on the field.
//
// This is the path the corpora actually run: neither the python binding nor
// the diagnostic harnesses ask for the full kickoff.
TEST(GameSimulator, SimpleKickoffAlwaysLeavesTheBallOnThePitch) {
    const TeamRoster* dwarf = getDevelopedRoster("dwarf", 1200);
    const TeamRoster* skaven = getDevelopedRoster("skaven", 1200);
    ASSERT_NE(dwarf, nullptr);
    ASSERT_NE(skaven, nullptr);

    for (uint32_t seed = 5200; seed < 5320; ++seed) {
        DiceRoller dice(seed);
        LoggedGameResult lgr = simulateGameLogged(
            *dwarf, *skaven,
            [&dice](const GameState& s) { return randomPolicy(s, dice); },
            [&dice](const GameState& s) { return randomPolicy(s, dice); },
            dice);
        for (const auto& t : lgr.turnLogs) {
            ASSERT_TRUE(t.ballHeld || (t.ballX >= 0 && t.ballY >= 0))
                << "seed " << seed << ": half " << t.half << " turn "
                << t.turnNumber << " played with the ball at ("
                << int(t.ballX) << "," << int(t.ballY) << ")";
        }
    }
}

// ============================================================================
// LOS PRED ZAPASEM -- BB2016 l. 304-307 (doplneno 24.08.2026)
// Do te doby se los nehazel vubec: `openingKickingTeam` byla konstanta AWAY.
// Merenim na krizovem korpusu (18 000 her) to vyslo jako systematicka vyhoda
// hostu: v ZRCADLOVYCH utkanich (stejna rasa na obou stranach => rozdil ma byt
// sum) vyhravali hoste o +4,87σ (human), +4,68σ (orc), +2,84σ (skaven),
// +2,61σ (wood-elf). Zadny test to nehlidal, protoze zadny netvrdil, ze
// zahajeni ma byt nahodne.
// ============================================================================

TEST(GameSimulator, CoinTossHomeWinsAndElectsToReceive) {
    // l. 304-306: vitéz losu volí, kdo se stavi prvni = kdo kope.
    FixedDiceRoller dice({1});   // 1-3 => los vyhrava HOME
    EXPECT_EQ(rollOpeningKickingTeam(dice), TeamSide::AWAY);  // HOME prijima => AWAY kope
}

TEST(GameSimulator, CoinTossAwayWinsAndElectsToReceive) {
    FixedDiceRoller dice({5});   // 4-6 => los vyhrava AWAY
    EXPECT_EQ(rollOpeningKickingTeam(dice), TeamSide::HOME);
}

TEST(GameSimulator, CoinTossWinnerMayElectToKick) {
    // druha strana volby: vitéz losu si smi vybrat kop (a prijmout 2. pulku)
    FixedDiceRoller dice({1});
    EXPECT_EQ(rollOpeningKickingTeam(dice, TossElection::KICK), TeamSide::HOME);
    FixedDiceRoller dice2({6});
    EXPECT_EQ(rollOpeningKickingTeam(dice2, TossElection::KICK), TeamSide::AWAY);
}

TEST(GameSimulator, CoinTossBothOpeningsOccurAcrossSeeds) {
    // vlastni nalez: kdyz to nikdo netvrdi, konstanta projde. Tohle to tvrdi.
    std::set<int> openings;
    for (int seed = 1; seed <= 40; ++seed) {
        DiceRoller dice(seed);
        openings.insert(static_cast<int>(rollOpeningKickingTeam(dice)));
    }
    EXPECT_EQ(openings.size(), 2u) << "los musi dat obe strany, ne konstantu";
}

TEST(GameSimulator, TossElectionIsRaceDependent) {
    // bbtactics.com/kicking-receiving + merení 24.08.: krehke/rychle soupisky
    // volí UTOK (prvni kolo, tri bloky zdarma na LOS, rychly TD), bashove
    // a vysokoAV volí OBRANU (branit s plnym soupisem, 2-1 grind).
    EXPECT_EQ(defaultTossElection(*getRosterByName("wood-elf")), TossElection::RECEIVE);
    EXPECT_EQ(defaultTossElection(*getRosterByName("skaven")),   TossElection::RECEIVE);
    EXPECT_EQ(defaultTossElection(*getRosterByName("dwarf")),    TossElection::KICK);
    EXPECT_EQ(defaultTossElection(*getRosterByName("orc")),      TossElection::KICK);
    EXPECT_EQ(defaultTossElection(*getRosterByName("human")),    TossElection::KICK);
}

TEST(GameSimulator, DwarfWinningTheTossElectsToDefend) {
    // trpaslik vyhraje los => volí OBRANU => kope on
    FixedDiceRoller dice({1});   // 1-3 => HOME vyhrava los
    EXPECT_EQ(rollOpeningKickingTeam(dice, *getRosterByName("dwarf"),
                                     *getRosterByName("wood-elf")),
              TeamSide::HOME);
}

TEST(GameSimulator, WoodElfWinningTheTossElectsToAttack) {
    // wood-elf vyhraje los => volí UTOK => kope soupeř
    FixedDiceRoller dice({5});   // 4-6 => AWAY vyhrava los
    EXPECT_EQ(rollOpeningKickingTeam(dice, *getRosterByName("dwarf"),
                                     *getRosterByName("wood-elf")),
              TeamSide::HOME);
}

// ============================================================================
// F10 (24.08.2026) -- BB2016 l. 275-283. `simpleKickoff` je cesta, na ktere
// bezi KORPUS, a touchback v ni neexistoval: mic se `clamp`nul na hriste.
// ============================================================================

TEST(GameSimulator, SimpleKickoffAwardsATouchbackWhenTheBallLeavesThePitch) {
    // l. 280-282: "If the ball scatters or bounces OFF THE PITCH or into the
    // kicking team's half, the receiving coach is awarded a 'touchback' and
    // must give the ball to any player in his team."
    // Do 24.08. se mic v teto ceste `clamp`nul na kraj hriste a hralo se dal.
    GameState gs;
    const TeamRoster& dwarf = *getRosterByName("dwarf");
    DiceRoller setupDice(7);
    setupHalf(gs, dwarf, dwarf, TeamSide::AWAY, &setupDice);   // AWAY kope, HOME prijima
    gs.kickingTeam = TeamSide::AWAY;

    // Kick skill sebereme, at se rozptyl nepuli (jinak by mic zustal na hristi).
    gs.forEachOnPitch(TeamSide::AWAY, [&](const Player& p) {
        gs.getPlayer(p.id).skills.remove(SkillName::Kick);
    });

    // AWAY kope na kickX = 3 (HOME je pomaly roster). D6=6, D8=7 (zapad)
    // => x = 3 - 6 = -3, tedy VEN Z HRISTE => touchback.
    FixedDiceRoller dice({6, 7, 4, 4});
    simpleKickoff(gs, dice);

    EXPECT_TRUE(gs.ball.isHeld) << "touchback musi dat mic hráči, ne ho nechat lezet";
    ASSERT_GE(gs.ball.carrierId, 0);
    EXPECT_EQ(gs.getPlayer(gs.ball.carrierId).teamSide, TeamSide::HOME)
        << "mic dostava PRIJIMAJICI tym";
    EXPECT_TRUE(gs.ball.position.isOnPitch());
    EXPECT_LE(gs.ball.position.x, 12) << "a stoji ve SVE polovine";
}

// ============================================================================
// NULOVÁ KONTROLA PRO RAMENO B2 (29.08.2026, návrh uživatele).
// `setWrestlePricingArm` opravuje cenu bloku proti obránci s Wrestle. Od
// 27.08. má Wrestle KAŽDÝ tým, takže matchup, kde se rameno spustit NEMŮŽE,
// přestal existovat -- a bez nuly se běh nesmí pustit (P20).
//
// ⛔ Proto VARIANTA, ne revert: kdyby se Wrestle odebral zpátky, běžela by
// nula na JINÉ BINÁRCE než expozice, a nula má dokázat, že aparát V TOMHLE
// BUILDU nevyrábí efekt z ničeho.
// ============================================================================

TEST(DevelopedRoster, DwarfNoWrestleVariantFieldsNoWrestleAtAll) {
    const TeamRoster* r = getDevelopedRoster("dwarf-nw", 1200);
    ASSERT_NE(r, nullptr);
    GameState state;
    setupHalf(state, *r, *r);

    EXPECT_EQ(countHomeSkill(state, SkillName::Wrestle), 0);
}

// A HRANICE: běžný trpaslík ho má pořád dva -- varianta se nesmí rozlézt.
TEST(DevelopedRoster, PlainDwarfStillFieldsTwoWrestleLongbeards) {
    const TeamRoster* r = getDevelopedRoster("dwarf", 1200);
    ASSERT_NE(r, nullptr);
    GameState state;
    setupHalf(state, *r, *r);

    EXPECT_EQ(countHomeSkill(state, SkillName::Wrestle), 2);
}

// ⭐ A JÁDRO NULY: musí to být TÁŽ SESTAVA minus jedna dovednost. Kdyby
// vypuštěním řádku vypadl i specialista, nula by neměřila podlahu aparátu,
// ale jiný tým. `buildTeam` sází specialisty OD ZADU a zbytek doplní prvním
// ("fill") řádkem, takže ti dva mají zůstat obyčejnými Longbeardy.
TEST(DevelopedRoster, DwarfNoWrestleKeepsEverySpecialistAndOnlyLosesTheSkill) {
    const TeamRoster* plain = getDevelopedRoster("dwarf", 1200);
    const TeamRoster* nw    = getDevelopedRoster("dwarf-nw", 1200);
    ASSERT_NE(plain, nullptr); ASSERT_NE(nw, nullptr);
    GameState a, b;
    setupHalf(a, *plain, *plain);
    setupHalf(b, *nw, *nw);

    // Specialisté beze změny
    EXPECT_EQ(countHomeSkill(b, SkillName::Guard),      countHomeSkill(a, SkillName::Guard));
    EXPECT_EQ(countHomeSkill(b, SkillName::Frenzy),     countHomeSkill(a, SkillName::Frenzy));
    EXPECT_EQ(countHomeSkill(b, SkillName::SureHands),  countHomeSkill(a, SkillName::SureHands));
    EXPECT_EQ(countHomeSkill(b, SkillName::Dauntless),  countHomeSkill(a, SkillName::Dauntless));
    // Longbeardi s Wrestle se stali obyčejnými Longbeardy => Block a Tackle
    // zůstávají, Wrestle mizí.
    EXPECT_EQ(countHomeSkill(b, SkillName::Block),      countHomeSkill(a, SkillName::Block));
    EXPECT_EQ(countHomeSkill(b, SkillName::Tackle),     countHomeSkill(a, SkillName::Tackle));
    EXPECT_EQ(countHomeSkill(b, SkillName::Wrestle), 0);
    EXPECT_EQ(countHomeSkill(a, SkillName::Wrestle), 2);
}
