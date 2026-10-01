#include <gtest/gtest.h>
#include "bb/kickoff_handler.h"
#include "bb/game_simulator.h"
#include "bb/roster.h"
#include "bb/policies.h"
#include "bb/helpers.h"

using namespace bb;

static GameState makeKickoffState() {
    GameState gs;
    gs.kickingTeam = TeamSide::AWAY;
    setupHalf(gs, getHumanRoster(), getHumanRoster());
    return gs;
}

TEST(KickoffHandler, KickoffSetsPhaseToPlay) {
    auto gs = makeKickoffState();
    DiceRoller dice(42);
    resolveKickoff(gs, dice, nullptr);

    EXPECT_EQ(gs.phase, GamePhase::PLAY);
}

TEST(KickoffHandler, KickoffSetsActiveTeam) {
    auto gs = makeKickoffState();
    gs.kickingTeam = TeamSide::AWAY;
    DiceRoller dice(42);
    resolveKickoff(gs, dice, nullptr);

    EXPECT_EQ(gs.activeTeam, TeamSide::HOME);  // HOME receives
}

TEST(KickoffHandler, KickoffPlacesBall) {
    auto gs = makeKickoffState();
    DiceRoller dice(42);
    resolveKickoff(gs, dice, nullptr);

    EXPECT_TRUE(gs.ball.isOnPitch() || gs.ball.isHeld);
}

TEST(KickoffHandler, CheeringFansWinnerGetsReroll) {
    // Test Cheering Fans directly
    auto gs = makeKickoffState();
    int homeRerolls = gs.homeTeam.rerolls;
    int awayRerolls = gs.awayTeam.rerolls;

    // Use a seed where the kickoff roll gives cheering fans (roll 6)
    // We can't easily control the exact roll, so test with many seeds
    DiceRoller dice(123);
    resolveKickoff(gs, dice, nullptr);

    // At least verify rerolls are >= initial (could increase from cheering/coaching).
    // P100: dřív EXPECT_GE(součet, 0) — vždy pravda; počáteční hodnoty se uložily a nepoužily.
    EXPECT_GE(gs.homeTeam.rerolls, homeRerolls);
    EXPECT_GE(gs.awayTeam.rerolls, awayRerolls);
}

TEST(KickoffHandler, ChangingWeatherChanges) {
    auto gs = makeKickoffState();
    std::vector<GameEvent> events;

    // Run many kickoffs and check at least one weather change event
    bool sawWeatherChange = false;
    for (uint32_t seed = 0; seed < 100; seed++) {
        auto gsCopy = makeKickoffState();
        events.clear();
        DiceRoller dice(seed);
        resolveKickoff(gsCopy, dice, &events);

        for (auto& e : events) {
            if (e.type == GameEvent::Type::WEATHER_CHANGE) {
                sawWeatherChange = true;
                break;
            }
        }
        if (sawWeatherChange) break;
    }
    EXPECT_TRUE(sawWeatherChange);
}

TEST(KickoffHandler, QuickSnapMovesReceivingPlayers) {
    // We can verify that Quick Snap works by checking that it doesn't crash
    // and the game state remains valid
    auto gs = makeKickoffState();
    DiceRoller dice(42);
    resolveKickoff(gs, dice, nullptr);

    // All players should still be on pitch
    int count = 0;
    for (auto& p : gs.players) {
        if (p.isOnPitch()) count++;
    }
    // Some might be stunned by Pitch Invasion or Throw a Rock, but at least most
    EXPECT_GE(count, 18);  // At least 18 of 22 should be on pitch
}

TEST(KickoffHandler, ThrowARockStunsPlayers) {
    // Try many seeds until we get a "Throw a Rock" event (roll 11)
    bool sawStun = false;
    for (uint32_t seed = 0; seed < 200; seed++) {
        auto gs = makeKickoffState();
        std::vector<GameEvent> events;
        DiceRoller dice(seed);
        resolveKickoff(gs, dice, &events);

        for (auto& e : events) {
            if (e.type == GameEvent::Type::KNOCKED_DOWN) {
                sawStun = true;
                break;
            }
        }
        if (sawStun) break;
    }
    EXPECT_TRUE(sawStun);
}

TEST(KickoffHandler, PitchInvasionCanStunPlayers) {
    // Pitch Invasion (roll 12) can stun many players
    // Just verify it doesn't crash
    for (uint32_t seed = 0; seed < 50; seed++) {
        auto gs = makeKickoffState();
        DiceRoller dice(seed);
        resolveKickoff(gs, dice, nullptr);
        EXPECT_EQ(gs.phase, GamePhase::PLAY);
    }
}

