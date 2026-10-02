#include <gtest/gtest.h>
#include "bb/kickoff_handler.h"
#include "bb/game_simulator.h"
#include "bb/roster.h"
#include "bb/policies.h"
#include "bb/helpers.h"
#include "bb/action_resolver.h"
#include "bb/rules_engine.h"
#include "bb/turn_handler.h"
#include <stdexcept>

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
                                      homePolicy, awayPolicy, dice);

    EXPECT_GE(result.homeScore, 0);
    EXPECT_GE(result.awayScore, 0);
    EXPECT_GT(result.totalActions, 0);
}

TEST(KickoffHandler, FullKickoffWithDifferentRosters) {
    DiceRoller dice(77);
    auto homePolicy = [&dice](const GameState& s) { return randomPolicy(s, dice); };
    auto awayPolicy = [&dice](const GameState& s) { return randomPolicy(s, dice); };

    GameResult result = simulateGame(getOrcRoster(), getSkavenRoster(),
                                      homePolicy, awayPolicy, dice);

    EXPECT_GE(result.homeScore, 0);
    EXPECT_GE(result.awayScore, 0);
    EXPECT_GT(result.totalActions, 0);
}

TEST(KickoffHandler, BallScatterStaysOnPitch) {
    // Po výkopu je míč na hřišti nebo v ruce. Jediná výjimka je Blitz! (ř. 1334-1341): míč letí,
    // dokud kopající neodehrají bonusové kolo — po jeho konci musí dopadnout.
    int blitzes = 0;
    for (uint32_t seed = 0; seed < 200; seed++) {
        auto gs = makeKickoffState();
        DiceRoller dice(seed);
        resolveKickoff(gs, dice, nullptr);
        if (gs.kickoffBallInAir) {
            ++blitzes;
            EXPECT_FALSE(gs.ball.isOnPitch() || gs.ball.isHeld) << "Blitz!: míč ještě letí, seed " << seed;
            Action end;
            end.type = ActionType::END_TURN;
            executeAction(gs, end, dice, nullptr);
            EXPECT_FALSE(gs.kickoffBallInAir);
        }
        EXPECT_TRUE(gs.ball.isOnPitch() || gs.ball.isHeld)
            << "Ball off-pitch after kickoff with seed " << seed;
    }
    EXPECT_GT(blitzes, 0) << "200 výkopů bez jediného Blitz! — výjimka nebyla vyzkoušena";
}

TEST(KickoffHandler, MultipleKickoffsInGame) {
    // Simulate with full kickoff and verify multiple kickoffs happen (scoring = new kickoff)
    DiceRoller dice(42);
    auto policy = [&dice](const GameState& s) { return randomPolicy(s, dice); };

    GameResult result = simulateGame(getSkavenRoster(), getSkavenRoster(),
                                      policy, policy, dice);

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

// Sestava od 02.10. dává KOR throwerovi; testy si určují samy, kdo ho má.
GameState korFixture() {
    auto gs = makeKickoffState();
    for (auto& p : gs.players) p.skills.remove(SkillName::KickOffReturn);
    return gs;
}

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
    auto gs = korFixture();
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
    auto gs = korFixture();
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
    auto gs = korFixture();
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
    auto gs = korFixture();
    ASSERT_EQ(gs.getPlayerAtPosition({8, 2}), nullptr);
    ASSERT_EQ(gs.getPlayerAtPosition({8, 12}), nullptr);
    const int a = placeKor(gs, {8, 2});
    const int b = placeKor(gs, {8, 12});
    FixedDiceRoller dice(kickDice());
    resolveKickoff(gs, dice, nullptr);
    const int moved = (gs.getPlayer(a).position != Position{8, 2}) + (gs.getPlayer(b).position != Position{8, 12});
    EXPECT_EQ(moved, 1);
}

// ===========================================================================
// SJEDNOCENÍ VÝKOPU (02.10.2026, F13): jediná výkopová funkce `resolveKickoff`. Testy níž citují
// BB2016 a kostky jsou volené tak, aby stará mechanika (mrtvá `resolveKickoff` do 02.10.) dala jiný
// výsledek. Přehled: evidence/kickoff_sjednoceni_20261002.md.
//
// Společná fixtura: Human × Human, AWAY kope, HOME přijímá (MIXED ⇒ kop na (3,7)). HOME stojí
// LoS x=12 y5-8, x=11 y4/6/8/10, x=9 y5/9, hluboko (7,7). AWAY LoS x=13 y4/7/10, x=14 a 15 tytéž
// y, hluboko (18,5) a (18,9) (jeden z nich má Kick). Kostky výkopu: D6 vzdálenost, D8 směr
// (1=S, 3=V, 7=Z), 2D6 tabulka, pak kostky výsledku tabulky, pak dopad (odraz D8, chycení D6).
// 2D6 = 1+1 = Get the Ref — úplatky engine nemá, takže je to „neutrální“ výsledek.
// ===========================================================================
namespace {
GameState kickFixture(bool withKick = true) {
    auto gs = korFixture();                       // bez Kick-Off Return (žádný pohyb před tabulkou)
    if (!withKick)
        for (auto& p : gs.players) p.skills.remove(SkillName::Kick);
    return gs;
}

Player& playerAt(GameState& gs, Position at) {
    Player* p = gs.getPlayerAtPosition(at);
    if (!p) throw std::logic_error("fixtura: na zadaném poli nikdo nestojí");
    return *p;
}

Player& firstOnPitch(GameState& gs, TeamSide side) {
    for (auto& p : gs.players)
        if (p.teamSide == side && p.isOnPitch()) return p;
    throw std::logic_error("fixtura: tým nemá nikoho na hřišti");
}

// D6 1 se Kick = 0 polí ⇒ míč nad (3,7), prázdné pole. Pak tabulka t1+t2, pak `rest`.
std::vector<int> onThree(int t1, int t2, std::vector<int> rest) {
    std::vector<int> d{1, 1, t1, t2};
    d.insert(d.end(), rest.begin(), rest.end());
    return d;
}

Action endTurn() {
    Action a;
    a.type = ActionType::END_TURN;
    return a;
}
}  // namespace

