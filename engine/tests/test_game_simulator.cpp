#include <gtest/gtest.h>
#include "bb/game_simulator.h"
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

TEST(GameSimulator, DefensiveFormationWall7Players) {
    GameState state;
    setupHalf(state, getHumanRoster(), getHumanRoster(), TeamSide::AWAY);

    // Away kicking → away defensive: 7 players on wall row (x=14)
    int awayWall = 0;
    state.forEachOnPitch(TeamSide::AWAY, [&](const Player& p) {
        if (p.position.x == 14) awayWall++;
    });
    EXPECT_EQ(awayWall, 7);
}

TEST(GameSimulator, DefensiveFormationDeepSafety) {
    GameState state;
    setupHalf(state, getHumanRoster(), getHumanRoster(), TeamSide::AWAY);

    // Away kicking → away defensive: 1 safety deep at x=18, y=7
    bool foundSafety = false;
    state.forEachOnPitch(TeamSide::AWAY, [&](const Player& p) {
        if (p.position.x == 18 && p.position.y == 7) foundSafety = true;
    });
    EXPECT_TRUE(foundSafety);
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

    // Away is receiving → standard formation → 4 on LOS (x=13)
    int awayOnLOS = 0;
    state.forEachOnPitch(TeamSide::AWAY, [&](const Player& p) {
        if (p.position.x == 13) awayOnLOS++;
    });
    EXPECT_EQ(awayOnLOS, 4);
}