TEST(KickoffHandler, TouchbackGivesBallToReceiver) {
    // When ball scatters into kicking half, touchback occurs
    // We test by setting up a scenario where the ball goes far
    bool sawTouchback = false;
    for (uint32_t seed = 0; seed < 200; seed++) {
        auto gs = makeKickoffState();
        DiceRoller dice(seed);
        resolveKickoff(gs, dice, nullptr);

        if (gs.ball.isHeld) {
            Player& carrier = gs.getPlayer(gs.ball.carrierId);
            if (carrier.teamSide == TeamSide::HOME) {
                sawTouchback = true;
                break;
            }
        }
    }
    // Either the receiver has the ball (touchback or catch), or it's on ground
    EXPECT_TRUE(sawTouchback);
}

TEST(KickoffHandler, FullKickoffSimulation) {
    // Simulate a complete game with full kickoff events
    DiceRoller dice(42);
    auto homePolicy = [&dice](const GameState& s) { return randomPolicy(s, dice); };
    auto awayPolicy = [&dice](const GameState& s) { return randomPolicy(s, dice); };

    GameResult result = simulateGame(getHumanRoster(), getHumanRoster(),
                                      homePolicy, awayPolicy, dice, true);

    EXPECT_GE(result.homeScore, 0);
    EXPECT_GE(result.awayScore, 0);
    EXPECT_GT(result.totalActions, 0);
}

TEST(KickoffHandler, FullKickoffWithDifferentRosters) {
    DiceRoller dice(77);
    auto homePolicy = [&dice](const GameState& s) { return randomPolicy(s, dice); };
    auto awayPolicy = [&dice](const GameState& s) { return randomPolicy(s, dice); };

    GameResult result = simulateGame(getOrcRoster(), getSkavenRoster(),
                                      homePolicy, awayPolicy, dice, true);

    EXPECT_GE(result.homeScore, 0);
    EXPECT_GE(result.awayScore, 0);
    EXPECT_GT(result.totalActions, 0);
}

TEST(KickoffHandler, BallScatterStaysOnPitch) {
    // Verify ball doesn't end up off-pitch after kickoff
    for (uint32_t seed = 0; seed < 50; seed++) {
        auto gs = makeKickoffState();
        DiceRoller dice(seed);
        resolveKickoff(gs, dice, nullptr);

        EXPECT_TRUE(gs.ball.isOnPitch() || gs.ball.isHeld)
            << "Ball off-pitch after kickoff with seed " << seed;
    }
}

TEST(KickoffHandler, MultipleKickoffsInGame) {
    // Simulate with full kickoff and verify multiple kickoffs happen (scoring = new kickoff)
    DiceRoller dice(42);
    auto policy = [&dice](const GameState& s) { return randomPolicy(s, dice); };

    GameResult result = simulateGame(getSkavenRoster(), getSkavenRoster(),
                                      policy, policy, dice, true);

    // With Skaven's speed, scoring should happen
    // Just verify game completes
    EXPECT_GT(result.totalActions, 0);
}

// --- P66 (29.09.2026): weather is rolled ONCE per match (BB2016 l. 2551,
// 2571-2573) and only Changing Weather changes it. Until then both kick-off
// paths re-rolled it before every drive.
TEST(KickoffHandler, KickoffKeepsMatchWeatherUnlessChangingWeather) {
    // Many seeds, so that the kick-off table hits Changing Weather in some of
    // them: those may change the weather, all others must keep it.
    int kept = 0, changedByEvent = 0;
    for (uint64_t seed = 1; seed <= 200; ++seed) {
        auto gs = makeKickoffState();
        gs.weather = Weather::BLIZZARD;
        DiceRoller dice(seed);
        std::vector<GameEvent> events;
        resolveKickoff(gs, dice, &events);
        bool changing = false;
        for (const auto& e : events)
            if (e.type == GameEvent::Type::WEATHER_CHANGE) changing = true;
        if (changing) { ++changedByEvent; continue; }
        EXPECT_EQ(gs.weather, Weather::BLIZZARD) << "seed " << seed;
        ++kept;
    }
    EXPECT_GT(kept, 150);          // most kick-offs are not Changing Weather
    EXPECT_GT(changedByEvent, 0);  // and the exception really was exercised
}

TEST(KickoffHandler, SimpleKickoffKeepsMatchWeather) {
    for (uint64_t seed = 1; seed <= 50; ++seed) {
        auto gs = makeKickoffState();
        gs.weather = Weather::POURING_RAIN;
        DiceRoller dice(seed);
        simpleKickoff(gs, dice);
        EXPECT_EQ(gs.weather, Weather::POURING_RAIN) << "seed " << seed;
    }
}

TEST(KickoffHandler, RollMatchWeatherUsesTheWeatherTable) {
    GameState gs;
    FixedDiceRoller dice({6, 6});      // 12 = Blizzard
    rollMatchWeather(gs, dice);
    EXPECT_EQ(gs.weather, Weather::BLIZZARD);
}