// --- Dopad míče --------------------------------------------------------------------------------

// P123. ř. 280-283: „If the ball scatters or bounces off the pitch … the receiving coach is
// awarded a 'touchback' and must give the ball to any player in his team.“ Dřív `clamp` na okraj.
TEST(KickoffLanding, BallKickedOffThePitchIsATouchback) {
    auto gs = kickFixture(/*withKick=*/false);
    FixedDiceRoller dice({6, 7, 1, 1});            // 3 - 6 = -3 ⇒ ven; Get the Ref
    resolveKickoff(gs, dice, nullptr);
    ASSERT_TRUE(gs.ball.isHeld) << "touchback: míč dostane hráč, neleží na okraji";
    EXPECT_EQ(gs.getPlayer(gs.ball.carrierId).teamSide, TeamSide::HOME);
    EXPECT_LE(gs.ball.position.x, 12);
}

// ř. 277-278: „If the ball lands in an empty square it will bounce one more square.“
TEST(KickoffLanding, BallLandingInAnEmptySquareBouncesOnce) {
    auto gs = kickFixture();
    FixedDiceRoller dice(onThree(1, 1, {3}));      // (3,7) prázdné ⇒ odraz na východ
    resolveKickoff(gs, dice, nullptr);
    EXPECT_FALSE(gs.ball.isHeld);
    EXPECT_EQ(gs.ball.position, (Position{4, 7}));
}

// ř. 280-282: „…scatters or BOUNCES off the pitch or into the kicking team's half … touchback“.
TEST(KickoffLanding, BounceIntoTheKickingHalfIsATouchback) {
    auto gs = kickFixture(/*withKick=*/false);
    gs.receiverSpeed = RosterSpeed::FAST;          // krátký kop na (7,7)
    playerAt(gs, {12, 7}).position = {2, 2};       // uvolní pole dopadu na LoS
    FixedDiceRoller dice({5, 3, 1, 1, 3});         // (12,7) prázdné ⇒ odraz na (13,7) = kopající půlka
    resolveKickoff(gs, dice, nullptr);
    ASSERT_TRUE(gs.ball.isHeld) << "odraz do kopající poloviny je touchback";
    EXPECT_EQ(gs.getPlayer(gs.ball.carrierId).teamSide, TeamSide::HOME);
}

// ř. 281-283 „give the ball to any player in his team“ + No Hands ř. 8318-8320: „unable to …
// carry the ball“. Dřív touchback dostal nejbližší hráč k místu dopadu, i bez rukou.
TEST(KickoffLanding, TouchbackNeverGoesToAPlayerWithNoHands) {
    auto gs = kickFixture(/*withKick=*/false);
    gs.receiverSpeed = RosterSpeed::FAST;
    for (auto& p : gs.players)
        if (p.teamSide == TeamSide::HOME && p.isOnPitch() && p.position.x == 12)
            p.skills.add(SkillName::NoHands);
    FixedDiceRoller dice({6, 3, 1, 1});            // 7 + 6 = 13 ⇒ kopající půlka ⇒ touchback
    resolveKickoff(gs, dice, nullptr);
    ASSERT_TRUE(gs.ball.isHeld);
    EXPECT_FALSE(gs.getPlayer(gs.ball.carrierId).hasSkill(SkillName::NoHands));
}

// ř. 857-858: „Prone and Stunned players may never attempt to catch the ball.“ Míč na ležícím se
// odrazí; dřív na něm zůstal ležet.
TEST(KickoffLanding, BallLandingOnAProneReceiverBounces) {
    auto gs = kickFixture(/*withKick=*/false);
    playerAt(gs, {7, 7}).state = PlayerState::PRONE;
    FixedDiceRoller dice({4, 3, 1, 1, 3});         // 3 + 4 = (7,7) ⇒ odraz na (8,7)
    resolveKickoff(gs, dice, nullptr);
    EXPECT_FALSE(gs.ball.isHeld);
    EXPECT_EQ(gs.ball.position, (Position{8, 7}));
}

// ř. 278-279: „If the ball lands on a square occupied by a player, the player must try to catch the
// ball.“ — kterýkoli hráč, i kopajícího týmu (po Blitz!). Dřív míč zůstal ležet pod ním.
TEST(KickoffLanding, KickingPlayerUnderTheBallMustTryToCatchIt) {
    auto gs = kickFixture(/*withKick=*/false);
    Player& away = playerAt(gs, {13, 10});
    away.position = {5, 7};
    FixedDiceRoller dice({2, 3, 1, 1, 6});         // 3 + 2 = (5,7); chytá na 6
    resolveKickoff(gs, dice, nullptr);
    ASSERT_TRUE(gs.ball.isHeld);
    EXPECT_EQ(gs.ball.carrierId, away.id);
}

// ř. 1033-1035: „Play stops when both coaches have had eight turns each.“ Přijímající, kterému
// nezbývá kolo, už výkop nedostane (dřív se kopalo, házelo na tabulku a teprve pak skončil poločas).
TEST(KickoffHandler, NoKickoffWhenTheReceivingTeamHasNoTurnLeft) {
    auto gs = kickFixture();
    gs.homeTeam.turnNumber = 8;
    gs.awayTeam.turnNumber = 8;
    FixedDiceRoller dice({});
    EXPECT_NO_THROW(resolveKickoff(gs, dice, nullptr)) << "nekope se ⇒ žádná kostka";
    EXPECT_TRUE(checkHalfOver(gs));
}

// --- Výkopová tabulka (ř. 1265-1356) -----------------------------------------------------------

