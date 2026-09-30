#include <gtest/gtest.h>
#include "bb/ttm_handler.h"
#include "bb/helpers.h"
#include "bb/ball_handler.h"
#include <vector>

using namespace bb;

static void placePlayer(GameState& gs, int id, Position pos, TeamSide side,
                         int ma = 6, int st = 3, int ag = 3, int av = 8) {
    Player& p = gs.getPlayer(id);
    p.state = PlayerState::STANDING;
    p.position = pos;
    p.stats = {static_cast<int8_t>(ma), static_cast<int8_t>(st),
               static_cast<int8_t>(ag), static_cast<int8_t>(av)};
    p.movementRemaining = ma;
}

// ============================================================================
// TA3 (24.08.2026) -- BB2016 l. 8600-8624 (Throw Team-Mate), l. 8417-8431
// (Right Stuff), l. 7784-7795 (Always Hungry). Puvodnich osm testu
// certifikovalo SEST rozporu naraz, jeden z nich doslova opacne, nez pravidlo
// zni, a jeden byl tautologie.
//
// ⭐ A neni to exotika: Throw Team-Mate nese ORKSKY TROLL (l. 9073-9075) a
// Right Stuff + Stunty orksky GOBLIN (l. 9069) -- obojí je standardni orkska
// pozice. Latentni je to jen proto, ze nase TV1200 sestava trolla a gobliny
// vynechava.
// ============================================================================

static GameState ttmSetup() {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    gs.activeTeam = TeamSide::HOME;
    gs.homeTeam.side = TeamSide::HOME;
    gs.awayTeam.side = TeamSide::AWAY;
    return gs;
}

TEST(TTMHandler, ThereIsNoAccurateThrowEveryThrowScattersThreeTimes) {
    // l. 8609-8612: "ACCURATE PASSES ARE TREATED INSTEAD AS INACCURATE PASSES
    // thus scattering the player THREE TIMES." Puvodni `AccurateLanding`
    // asertoval dopad PRESNE na cilove pole.
    auto gs = ttmSetup();
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME, 6, 5, 3, 9);
    gs.getPlayer(1).skills.add(SkillName::ThrowTeamMate);
    placePlayer(gs, 2, {11, 7}, TeamSide::HOME, 6, 2, 3, 7);
    gs.getPlayer(2).skills.add(SkillName::RightStuff);

    // cil (13,7). passTarget = 7-3 -1(QP) +1(TTM postih) = 4. Hod 5 => projde.
    // tri rozptyly D8=3 (vychod): (14,7) -> (15,7) -> (16,7).
    // dopad: AG3 => 4+, bez TZ; hod 5 => na nohou.
    FixedDiceRoller dice({5, 3, 3, 3, 5});
    auto result = resolveThrowTeamMate(gs, 1, 2, {13, 7}, dice, nullptr);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(gs.getPlayer(2).position, (Position{16, 7}));
    EXPECT_EQ(gs.getPlayer(2).state, PlayerState::STANDING);
}

TEST(TTMHandler, TheThrowerSubtractsOneFromTheRoll) {
    // l. 8606-8607: "the player must SUBTRACT 1 FROM THE D6 ROLL when he passes
    // the player." Ten postih v kodu nebyl vubec.
    auto gs = ttmSetup();
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME, 6, 5, 3, 9);
    gs.getPlayer(1).skills.add(SkillName::ThrowTeamMate);
    placePlayer(gs, 2, {11, 7}, TeamSide::HOME, 6, 2, 3, 7);
    gs.getPlayer(2).skills.add(SkillName::RightStuff);

    // passTarget = 7-3 -1(QP) +1 = 4. Hod 3 => NEPROJDE (bez postihu by 3 >= 3
    // proslo). Neni to fumble (fumble je jen prirozena 1), takze se scatteruje.
    FixedDiceRoller dice({3, 3, 3, 3, 5});
    auto result = resolveThrowTeamMate(gs, 1, 2, {13, 7}, dice, nullptr);
    EXPECT_EQ(gs.getPlayer(2).position, (Position{16, 7}));
}