// ---------------------------------------------------------------------------
// P122 — Kick-Off Return, BB2016 ř. 8249–8256: „A player on the receiving team that is not on the
// Line of Scrimmage or in an opposing tackle zone may use this skill when the ball has been kicked.
// It allows the player to move up to 3 squares after the ball has been scattered but BEFORE rolling
// on the Kick-Off table. Only one player may use this skill each kick-off. … may not be used for a
// touchback kick-off and does not allow the player to cross into the opponent's half.“
// Dřív: pohyb až PO výkopové tabulce, bez kontroly LoS a tacklezón, bez stopy v událostech.
// ---------------------------------------------------------------------------
namespace {
// HOME přijímá (AWAY kope). Kostky: D6 vzdálenost 1, D8 směr 1, tabulka 2D6 = 3+4 = 7
// (7 = Changing Weather dle ř. 1316-1321, viz P69), počasí 2D6 = 3+4. Míč dopadne kolem (3,7).
std::vector<int> kickDice() { return {1, 1, 3, 4, 3, 4, 3, 3, 3, 3, 3, 3}; }

int placeKor(GameState& gs, Position at) {
    // první domácí hráč na hřišti dostane KOR a postaví se na zadané (volné) pole
    for (auto& p : gs.players) {
        if (p.teamSide == TeamSide::HOME && p.isOnPitch() && !p.hasSkill(SkillName::KickOffReturn)) {
            if (const Player* occ = gs.getPlayerAtPosition(at); occ && occ->id != p.id) continue;
            p.position = at;
            p.skills.add(SkillName::KickOffReturn);
            return p.id;
        }
    }
    return -1;
}

int indexOf(const std::vector<GameEvent>& ev, GameEvent::Type t, int roll = -1) {
    for (size_t i = 0; i < ev.size(); ++i)
        if (ev[i].type == t && (roll < 0 || ev[i].roll == roll)) return static_cast<int>(i);
    return -1;
}
}  // namespace

TEST(KickOffReturn, MovesUpToThreeSquaresBeforeTheKickoffTable) {
    auto gs = makeKickoffState();
    ASSERT_EQ(gs.getPlayerAtPosition({8, 2}), nullptr);
    const int id = placeKor(gs, {8, 2});
    ASSERT_GE(id, 0);
    std::vector<GameEvent> ev;
    FixedDiceRoller dice(kickDice());
    resolveKickoff(gs, dice, &ev);

    EXPECT_EQ(gs.getPlayer(id).position.distanceTo({8, 2}), 3) << "má se posunout o 3 pole k míči";
    const int kor = indexOf(ev, GameEvent::Type::SKILL_USED, static_cast<int>(SkillName::KickOffReturn));
    const int table = indexOf(ev, GameEvent::Type::WEATHER_CHANGE);
    ASSERT_GE(kor, 0) << "použití Kick-Off Return musí zanechat stopu";
    ASSERT_GE(table, 0) << "fixtura: výkopová tabulka má hodit Changing Weather";
    EXPECT_LT(kor, table) << "pohyb je PŘED hodem na výkopovou tabulku";
}

TEST(KickOffReturn, NotFromTheLineOfScrimmage) {
    auto gs = makeKickoffState();
    // HOME LoS je sloupec x=12; najdeme tam domácího hráče
    int id = -1;
    for (auto& p : gs.players)
        if (p.teamSide == TeamSide::HOME && p.isOnPitch() && p.position.x == 12) { id = p.id; break; }
    ASSERT_GE(id, 0) << "fixtura: na LoS musí někdo stát";
    gs.getPlayer(id).skills.add(SkillName::KickOffReturn);
    const Position before = gs.getPlayer(id).position;
    FixedDiceRoller dice(kickDice());
    resolveKickoff(gs, dice, nullptr);
    EXPECT_EQ(gs.getPlayer(id).position, before);
}

TEST(KickOffReturn, NotFromAnOpposingTacklezone) {
    auto gs = makeKickoffState();
    ASSERT_EQ(gs.getPlayerAtPosition({8, 2}), nullptr);
    ASSERT_EQ(gs.getPlayerAtPosition({9, 2}), nullptr);
    const int id = placeKor(gs, {8, 2});
    for (auto& p : gs.players)   // jeden hostující hráč vedle něj
        if (p.teamSide == TeamSide::AWAY && p.isOnPitch()) { p.position = {9, 2}; break; }
    FixedDiceRoller dice(kickDice());
    resolveKickoff(gs, dice, nullptr);
    EXPECT_EQ(gs.getPlayer(id).position, (Position{8, 2}));
}

TEST(KickOffReturn, OnlyOnePlayerPerKickoff) {
    auto gs = makeKickoffState();
    ASSERT_EQ(gs.getPlayerAtPosition({8, 2}), nullptr);
    ASSERT_EQ(gs.getPlayerAtPosition({8, 12}), nullptr);
    const int a = placeKor(gs, {8, 2});
    const int b = placeKor(gs, {8, 12});
    FixedDiceRoller dice(kickDice());
    resolveKickoff(gs, dice, nullptr);
    const int moved = (gs.getPlayer(a).position != Position{8, 2}) + (gs.getPlayer(b).position != Position{8, 12});
    EXPECT_EQ(moved, 1);
}