// Ž3. ř. 1284-1296: „If the receiving team has not yet taken a turn this half … both teams' turn
// markers are moved forward one space.“ Dřív se hnul jen přijímající.
TEST(KickoffTable, RiotBeforeTheFirstTurnMovesBothMarkersForward) {
    auto gs = kickFixture();
    FixedDiceRoller dice(onThree(1, 2, {3}));
    resolveKickoff(gs, dice, nullptr);
    EXPECT_EQ(gs.homeTeam.turnNumber, 2);          // přijímající: 1. kolo propadlo
    EXPECT_EQ(gs.awayTeam.turnNumber, 1);          // kopající taky
}

// ř. 1291-1296: „Otherwise roll a D6. On a 1-3, both teams' turn markers are moved forward one
// space. On a 4-6, both team's turn markers are moved back one space.“ Dřív bez D6, vždy zpět.
TEST(KickoffTable, RiotMidHalfRollsADieForBothMarkers) {
    auto gs = kickFixture();
    gs.homeTeam.turnNumber = 3;                    // přijímající odehrál 3 kola
    gs.awayTeam.turnNumber = 4;
    FixedDiceRoller dice(onThree(1, 2, {2, 3}));   // D6 = 2 ⇒ vpřed
    resolveKickoff(gs, dice, nullptr);
    EXPECT_EQ(gs.homeTeam.turnNumber, 5);          // výkop ho posune na 4. kolo, Riot na 5.
    EXPECT_EQ(gs.awayTeam.turnNumber, 5);
}

// ř. 1285-1288: „If the receiving team's turn marker is on turn 7 for the half, both teams move
// their turn marker back one space.“
TEST(KickoffTable, RiotOnTurnSevenMovesBothMarkersBack) {
    auto gs = kickFixture();
    gs.homeTeam.turnNumber = 7;
    gs.awayTeam.turnNumber = 8;
    FixedDiceRoller dice(onThree(1, 2, {3}));
    resolveKickoff(gs, dice, nullptr);
    EXPECT_EQ(gs.homeTeam.turnNumber, 7);          // výkop 8, Riot zpět na 7
    EXPECT_EQ(gs.awayTeam.turnNumber, 7);
}

// Ž4/P90. ř. 1309-1314: „Each coach rolls a D3 … If both teams have the same score, then both teams
// get a re-roll.“ D6 4 a 3 jsou na D3 obě 2 ⇒ remíza. Dřív D6 (4 > 3) a při remíze nikdo.
TEST(KickoffTable, CheeringFansRollsD3AndATieRewardsBothTeams) {
    auto gs = kickFixture();
    const int home = gs.homeTeam.rerolls, away = gs.awayTeam.rerolls;
    FixedDiceRoller dice(onThree(3, 3, {4, 3, 3}));
    resolveKickoff(gs, dice, nullptr);
    EXPECT_EQ(gs.homeTeam.rerolls, home + 1);
    EXPECT_EQ(gs.awayTeam.rerolls, away + 1);
}

// Ž4/P90. ř. 1321-1326: „Each coach rolls a D3 … In case of a tie both teams get an extra team
// re-roll.“
TEST(KickoffTable, BrilliantCoachingRollsD3AndATieRewardsBothTeams) {
    auto gs = kickFixture();
    const int home = gs.homeTeam.rerolls, away = gs.awayTeam.rerolls;
    FixedDiceRoller dice(onThree(4, 4, {4, 3, 3}));
    resolveKickoff(gs, dice, nullptr);
    EXPECT_EQ(gs.homeTeam.rerolls, home + 1);
    EXPECT_EQ(gs.awayTeam.rerolls, away + 1);
}

// P91. ř. 1316-1320: „If the new Weather roll was a 'Nice' result, then a gentle gust of wind makes
// the ball scatter one extra square in a random direction before landing.“ Kick: 6 ⇒ 3 pole na
// (6,7); počasí 3+4 = Nice; poryv na východ ⇒ (7,7), kde stojí hloubkový hráč a chytá.
TEST(KickoffTable, ChangingWeatherToNiceBlowsTheBallOneMoreSquare) {
    auto gs = kickFixture();
    const int deep = playerAt(gs, {7, 7}).id;
    FixedDiceRoller dice({6, 3, 3, 4, 3, 4, 3, 6});
    resolveKickoff(gs, dice, nullptr);
    EXPECT_EQ(gs.weather, Weather::NICE);
    ASSERT_TRUE(gs.ball.isHeld) << "poryv posunul míč na hráče, který ho chytil";
    EXPECT_EQ(gs.ball.carrierId, deep);
}

// Ž5. ř. 1302-1308: „Any one player on the receiving team who is NOT IN AN OPPOSING PLAYER'S TACKLE
// ZONE may be moved into the square where the ball will land.“ Nejbližší (7,7) je v zóně hosta.
TEST(KickoffTable, HighKickIgnoresPlayersInAnOpposingTacklezone) {
    auto gs = kickFixture();
    const int deep = playerAt(gs, {7, 7}).id;
    playerAt(gs, {13, 10}).position = {6, 6};      // host vedle hloubkového hráče
    FixedDiceRoller dice(onThree(2, 3, {6}));
    resolveKickoff(gs, dice, nullptr);
    EXPECT_EQ(gs.getPlayer(deep).position, (Position{7, 7})) << "hráč v soupeřově TZ se nehýbe";
    const Player* under = gs.getPlayerAtPosition({3, 7});
    ASSERT_NE(under, nullptr) << "pod míč se postaví jiný, způsobilý hráč";
    EXPECT_EQ(under->teamSide, TeamSide::HOME);
}

// Ž6. ř. 1327-1333: „All of the players on the receiving team are allowed to move one square. This
// is a free move and may be made into any adjacent empty square, ignoring tackle zones.“ Dřív se
// všichni šoupli ke středu vlastní LoS. Volba AI: krok k místu dopadu (hloubkový hráč je v TZ hosta
// — volný krok ji ignoruje).
TEST(KickoffTable, QuickSnapIsAFreeStepTowardsTheBall) {
    auto gs = kickFixture();
    const int deep = playerAt(gs, {7, 7}).id;
    playerAt(gs, {13, 10}).position = {8, 8};
    FixedDiceRoller dice(onThree(4, 5, {3}));
    resolveKickoff(gs, dice, nullptr);
    EXPECT_EQ(gs.getPlayer(deep).position.distanceTo({3, 7}), 3) << "o jedno pole blíž k míči";
}