TEST(TTMHandler, AFumbledTeamMateLandsInHisOwnOriginalSquare) {
    // l. 8613-8614: "A fumbled team-mate will land in THE SQUARE HE ORIGINALLY
    // OCCUPIED." Puvodni `Fumble` scatteroval z pozice HAZECE a dokonce
    // asertoval EXPECT_NE(pozice, puvodni) -- pravy opak pravidla.
    // l. 8607-8608: "fumbles are NOT automatically turnovers."
    auto gs = ttmSetup();
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME, 6, 5, 3, 9);
    gs.getPlayer(1).skills.add(SkillName::ThrowTeamMate);
    placePlayer(gs, 2, {11, 7}, TeamSide::HOME, 6, 2, 3, 7);
    gs.getPlayer(2).skills.add(SkillName::RightStuff);

    // hod 1 => fumble => dopad na (11,7). Dopadovy hod: AG3 => 4+, hod 5.
    FixedDiceRoller dice({1, 5});
    auto result = resolveThrowTeamMate(gs, 1, 2, {13, 7}, dice, nullptr);

    EXPECT_FALSE(result.turnover);
    EXPECT_EQ(gs.getPlayer(2).position, (Position{11, 7}));
}

TEST(TTMHandler, AFailedLandingIsNotATurnoverWithoutTheBall) {
    // l. 8430-8431: "A failed landing roll or landing in the crowd DOES NOT
    // CAUSE A TURNOVER, UNLESS HE WAS HOLDING THE BALL." Puvodni `FailedLanding`
    // a `OffPitchTurnover` asertovaly turnover bez mice.
    auto gs = ttmSetup();
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME, 6, 5, 3, 9);
    gs.getPlayer(1).skills.add(SkillName::ThrowTeamMate);
    placePlayer(gs, 2, {11, 7}, TeamSide::HOME, 6, 2, 3, 7);
    gs.getPlayer(2).skills.add(SkillName::RightStuff);

    // hod 5 projde; tri rozptyly na vychod => (16,7); dopad hod 2 < 4 => spadne;
    // zbroj 3+3 = 6 <= AV7 => neproraženo.
    FixedDiceRoller dice({5, 3, 3, 3, 2, 3, 3});
    auto result = resolveThrowTeamMate(gs, 1, 2, {13, 7}, dice, nullptr);

    EXPECT_FALSE(result.turnover);
    EXPECT_EQ(gs.getPlayer(2).state, PlayerState::PRONE);
}

TEST(TTMHandler, LandingOnAPlayerKnocksHimDownAndSkipsTheLandingRoll) {
    // l. 8617-8622: "If the final square he scatters into is occupied by another
    // player, treat the player landed on as KNOCKED DOWN and ROLL FOR ARMOUR
    // (even if already Prone or Stunned), and then the player being thrown will
    // SCATTER ONE MORE SQUARE."
    // l. 8421-8428: dopadovy hod se NEDELA, kdyz dopadl na hráče -- je rovnou
    // Placed Prone a hazi se mu na zbroj.
    auto gs = ttmSetup();
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME, 6, 5, 3, 9);
    gs.getPlayer(1).skills.add(SkillName::ThrowTeamMate);
    placePlayer(gs, 2, {11, 7}, TeamSide::HOME, 6, 2, 3, 7);
    gs.getPlayer(2).skills.add(SkillName::RightStuff);
    placePlayer(gs, 12, {16, 7}, TeamSide::AWAY, 6, 3, 3, 8);   // stoji v cili

    // hod 5; tri rozptyly na vychod => (16,7), kde stoji hráč 12.
    // hráč 12: KNOCKED DOWN + zbroj 2+2 = 4 <= AV8 => neproraženo.
    // hazeny scatteruje jeste jednou: D8=3 => (17,7), prazdne.
    // dopadovy hod se NEHAZI => Placed Prone + zbroj 3+3 = 6 <= AV7.
    FixedDiceRoller dice({5, 3, 3, 3, 2, 2, 3, 3, 3});
    auto result = resolveThrowTeamMate(gs, 1, 2, {13, 7}, dice, nullptr);

    EXPECT_EQ(gs.getPlayer(12).state, PlayerState::PRONE) << "na koho dopadl, jde k zemi";
    EXPECT_EQ(gs.getPlayer(2).position, (Position{17, 7})) << "a hazeny scatteruje o pole dal";
    EXPECT_EQ(gs.getPlayer(2).state, PlayerState::PRONE) << "dopad na hráče = Placed Prone";
    EXPECT_FALSE(result.turnover);
}

