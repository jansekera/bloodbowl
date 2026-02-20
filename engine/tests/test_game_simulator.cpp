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