// Ž1. ř. 1334-1341: „The kicking team receives a free 'bonus' turn: however, players that are in an
// enemy tackle zone at the beginning of this free turn may not perform an Action.“ Míč je ve vzduchu
// (pořadí ř. 1242-1248: dopad až po vyřešení tabulky). Dřív se kopající jen šoupli o pole k LoS.
TEST(KickoffTable, BlitzGivesTheKickingTeamAFreeTurnBeforeTheBallLands) {
    auto gs = kickFixture();
    const int marked = playerAt(gs, {13, 7}).id;   // v zóně domácí LoS
    const int free = playerAt(gs, {18, 5}).id;
    FixedDiceRoller dice(onThree(4, 6, {3}));
    resolveKickoff(gs, dice, nullptr);

    ASSERT_EQ(gs.activeTeam, TeamSide::AWAY) << "na tahu je kopající tým";
    EXPECT_FALSE(gs.ball.isHeld);
    EXPECT_FALSE(gs.ball.isOnPitch()) << "míč ještě letí";
    std::vector<Action> actions;
    getAvailableActions(gs, actions);
    bool markedActs = false, freeActs = false;
    for (const auto& a : actions) {
        markedActs |= a.playerId == marked;
        freeActs |= a.playerId == free;
    }
    EXPECT_FALSE(markedActs) << "hráč v soupeřově TZ nesmí v bonusovém kole jednat";
    EXPECT_TRUE(freeActs);

    executeAction(gs, endTurn(), dice, nullptr);
    EXPECT_EQ(gs.activeTeam, TeamSide::HOME);
    EXPECT_EQ(gs.ball.position, (Position{4, 7})) << "po bonusovém kole míč dopadne a odrazí se";
    EXPECT_EQ(gs.homeTeam.turnNumber, 1) << "bonusové kolo nikomu nebere kolo";
    EXPECT_EQ(gs.awayTeam.turnNumber, 0);
}

// Ž2. ř. 1342-1350: „Each coach rolls a D6 … The fans of the team that rolls higher are the ones that
// threw the rock. … Decide randomly which player in the other team was hit … and roll for the effects
// of the injury straight away.“ Domácí 5 > hosté 2 ⇒ kámen jen na hosty; zranění 6+6 = Casualty.
// Dřív stun jednoho hráče z KAŽDÉHO týmu.
TEST(KickoffTable, ThrowARockHitsOnlyTheOtherTeamAndRollsForInjury) {
    auto gs = kickFixture();
    const int victim = firstOnPitch(gs, TeamSide::AWAY).id;   // náhodný výběr 2D6 (1,1) ⇒ první
    FixedDiceRoller dice(onThree(5, 6, {5, 2, 1, 1, 6, 6, 1, 1, 3, 3, 3}));
    resolveKickoff(gs, dice, nullptr);
    gs.forEachOnPitch(TeamSide::HOME, [](const Player& p) {
        EXPECT_EQ(p.state, PlayerState::STANDING) << "domácí fanoušci házeli — domácí nezasaženi";
    });
    EXPECT_FALSE(gs.getPlayer(victim).isOnPitch()) << "zranění 12 = Casualty, ne jen stun";
}

// Ž7. ř. 1351-1356: „Both coaches roll a D6 for each opposing player ON THE PITCH … If a roll is 6 or
// more … the player is Stunned“. Dřív se házelo jen za stojící.
TEST(KickoffTable, PitchInvasionAlsoHitsPlayersOnTheGround) {
    auto gs = kickFixture();
    Player& down = playerAt(gs, {9, 5});
    down.state = PlayerState::PRONE;
    FixedDiceRoller dice(onThree(6, 6, std::vector<int>(40, 6)));
    resolveKickoff(gs, dice, nullptr);
    EXPECT_EQ(down.state, PlayerState::STUNNED);
}

// Ž7. ř. 1353-1355: „…the player is Stunned (players with the Ball & Chain skill are KO'd)“.
TEST(KickoffTable, PitchInvasionKnocksOutBallAndChain) {
    auto gs = kickFixture();
    Player& bc = playerAt(gs, {9, 9});
    bc.skills.add(SkillName::BallAndChain);
    FixedDiceRoller dice(onThree(6, 6, std::vector<int>(40, 6)));
    resolveKickoff(gs, dice, nullptr);
    EXPECT_EQ(bc.state, PlayerState::KO);
    EXPECT_FALSE(bc.isOnPitch());
}

// ř. 703-708: „All face-down players are turned face up at the end of their team's next turn …
// a player may not turn face up on the turn they are Stunned.“ Omráčený při výkopu (mimo kolo)
// vstane na konci PRVNÍHO kola svého týmu. Dřív přijímající ležel o kolo déle.
TEST(KickoffTable, ReceiverStunnedAtTheKickoffTurnsFaceUpAfterHisFirstTurn) {
    auto gs = kickFixture();
    const int id = playerAt(gs, {7, 7}).id;
    FixedDiceRoller dice(onThree(6, 6, std::vector<int>(40, 6)));
    resolveKickoff(gs, dice, nullptr);
    ASSERT_EQ(gs.getPlayer(id).state, PlayerState::STUNNED);
    ASSERT_EQ(gs.activeTeam, TeamSide::HOME);
    executeAction(gs, endTurn(), dice, nullptr);
    EXPECT_EQ(gs.getPlayer(id).state, PlayerState::PRONE);
}