TEST(TTMHandler, AlwaysHungryNeedsTWOOnesToEatTheTeamMate) {
    // l. 7787-7790: "On a roll of 1 he attempts to eat ... ROLL THE D6 AGAIN,
    // A SECOND 1 means that he successfully scoffs the team-mate down, WHICH
    // KILLS the team-mate without opportunity for recovery."
    // Puvodni `AlwaysHungryEat` sezral spoluhrace po JEDNE jednicce a dal mu
    // stav INJURED.
    auto gs = ttmSetup();
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME, 4, 5, 1, 9);
    gs.getPlayer(1).skills.add(SkillName::ThrowTeamMate);
    gs.getPlayer(1).skills.add(SkillName::AlwaysHungry);
    placePlayer(gs, 2, {11, 7}, TeamSide::HOME, 6, 2, 3, 7);
    gs.getPlayer(2).skills.add(SkillName::RightStuff);

    FixedDiceRoller dice({1, 1});
    auto result = resolveThrowTeamMate(gs, 1, 2, {13, 7}, dice, nullptr);

    EXPECT_EQ(gs.getPlayer(2).state, PlayerState::DEAD) << "sezrani je smrt bez zachrany";
    EXPECT_FALSE(result.turnover);
}

TEST(TTMHandler, AlwaysHungrySecondRollTwoToSixIsJustAFumble) {
    // l. 7792-7795: "If the second roll is 2-6 the team-mate SQUIRMS FREE and
    // the Pass Action is automatically treated as A FUMBLED PASS."
    // Fumble => spoluhrac konci ve svem PUVODNIM poli, a neni to turnover.
    auto gs = ttmSetup();
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME, 4, 5, 1, 9);
    gs.getPlayer(1).skills.add(SkillName::ThrowTeamMate);
    gs.getPlayer(1).skills.add(SkillName::AlwaysHungry);
    placePlayer(gs, 2, {11, 7}, TeamSide::HOME, 6, 2, 3, 7);
    gs.getPlayer(2).skills.add(SkillName::RightStuff);

    // 1 => pokus o sezrani; 4 => vykroutil se; fumble => (11,7);
    // dopadovy hod AG3 => 4+, hod 5 => na nohou.
    FixedDiceRoller dice({1, 4, 5});
    auto result = resolveThrowTeamMate(gs, 1, 2, {13, 7}, dice, nullptr);

    EXPECT_NE(gs.getPlayer(2).state, PlayerState::DEAD);
    EXPECT_EQ(gs.getPlayer(2).position, (Position{11, 7}));
    EXPECT_FALSE(result.turnover);
}

TEST(TTMHandler, ThrownCarrierOffThePitchIsATurnover) {
    // druha strana l. 8430-8431: "...UNLESS HE WAS HOLDING THE BALL"
    auto gs = ttmSetup();
    placePlayer(gs, 1, {2, 7}, TeamSide::HOME, 6, 5, 3, 9);
    gs.getPlayer(1).skills.add(SkillName::ThrowTeamMate);
    placePlayer(gs, 2, {3, 7}, TeamSide::HOME, 6, 2, 3, 7);
    gs.getPlayer(2).skills.add(SkillName::RightStuff);
    gs.ball = BallState::carried({3, 7}, 2);

    // hod 5 projde; cil (1,7); tri rozptyly na zapad => (-2,7) = mimo hriste
    FixedDiceRoller dice(std::vector<int>(40, 3));   // dost kostek na davovy hod i vhazovani
    auto result = resolveThrowTeamMate(gs, 1, 2, {1, 7}, dice, nullptr);
    EXPECT_TRUE(result.turnover);
}

// P74 (30.09.2026), port PHP ced20b99. r. 382-384: turnover, kdyz "a player
// with the ball is thrown ... and fails to land successfully (INCLUDING
// BEING EATEN ...)". Bez mice turnover neni (test vyse to hlida).
TEST(TTMHandler, P74EatingTheBallCarrierIsATurnover) {
    auto gs = ttmSetup();
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME, 4, 5, 1, 9);
    gs.getPlayer(1).skills.add(SkillName::ThrowTeamMate);
    gs.getPlayer(1).skills.add(SkillName::AlwaysHungry);
    placePlayer(gs, 2, {11, 7}, TeamSide::HOME, 6, 2, 3, 7);
    gs.getPlayer(2).skills.add(SkillName::RightStuff);
    gs.ball = BallState::carried({11, 7}, 2);

    FixedDiceRoller dice({1, 1, 3, 3, 3, 3});               // sezran; mic odskoci
    auto result = resolveThrowTeamMate(gs, 1, 2, {13, 7}, dice, nullptr);

    ASSERT_EQ(gs.getPlayer(2).state, PlayerState::DEAD);
    EXPECT_TRUE(result.turnover) << "sezrany nosic mice neni turnover";
}