// --- Kick-Off Return ve skutečné hře (P122 + P125) ---------------------------------------------
// ř. 8249-8256. `buildTeam` dává KOR jednomu hráči (P125); do 02.10. ho výkop ve hře nikdy nepoužil,
// protože hry běžely na `simpleKickoff`. Tady jde výkop přes `simulateGame` a pozoruje se první
// rozhodnutí přijímajícího týmu: hráč s KOR už nestojí tam, kam ho postavila sestava.
TEST(KickOffReturn, MovesInARealGame) {
    const TeamRoster* dwarf = getDevelopedRoster("dwarf", 1200);
    const TeamRoster* elf = getDevelopedRoster("wood-elf", 1200);
    ASSERT_NE(dwarf, nullptr);
    ASSERT_NE(elf, nullptr);

    int games = 0, moved = 0;
    for (uint32_t seed = 1; seed <= 20; ++seed) {
        DiceRoller dice(seed);
        bool seen = false;
        auto observe = [&](const GameState& s) {
            const TeamSide recv = opponent(s.kickingTeam);
            if (!seen && s.half == 1 && s.activeTeam == recv) {
                seen = true;
                GameState setup;                       // kde ho sestava postavila
                setupHalf(setup, *dwarf, *elf, s.kickingTeam);
                setup.forEachOnPitch(recv, [&](const Player& p) {
                    if (!p.hasSkill(SkillName::KickOffReturn)) return;
                    ++games;
                    if (s.getPlayer(p.id).position != p.position) ++moved;
                });
            }
            return greedyPolicy(s, dice);
        };
        simulateGame(*dwarf, *elf, observe, observe, dice);
    }
    ASSERT_EQ(games, 20) << "každá sestava má právě jednoho nositele KOR (P125)";
    EXPECT_GE(moved, 10) << "KOR se ve hře použil jen " << moved << "× z " << games;
}

// ===========================================================================
// REVIEW VÝKOPU (02.10.2026) — evidence/kickoff_review_opravy_20261002.md
// ===========================================================================

// Položka 1. Bonusové kolo Blitz! (ř. 1334-1341) je samostatné kolo kopajícího týmu. Po TD kope
// skórující tým a jeho `turnNumber` se výkopem nemění ⇒ hranice záznamu kol podle (tým, číslo kola)
// ho slila s kolem, ve kterém padl TD. Hledá se hra, kde po TD padl Blitz! (politika ho vidí jako
// tah s `kickoffBallInAir` a nenulovým skóre), a pro každé takové kolo musí v záznamu být vlastní
// řádek s `kickoffBallInAir`.
TEST(KickoffTurnLog, BlitzAfterATouchdownGetsItsOwnTurnLog) {
    struct Key { int half, home, away; };
    int checked = 0;
    for (uint32_t seed = 1; seed <= 300 && checked < 2; ++seed) {
        DiceRoller dice(seed);
        std::vector<Key> blitzTurns;
        auto observe = [&](const GameState& s) {
            if (s.kickoffBallInAir && s.homeTeam.score + s.awayTeam.score > 0) {
                const Key k{s.half, s.homeTeam.score, s.awayTeam.score};
                bool known = false;
                for (const auto& b : blitzTurns)
                    known |= b.half == k.half && b.home == k.home && b.away == k.away;
                if (!known) blitzTurns.push_back(k);
            }
            return greedyPolicy(s, dice);
        };
        const LoggedGameResult lg =
            simulateGameLogged(getHumanRoster(), getOrcRoster(), observe, observe, dice);
        for (const auto& b : blitzTurns) {
            bool logged = false;
            for (const auto& t : lg.turnLogs)
                logged |= t.kickoffBallInAir && t.half == b.half && t.homeScore == b.home &&
                          t.awayScore == b.away;
            EXPECT_TRUE(logged) << "seed " << seed << ": Blitz! po TD (" << b.home << ":" << b.away
                                << ", poločas " << b.half << ") nemá vlastní záznam kola";
            ++checked;
        }
    }
    ASSERT_GE(checked, 1) << "fixtura: v hledaných hrách nepadl Blitz! po TD";
}

// Položka 2. FAQ ř. 9315-9317: „any event that causes the ball to go out of bounds or over the line
// of scrimmage during a kick-off results in a touchback.“ Poryv „Nice“ (ř. 1318-1320) míč, který už
// je mimo hřiště, nevrací zpět. Bez Kick: (3,7) - 4 = (-1,7) ⇒ mimo; tabulka 3+4, počasí 3+4 = Nice;
// dřív poryv D8 3 vrátil míč na (0,7) a ten se odrazil (D8 3) na (1,7) — touchback zmizel.
TEST(KickoffTable, NiceGustDoesNotBringAKickOffThePitchBackIn) {
    auto gs = kickFixture(/*withKick=*/false);
    FixedDiceRoller dice({4, 7, 3, 4, 3, 4, 3, 3, 3});
    resolveKickoff(gs, dice, nullptr);
    ASSERT_EQ(gs.weather, Weather::NICE);
    ASSERT_TRUE(gs.ball.isHeld) << "míč mimo hřiště = touchback, poryv ho nevrací";
    EXPECT_EQ(gs.getPlayer(gs.ball.carrierId).teamSide, TeamSide::HOME);
}

// Táž věta FAQ, „over the line of scrimmage“: krátký kop (7,7) + 6 na východ = (13,7), kopající
// polovina; poryv na západ by ho vrátil na (12,7) k domácímu na LoS.
TEST(KickoffTable, NiceGustDoesNotBringAKickBackOverTheLineOfScrimmage) {
    auto gs = kickFixture(/*withKick=*/false);
    gs.receiverSpeed = RosterSpeed::FAST;
    const int los = playerAt(gs, {12, 7}).id;
    FixedDiceRoller dice({6, 3, 3, 4, 3, 4, 7, 6, 6});
    resolveKickoff(gs, dice, nullptr);
    ASSERT_EQ(gs.weather, Weather::NICE);
    ASSERT_TRUE(gs.ball.isHeld);
    EXPECT_NE(gs.ball.carrierId, los) << "touchback dostane nejhlubší hráč, ne ten na LoS pod poryvem";
}

// Položka 4. ř. 997-1004 „Scoring in the opponent's turn“: „If one of your players is holding the
// ball in the opposing team's End Zone at any point during your opponent's turn then your team
// scores a touchdown immediately, but MUST MOVE THEIR TURN MARKER one space along the Turn track“.
// Po Blitz! chytí kopající hráč v koncové zóně přijímajících míč, který teprve dopadá: skóruje
// kopající ⇒ značku posouvá ON. Přijímající své kolo nezačal (míč dopadl před ním), takže o něj
// nepřijde. Dřív: přijímajícímu kolo propadlo a kopající značku neposunul.
TEST(KickoffTable, KickingTeamScoringOnTheBlitzLandingMovesItsOwnTurnMarker) {
    auto gs = kickFixture(/*withKick=*/false);
    FixedDiceRoller dice({3, 7, 4, 6, 6});         // (3,7) - 3 = (0,7); Blitz!; chytá na 6
    resolveKickoff(gs, dice, nullptr);
    ASSERT_TRUE(gs.kickoffBallInAir);
    Player& runner = playerAt(gs, {18, 5});
    runner.position = {0, 7};                      // doběhl v bonusovém kole pod míč
    executeAction(gs, endTurn(), dice, nullptr);
    ASSERT_EQ(gs.phase, GamePhase::TOUCHDOWN);
    ASSERT_EQ(gs.awayTeam.score, 1);
    EXPECT_EQ(gs.awayTeam.turnNumber, 1) << "skórující (kopající) tým posouvá značku";
    EXPECT_EQ(gs.homeTeam.turnNumber, 0) << "přijímající své první kolo ještě neodehrál";
}

// Položka 5. Nová sestava (po TD i o poločase) začíná bez míče ve vzduchu: zbytek bonusového kola
// Blitz! (např. poločas skončil dřív, než míč dopadl) nesmí přežít do dalšího výkopu.
TEST(KickoffHandler, SetupClearsABallStillInTheAirFromBlitz) {
    for (bool newHalf : {false, true}) {
        auto gs = kickFixture();
        gs.kickoffBallInAir = true;
        gs.kickoffLanding = {5, 5};
        if (newHalf) setupHalf(gs, getHumanRoster(), getHumanRoster(), TeamSide::HOME);
        else setupDrive(gs, getHumanRoster(), getHumanRoster(), TeamSide::HOME);
        EXPECT_FALSE(gs.kickoffBallInAir) << (newHalf ? "setupHalf" : "setupDrive");
        EXPECT_EQ(gs.kickoffLanding, (Position{-1, -1})) << (newHalf ? "setupHalf" : "setupDrive");
    }
}

// Položka 8. ř. 703-708: „All face-down players are turned face up at the end of their team's next
// turn … a player may not turn face up on the turn they are Stunned.“ Pitch Invasion (všechny D6 = 6)
// omráčí oba týmy MIMO kolo: přijímající vstanou na konci svého prvního kola, kopající na konci
// svého prvního kola — ne dřív (konec kola přijímajících) a ne později.
TEST(KickoffTable, PitchInvasionStunnedPlayersOfBothTeamsTurnFaceUpAfterTheirOwnNextTurn) {
    auto gs = kickFixture();
    const int recv = playerAt(gs, {7, 7}).id;
    const int kick = playerAt(gs, {18, 5}).id;
    FixedDiceRoller dice(onThree(6, 6, std::vector<int>(40, 6)));
    resolveKickoff(gs, dice, nullptr);
    ASSERT_EQ(gs.getPlayer(recv).state, PlayerState::STUNNED);
    ASSERT_EQ(gs.getPlayer(kick).state, PlayerState::STUNNED);
    EXPECT_TRUE(gs.getPlayer(kick).stunnedThisTurn) << "omráčen teď — stejně jako po hodu na zranění";

    executeAction(gs, endTurn(), dice, nullptr);   // konec 1. kola přijímajících
    EXPECT_EQ(gs.getPlayer(recv).state, PlayerState::PRONE);
    EXPECT_EQ(gs.getPlayer(kick).state, PlayerState::STUNNED) << "kopající ještě své kolo neměl";

    executeAction(gs, endTurn(), dice, nullptr);   // konec 1. kola kopajících
    EXPECT_EQ(gs.getPlayer(kick).state, PlayerState::PRONE);
}

// Položka 9. Touchback, když přijímající nemá nikoho stojícího s rukama: míč jde na zem doprostřed
// jejich poloviny (6,7). Leží-li tam hráč, míč pod ním zůstat nesmí (ř. 857-858: ležící nechytá ⇒
// odraz). Dřív zůstal míč ležet pod ním.
TEST(KickoffLanding, TouchbackFallbackSquareOccupiedByAPronePlayerBounces) {
    auto gs = kickFixture(/*withKick=*/false);
    playerAt(gs, {7, 7}).position = {6, 7};
    for (auto& p : gs.players)
        if (p.teamSide == TeamSide::HOME && p.isOnPitch()) p.state = PlayerState::PRONE;
    FixedDiceRoller dice({6, 7, 1, 1, 1});         // (3,7) - 6 ⇒ mimo hřiště ⇒ touchback; odraz na sever
    resolveKickoff(gs, dice, nullptr);
    EXPECT_FALSE(gs.ball.isHeld);
    EXPECT_EQ(gs.getPlayerAtPosition(gs.ball.position), nullptr) << "míč nesmí ležet pod hráčem";
    EXPECT_EQ(gs.ball.position, (Position{6, 6}));
}

// ===========================================================================
// Pokrytí pravidel výkopu, která dosud žádný test nehlídal (review 02.10.2026). Každý test má
// pozitivní kontrolu — mutaci kódu, na které spadne (evidence/kickoff_review_opravy_20261002.md).
// ===========================================================================