// ============================================================================
// P76 (30.09.2026) -- port PHP 01bdfd77, 1657a19e, ec3feb2c.
// ============================================================================

static void ttmPair(GameState& gs, Position thrower, Position projectile) {
    placePlayer(gs, 1, thrower, TeamSide::HOME, 4, 5, 3, 9);   // AG3
    gs.getPlayer(1).skills.add(SkillName::ThrowTeamMate);
    placePlayer(gs, 2, projectile, TeamSide::HOME, 6, 2, 3, 7);
    gs.getPlayer(2).skills.add(SkillName::RightStuff);
}

TEST(TTMHandler, P76FumbleCountsTheModifiedRoll) {
    // Fumble je "1 or less before or after modification" (r. 1742-1744).
    // Short 0, TTM -1 => hod 2 je 1 = fumble => pristane na PUVODNIM poli.
    auto gs = ttmSetup();
    ttmPair(gs, {10, 7}, {11, 7});
    FixedDiceRoller dice({2, 6, 6, 6, 6, 6, 6});
    resolveThrowTeamMate(gs, 1, 2, {15, 7}, dice, nullptr);
    EXPECT_EQ(gs.getPlayer(2).position, (Position{11, 7}))
        << "hod 2 s -1 se bral jako neprecny, ne jako fumble";
}

TEST(TTMHandler, P76ScatterStopsOnceThePlayerLeavesThePitch) {
    // "If the thrown player scatters off the pitch, he is beaten up by the
    // crowd" (r. 8615-8617) -- dalsi rozptyl ho uz nevrati.
    auto gs = ttmSetup();
    ttmPair(gs, {10, 1}, {11, 1});
    // presny · 1. rozptyl N = ven · (dalsi dva by ho vratily: S, S) · dav 3+3
    FixedDiceRoller dice({6, 1, 5, 5, 3, 3, 3, 3, 3, 3});
    resolveThrowTeamMate(gs, 1, 2, {15, 0}, dice, nullptr);
    EXPECT_FALSE(gs.getPlayer(2).isOnPitch()) << "po vyletu z hriste se vratil";
}

TEST(TTMHandler, P76ThrownCarrierInTheCrowdMeansAThrowInFromHisLastSquare) {
    // Mic "is immediately thrown back in ... using the last square the ball
    // crossed before going off" (r. 866-871). C++ ho odrazel z pole (-1,-1).
    auto gs = ttmSetup();
    ttmPair(gs, {10, 1}, {11, 1});
    gs.ball = BallState::carried({11, 1}, 2);
    // presny · N ven z (15,0) · vhazovani: smer 3 = S, vzdalenost 2+2 =>
    // (15,4) · odraz D8 5 = S => (15,5) · dav 3+3
    FixedDiceRoller dice({6, 1, 3, 2, 2, 5, 3, 3, 3, 3, 3});
    auto result = resolveThrowTeamMate(gs, 1, 2, {15, 0}, dice, nullptr);
    EXPECT_TRUE(result.turnover);
    EXPECT_FALSE(gs.ball.isHeld);
    EXPECT_EQ(gs.ball.position, (Position{15, 5}));
}

TEST(TTMHandler, P76AStunnedPlayerLandedOnStaysStunned) {
    // Stejny princip jako u bomby (P73): "knocked down ... (even if already
    // prone or stunned)" (r. 8617-8619) = hod na brneni, ne otoceni licem nahoru.
    auto gs = ttmSetup();
    ttmPair(gs, {10, 7}, {11, 7});
    placePlayer(gs, 12, {15, 6}, TeamSide::AWAY);
    gs.getPlayer(12).state = PlayerState::STUNNED;
    // presny · E, W, N => (15,6) obsazeno · brneni 3+3 · o pole dal S => (15,7) · pristani 6
    FixedDiceRoller dice({6, 3, 7, 1, 3, 3, 5, 6, 6, 6, 6, 6, 6, 6, 6, 6});
    resolveThrowTeamMate(gs, 1, 2, {15, 7}, dice, nullptr);
    EXPECT_EQ(gs.getPlayer(12).state, PlayerState::STUNNED);
}