// ř. 280-283: „If the ball scatters or BOUNCES off the pitch … touchback“. (3,7) - 3 = (0,7), prázdné
// ⇒ odraz na západ mimo hřiště ⇒ touchback (žádné vhazování z davu, žádná další kostka).
TEST(KickoffLanding, BounceOffThePitchIsATouchback) {
    auto gs = kickFixture(/*withKick=*/false);
    FixedDiceRoller dice({3, 7, 1, 1, 7});
    resolveKickoff(gs, dice, nullptr);
    ASSERT_TRUE(gs.ball.isHeld) << "odraz z hřiště = touchback";
    EXPECT_EQ(gs.getPlayer(gs.ball.carrierId).teamSide, TeamSide::HOME);
    EXPECT_EQ(dice.remaining(), 0u);
}

// FAQ ř. 9312-9314: „If a player fails to catch a ball from a kick-off, and the ball bounces over the
// line of scrimmage, is it a touchback? A. Yes“. Krátký kop (7,7) + 5 na východ = (12,7) na hráče
// na LoS; chytání 1 ⇒ odraz na východ (13,7) = kopající polovina.
TEST(KickoffLanding, FailedCatchBouncingOverTheLineOfScrimmageIsATouchback) {
    auto gs = kickFixture(/*withKick=*/false);
    gs.receiverSpeed = RosterSpeed::FAST;
    const int los = playerAt(gs, {12, 7}).id;
    FixedDiceRoller dice({5, 3, 1, 1, 1, 3});
    resolveKickoff(gs, dice, nullptr);
    ASSERT_TRUE(gs.ball.isHeld) << "odraz přes LoS po nechyceném míči = touchback";
    EXPECT_EQ(gs.getPlayer(gs.ball.carrierId).teamSide, TeamSide::HOME);
    EXPECT_NE(gs.ball.carrierId, los);
    EXPECT_EQ(dice.remaining(), 0u);
}

namespace {
// Blitz! s míčem nad (3,7) (Kick, D6 1 ⇒ 0 polí). Volný hostující hráč (18,5) se postaví vedle
// domácího (11,4) — jeho akce v bonusovém kole pak začíná uhýbáním z tacklezóny.
struct BlitzDodge {
    GameState gs;
    int runner = -1;
};
BlitzDodge blitzWithADodgeAhead(FixedDiceRoller& dice) {
    BlitzDodge b{kickFixture(), -1};
    resolveKickoff(b.gs, dice, nullptr);
    Player& r = playerAt(b.gs, {18, 5});
    r.skills.remove(SkillName::Dodge);
    r.skills.remove(SkillName::SureFeet);
    r.position = {12, 3};
    b.runner = r.id;
    return b;
}
Action moveTo(int id, Position to) { return Action{ActionType::MOVE, id, -1, to}; }
}  // namespace

// ř. 1339-1341: „If any player suffers a turnover then the bonus turn ends.“ + pořadí ř. 1242-1248:
// teprve pak míč dopadne. Uhýbání 1, bez přehozů ⇒ turnover; brnění 1+1; dopad (3,7) ⇒ odraz (4,7).
TEST(KickoffTable, TurnoverEndsTheBlitzBonusTurnAndTheBallLands) {
    FixedDiceRoller dice(onThree(4, 6, {1, 1, 1, 3}));
    auto b = blitzWithADodgeAhead(dice);
    b.gs.awayTeam.rerolls = 0;
    executeAction(b.gs, moveTo(b.runner, {13, 2}), dice, nullptr);
    EXPECT_EQ(b.gs.getPlayer(b.runner).state, PlayerState::PRONE);
    EXPECT_EQ(b.gs.activeTeam, TeamSide::HOME) << "turnover ukončil bonusové kolo";
    EXPECT_FALSE(b.gs.kickoffBallInAir);
    EXPECT_EQ(b.gs.ball.position, (Position{4, 7})) << "míč dopadl a odrazil se";
    EXPECT_EQ(b.gs.homeTeam.turnNumber, 1);
    EXPECT_EQ(dice.remaining(), 0u);
}

// ř. 1338-1339: „The kicking team may use team re-rolls during a Blitz.“ Uhýbání 1, přehoz 6.
TEST(KickoffTable, KickingTeamMayUseATeamRerollDuringTheBlitz) {
    FixedDiceRoller dice(onThree(4, 6, {1, 6}));
    auto b = blitzWithADodgeAhead(dice);
    b.gs.awayTeam.rerolls = 1;
    executeAction(b.gs, moveTo(b.runner, {13, 2}), dice, nullptr);
    EXPECT_EQ(b.gs.getPlayer(b.runner).position, (Position{13, 2}));
    EXPECT_EQ(b.gs.getPlayer(b.runner).state, PlayerState::STANDING);
    EXPECT_EQ(b.gs.awayTeam.rerolls, 0) << "týmový přehoz použit";
    EXPECT_EQ(b.gs.activeTeam, TeamSide::AWAY) << "bonusové kolo pokračuje";
    EXPECT_TRUE(b.gs.kickoffBallInAir);
}

// ř. 703-708: omráčený se otočí „at the end of their team's NEXT turn … may not turn face up on the
// turn they are Stunned“. Kopající omráčený v bonusovém kole (uhýbání 1, brnění 6+6, zranění 1+1)
// leží přes konec bonusového kola i kola přijímajících a otočí se na konci svého prvního řádného kola.
TEST(KickoffTable, KickingPlayerStunnedInTheBlitzTurnsFaceUpAfterHisNextNormalTurn) {
    FixedDiceRoller dice(onThree(4, 6, {1, 6, 6, 1, 1, 3}));
    auto b = blitzWithADodgeAhead(dice);
    b.gs.awayTeam.rerolls = 0;
    executeAction(b.gs, moveTo(b.runner, {13, 2}), dice, nullptr);
    ASSERT_EQ(b.gs.activeTeam, TeamSide::HOME);
    EXPECT_EQ(b.gs.getPlayer(b.runner).state, PlayerState::STUNNED) << "konec bonusového kola";
    executeAction(b.gs, endTurn(), dice, nullptr);
    EXPECT_EQ(b.gs.getPlayer(b.runner).state, PlayerState::STUNNED) << "konec kola přijímajících";
    executeAction(b.gs, endTurn(), dice, nullptr);
    EXPECT_EQ(b.gs.getPlayer(b.runner).state, PlayerState::PRONE) << "konec 1. řádného kola kopajících";
}

// ř. 1291-1296: „On a 4-6, both team's turn markers are moved back one space.“
TEST(KickoffTable, RiotMidHalfOnFourToSixMovesBothMarkersBack) {
    auto gs = kickFixture();
    gs.homeTeam.turnNumber = 3;
    gs.awayTeam.turnNumber = 4;
    FixedDiceRoller dice(onThree(1, 2, {5, 3}));   // D6 = 5 ⇒ zpět
    resolveKickoff(gs, dice, nullptr);
    EXPECT_EQ(gs.homeTeam.turnNumber, 3);          // výkop ho posune na 4. kolo, Riot zpět na 3.
    EXPECT_EQ(gs.awayTeam.turnNumber, 3);
}

// ř. 1345-1347: „The fans of the team that rolls higher are the ones that threw the rock. In the case
// of a tie a rock is thrown at each team!“ D6 3 : 3 ⇒ kámen na oba; výběr 1+1 ⇒ první hráč; zranění
// 1+1 = Stunned.
TEST(KickoffTable, ThrowARockTieHitsBothTeams) {
    auto gs = kickFixture();
    const int away = firstOnPitch(gs, TeamSide::AWAY).id;
    const int home = firstOnPitch(gs, TeamSide::HOME).id;
    FixedDiceRoller dice(onThree(5, 6, {3, 3, 1, 1, 1, 1, 1, 1, 1, 1, 3}));
    resolveKickoff(gs, dice, nullptr);
    EXPECT_EQ(gs.getPlayer(away).state, PlayerState::STUNNED);
    EXPECT_EQ(gs.getPlayer(home).state, PlayerState::STUNNED);
    EXPECT_EQ(dice.remaining(), 0u);
}

namespace {
std::vector<Position> homePositions(const GameState& gs) {
    std::vector<Position> v;
    gs.forEachOnPitch(TeamSide::HOME, [&](const Player& p) { v.push_back(p.position); });
    return v;
}
}  // namespace

// ř. 1305-1308: High Kick jen „as long as the square is unoccupied“. (3,7) + 4 = (7,7), kde stojí
// hloubkový hráč ⇒ nikdo se nepřesouvá, chytá ten, kdo tam stojí.
TEST(KickoffTable, HighKickDoesNothingWhenTheLandingSquareIsOccupied) {
    auto gs = kickFixture(/*withKick=*/false);
    const int deep = playerAt(gs, {7, 7}).id;
    const auto before = homePositions(gs);
    FixedDiceRoller dice({4, 3, 2, 3, 6});
    resolveKickoff(gs, dice, nullptr);
    EXPECT_EQ(homePositions(gs), before);
    ASSERT_TRUE(gs.ball.isHeld);
    EXPECT_EQ(gs.ball.carrierId, deep);
}

// High Kick: hráč se staví „into the square where the ball will land“ — míč mířící mimo hřiště
// (touchback) žádné takové pole nemá ⇒ nikdo se nehýbe.
TEST(KickoffTable, HighKickDoesNothingWhenTheKickIsATouchback) {
    auto gs = kickFixture(/*withKick=*/false);
    const auto before = homePositions(gs);
    FixedDiceRoller dice({6, 7, 2, 3});
    resolveKickoff(gs, dice, nullptr);
    EXPECT_EQ(homePositions(gs), before);
    ASSERT_TRUE(gs.ball.isHeld) << "touchback";
}

// ř. 1033-1035: „Play stops when both coaches have had eight turns each.“ Na úrovni simulateGame:
// když TD padne, až přijímající nemá kolo, výkop se nekoná — během výkopu, kdy přijímající nemá
// kolo, se nesmí hodit žádná kostka. Pozorováno kostkou, která vidí stav hry (politika dostává
// referenci na skutečný stav). Pozitivní kontrola: alespoň jedna hra s TD v posledním kole.
TEST(KickoffHandler, NoKickoffWithoutATurnLeftInARealGame) {
    struct WatchDice : DiceRollerBase {
        DiceRoller inner;
        const GameState* st = nullptr;
        int illegal = 0;
        explicit WatchDice(uint32_t seed) : inner(seed) {}
        void check() {
            if (st && st->kickoffInProgress &&
                st->getTeamState(opponent(st->kickingTeam)).turnNumber > 8) ++illegal;
        }
        int rollD6() override { check(); return inner.rollD6(); }
        int rollD8() override { check(); return inner.rollD8(); }
    };
    int lastTurnTds = 0;
    for (uint32_t seed = 1; seed <= 200 && lastTurnTds < 1; ++seed) {
        WatchDice dice(seed);
        int lastHalf = 0, lastActiveTurn = 0, lastOppTurn = 0, lastScore = 0;
        auto lastTurnTd = [&](int scoreNow) {
            return lastActiveTurn == 8 && lastOppTurn == 8 && scoreNow > lastScore;
        };
        auto observe = [&](const GameState& s) {
            dice.st = &s;
            const int score = s.homeTeam.score + s.awayTeam.score;
            if (lastHalf == 1 && s.half == 2 && lastTurnTd(score)) ++lastTurnTds;
            lastHalf = s.half;
            lastActiveTurn = s.getTeamState(s.activeTeam).turnNumber;
            lastOppTurn = s.getTeamState(opponent(s.activeTeam)).turnNumber;
            lastScore = score;
            return greedyPolicy(s, dice);
        };
        const GameResult r = simulateGame(getHumanRoster(), getOrcRoster(), observe, observe, dice);
        if (lastHalf == 2 && lastTurnTd(r.homeScore + r.awayScore)) ++lastTurnTds;
        EXPECT_EQ(dice.illegal, 0) << "seed " << seed << ": výkop házel, ač přijímající nemá kolo";
    }
    ASSERT_GE(lastTurnTds, 1) << "fixtura: žádný TD v posledním kole poločasu";
}
