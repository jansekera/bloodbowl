// P223 (20.10.2026): Diving Catch v C++ — zrcadlo PHP `tests/Engine/DivingCatchTest.php`.
//
// `rules_bb2016.txt` ř. 8063-8071: "The player may add 1 to any catch roll from an accurate pass
//   targeted to his square. In addition, the player can attempt to catch any pass, kick off or crowd
//   throw-in, but not bouncing ball, that would land in an EMPTY square IN ONE OF HIS TACKLE ZONES
//   as if it had landed in his own square WITHOUT LEAVING HIS CURRENT SQUARE. A failed catch will
//   bounce from the Diving Catch player's square. If there are two or more players attempting to
//   use this skill then they get in each other's way and neither can use it."
// ř. 7983-7986 (a obdobně 8186, 8401): hráč bez tackle zón "may not catch".
//
// Dřív engine uměl jen +1 k přesné přihrávce (helpers.cpp); chytání míče dopadajícího VEDLE hráče
// chybělo úplně. Kostky jsou volené tak, aby stará mechanika (odraz z pole dopadu) dala jiný výsledek.

#include <gtest/gtest.h>
#include "bb/pass_handler.h"
#include "bb/ball_handler.h"
#include "bb/game_simulator.h"
#include "bb/helpers.h"
#include "bb/roster.h"

using namespace bb;

namespace {

void placePlayer(GameState& gs, int id, Position pos, TeamSide side, int ag = 3) {
    Player& p = gs.getPlayer(id);
    p.id = id;
    p.teamSide = side;
    p.state = PlayerState::STANDING;
    p.position = pos;
    p.stats = {6, 3, static_cast<int8_t>(ag), 8};
    p.movementRemaining = 6;
}

GameState passSetup() {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    gs.activeTeam = TeamSide::HOME;
    gs.homeTeam.side = TeamSide::HOME;
    gs.awayTeam.side = TeamSide::AWAY;
    gs.homeTeam.rerolls = 0;
    gs.homeTeam.turnNumber = 1;
    return gs;
}

// Nepřesná přihrávka z (5,5) na (10,5) (Short, AG3 ⇒ 4+; hod 2 = nepřesná, ne fumble),
// rozptyl J, V, Z ⇒ míč dopadá na prázdné (10,6). Hráč 2 s Diving Catch stojí na (10,7).
GameState thrownToTenFive() {
    GameState gs = passSetup();
    placePlayer(gs, 1, {5, 5}, TeamSide::HOME);
    placePlayer(gs, 2, {10, 7}, TeamSide::HOME);
    gs.getPlayer(2).skills.add(SkillName::DivingCatch);
    gs.ball = BallState::carried({5, 5}, 1);
    return gs;
}

std::vector<int> inaccurateToTenSix(std::vector<int> tail) {
    std::vector<int> d{2, 5, 3, 7};
    d.insert(d.end(), tail.begin(), tail.end());
    return d;
}

int divingCatchEvents(const std::vector<GameEvent>& events) {
    int n = 0;
    for (const auto& e : events)
        if (e.type == GameEvent::Type::SKILL_USED && e.roll == static_cast<int>(SkillName::DivingCatch)) ++n;
    return n;
}

}  // namespace

TEST(DivingCatch, InaccuratePassIsCaughtWithoutLeavingHisSquare) {
    GameState gs = thrownToTenFive();
    FixedDiceRoller dice(inaccurateToTenSix({4}));     // chycení 4 (AG3, bez zón ⇒ 4+)
    std::vector<GameEvent> events;
    auto result = resolvePass(gs, 1, {10, 5}, dice, &events);

    EXPECT_EQ(divingCatchEvents(events), 1);
    ASSERT_TRUE(gs.ball.isHeld);
    EXPECT_EQ(gs.ball.carrierId, 2);
    EXPECT_EQ(gs.getPlayer(2).position, (Position{10, 7})) << "\"without leaving his current square\" (ř. 8068)";
    EXPECT_EQ(gs.ball.position, (Position{10, 7}));
    EXPECT_FALSE(result.turnover);
}

TEST(DivingCatch, FailedCatchBouncesFromTheDivingCatchPlayersSquare) {
    GameState gs = thrownToTenFive();
    // chycení 1 = neúspěch; odraz 5 = J z pole hráče (10,7) na (10,8). Stará mechanika: odraz z (10,6).
    FixedDiceRoller dice(inaccurateToTenSix({1, 5}));
    resolvePass(gs, 1, {10, 5}, dice, nullptr);

    EXPECT_FALSE(gs.ball.isHeld);
    EXPECT_EQ(gs.ball.position, (Position{10, 8})) << "\"A failed catch will bounce from the Diving Catch player's square\"";
    EXPECT_EQ(gs.getPlayer(2).position, (Position{10, 7}));
}

TEST(DivingCatch, NeedsHisTacklezones) {
    GameState gs = thrownToTenFive();
    gs.getPlayer(2).lostTacklezones = true;
    FixedDiceRoller dice(inaccurateToTenSix({4}));     // 4 = odraz z (10,6) na JV (11,7)
    std::vector<GameEvent> events;
    resolvePass(gs, 1, {10, 5}, dice, &events);

    EXPECT_EQ(divingCatchEvents(events), 0) << "bez zón \"may not catch\" (ř. 7984)";
    EXPECT_FALSE(gs.ball.isHeld);
    EXPECT_EQ(gs.ball.position, (Position{11, 7}));
}

TEST(DivingCatch, NeedsAnEmptyLandingSquare) {
    GameState gs = thrownToTenFive();
    placePlayer(gs, 13, {10, 6}, TeamSide::AWAY);
    gs.getPlayer(13).state = PlayerState::PRONE;      // ležící ⇒ pole není "empty square" (ř. 8067)
    FixedDiceRoller dice(inaccurateToTenSix({4}));
    std::vector<GameEvent> events;
    resolvePass(gs, 1, {10, 5}, dice, &events);

    EXPECT_EQ(divingCatchEvents(events), 0);
    EXPECT_FALSE(gs.ball.isHeld);
    EXPECT_EQ(gs.ball.position, (Position{11, 7}));
}

TEST(DivingCatch, TwoDivingCatchPlayersGetInEachOthersWay) {
    GameState gs = thrownToTenFive();
    placePlayer(gs, 13, {9, 7}, TeamSide::AWAY);       // i soupeř se počítá jako "attempting"
    gs.getPlayer(13).skills.add(SkillName::DivingCatch);
    FixedDiceRoller dice(inaccurateToTenSix({4}));
    std::vector<GameEvent> events;
    resolvePass(gs, 1, {10, 5}, dice, &events);

    EXPECT_EQ(divingCatchEvents(events), 0) << "\"neither can use it\" (ř. 8070-8071)";
    EXPECT_FALSE(gs.ball.isHeld);
    EXPECT_EQ(gs.ball.position, (Position{11, 7}));
}

TEST(DivingCatch, OpposingDivingCatchPlayerInterceptsTheLandingBall) {
    GameState gs = thrownToTenFive();
    gs.getPlayer(2).skills.remove(SkillName::DivingCatch);
    placePlayer(gs, 13, {9, 7}, TeamSide::AWAY);
    gs.getPlayer(13).skills.add(SkillName::DivingCatch);
    FixedDiceRoller dice(inaccurateToTenSix({5}));     // AG3 + zóna hráče 2 na (10,7) ⇒ 5+
    auto result = resolvePass(gs, 1, {10, 5}, dice, nullptr);

    ASSERT_TRUE(gs.ball.isHeld);
    EXPECT_EQ(gs.ball.carrierId, 13);
    EXPECT_TRUE(result.turnover);
}

// Přesná přihrávka (6) na prázdné (10,5); hráč s Diving Catch na (10,6). Chytá "as if it had landed in
// his own square" ⇒ "Catching an accurate pass +1" (ř. 891), ale ne +1 z Diving Catch — ten je jen
// "from an accurate pass TARGETED TO HIS SQUARE" (ř. 8063-8064). AG3 ⇒ 3+. Stará: odraz 3 = V.
TEST(DivingCatch, AccuratePassToAnEmptySquareGetsOnlyTheAccuratePassModifier) {
    GameState gs = passSetup();
    placePlayer(gs, 1, {5, 5}, TeamSide::HOME);
    placePlayer(gs, 2, {10, 6}, TeamSide::HOME);
    gs.getPlayer(2).skills.add(SkillName::DivingCatch);
    gs.ball = BallState::carried({5, 5}, 1);
    FixedDiceRoller dice({6, 3});
    std::vector<GameEvent> events;
    auto result = resolvePass(gs, 1, {10, 5}, dice, &events);

    EXPECT_EQ(divingCatchEvents(events), 1);
    ASSERT_TRUE(gs.ball.isHeld);
    EXPECT_EQ(gs.ball.carrierId, 2);
    EXPECT_EQ(gs.getPlayer(2).position, (Position{10, 6}));
    int catchTarget = 0;
    for (const auto& e : events)
        if (e.type == GameEvent::Type::CATCH) catchTarget = e.roll;
    EXPECT_EQ(catchTarget, 3) << "jen +1 za přesnou přihrávku, ne +2";
    EXPECT_FALSE(result.turnover);
}

// Hail Mary z (2,5) na (20,5): 2 = ne fumble, rozptyl J, V, Z ⇒ (20,6); Diving Catch na (20,7) chytá 4.
TEST(DivingCatch, HailMaryLandingNextToDivingCatchPlayer) {
    GameState gs = passSetup();
    placePlayer(gs, 1, {2, 5}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::HailMaryPass);
    placePlayer(gs, 2, {20, 7}, TeamSide::HOME);
    gs.getPlayer(2).skills.add(SkillName::DivingCatch);
    gs.ball = BallState::carried({2, 5}, 1);
    FixedDiceRoller dice({2, 5, 3, 7, 4});
    auto result = resolvePass(gs, 1, {20, 5}, dice, nullptr);

    ASSERT_TRUE(gs.ball.isHeld);
    EXPECT_EQ(gs.ball.carrierId, 2);
    EXPECT_EQ(gs.getPlayer(2).position, (Position{20, 7}));
    EXPECT_FALSE(result.turnover);
}

// Vhazování od (0,7): šablona 3 = V, 2D6 = 3+3 ⇒ prázdné (5,7); Diving Catch na (5,8) chytá 4.
// Stará mechanika: odraz D8 4 = JV na (6,8).
TEST(DivingCatch, CrowdThrowInLandingNextToDivingCatchPlayer) {
    GameState gs = passSetup();
    placePlayer(gs, 1, {5, 8}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::DivingCatch);
    gs.ball = BallState::onGround({0, 7});
    FixedDiceRoller dice({3, 3, 3, 4});
    std::vector<GameEvent> events;
    resolveThrowIn(gs, {0, 7}, {-1, 7}, dice, &events);

    EXPECT_EQ(divingCatchEvents(events), 1);
    ASSERT_TRUE(gs.ball.isHeld);
    EXPECT_EQ(gs.ball.carrierId, 1);
    EXPECT_EQ(gs.getPlayer(1).position, (Position{5, 8}));
}

// --- P223 review LOW (30.10.2026): chybějící testy, zrcadlo PHP `DivingCatchTest` -------------------

namespace {
// Vhazování od (0,7) na prázdné (5,7) vedle hráče 1 s Diving Catch na (5,8); chytá 1, pak 4.
GameState throwInDivedForWithFirstCatchFailed(TeamSide catcherSide) {
    GameState gs = passSetup();
    gs.homeTeam.rerolls = 1;
    gs.awayTeam.rerolls = 1;
    placePlayer(gs, 1, {5, 8}, catcherSide);
    gs.getPlayer(1).skills.add(SkillName::DivingCatch);
    gs.ball = BallState::onGround({0, 7});
    FixedDiceRoller dice({3, 3, 3, 1, 4});
    resolveThrowIn(gs, {0, 7}, {-1, 7}, dice, nullptr);
    return gs;
}
}  // namespace

// ř. 929-933: týmový přehoz na hod hráče vlastního týmu ve vlastním tahu (na tahu HOME).
TEST(DivingCatch, ThrowInDivingCatchMayUseATeamRerollInOwnTurn) {
    GameState gs = throwInDivedForWithFirstCatchFailed(TeamSide::HOME);
    ASSERT_TRUE(gs.ball.isHeld) << "přehoz 4 = chyceno";
    EXPECT_EQ(gs.ball.carrierId, 1);
    EXPECT_EQ(gs.homeTeam.rerolls, 0);
}

// Totéž, ale skáče hráč AWAY v tahu HOME ⇒ bez přehozu; 4 je odraz z (5,8) na JV (6,9).
TEST(DivingCatch, ThrowInDivingCatchGetsNoTeamRerollInOpponentsTurn) {
    GameState gs = throwInDivedForWithFirstCatchFailed(TeamSide::AWAY);
    EXPECT_FALSE(gs.ball.isHeld);
    EXPECT_EQ(gs.ball.position, (Position{6, 9}));
    EXPECT_EQ(gs.awayTeam.rerolls, 1);
}

// Přesná přihrávka (6) na prázdné (10,5); hráč SOUPEŘE s Diving Catch na (11,5) (za cílem — žádné
// zachycení). Smí kterýkoli tým (ř. 8065 "any pass"); +1 za přesnou (ř. 891), ne +2 (ř. 8063-8064)
// ⇒ AG3 3+: hod 3 chytí (turnover), hod 2 ne (odraz z (11,5) D8 3 = V na (12,5)).
TEST(DivingCatch, OpposingDivingCatchPlayerCatchesAnAccuratePassToAnEmptySquare) {
    auto build = [] {
        GameState gs = passSetup();
        placePlayer(gs, 1, {5, 5}, TeamSide::HOME);
        placePlayer(gs, 13, {11, 5}, TeamSide::AWAY);
        gs.getPlayer(13).skills.add(SkillName::DivingCatch);
        gs.ball = BallState::carried({5, 5}, 1);
        return gs;
    };
    GameState caught = build();
    FixedDiceRoller d1({6, 3});
    std::vector<GameEvent> events;
    auto r1 = resolvePass(caught, 1, {10, 5}, d1, &events);
    EXPECT_EQ(divingCatchEvents(events), 1);
    ASSERT_TRUE(caught.ball.isHeld);
    EXPECT_EQ(caught.ball.carrierId, 13);
    EXPECT_TRUE(r1.turnover);

    GameState missed = build();
    FixedDiceRoller d2({6, 2, 3});
    resolvePass(missed, 1, {10, 5}, d2, nullptr);
    EXPECT_FALSE(missed.ball.isHeld) << "hod 2 na 3+ nestačí — jen +1";
    EXPECT_EQ(missed.ball.position, (Position{12, 5}));
}

// Pro (ř. 8385-8389): neúspěšné chytání skokem (1), Pro 4 dovolí přehoz, přehoz 4 chytí (AG3 4+).
TEST(DivingCatch, DivingCatchMayUseProToRerollAFailedCatch) {
    GameState gs = thrownToTenFive();
    gs.getPlayer(2).skills.add(SkillName::Pro);
    FixedDiceRoller dice(inaccurateToTenSix({1, 4, 4}));
    auto result = resolvePass(gs, 1, {10, 5}, dice, nullptr);

    ASSERT_TRUE(gs.ball.isHeld);
    EXPECT_EQ(gs.ball.carrierId, 2);
    EXPECT_FALSE(result.turnover);
}

// Strážní test (prošel i před P223): "but not bouncing ball" (ř. 8066).
TEST(DivingCatch, BouncingBallIsNotDivingCaught) {
    GameState gs = passSetup();
    placePlayer(gs, 1, {5, 8}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::DivingCatch);
    FixedDiceRoller dice({5, 6});                      // odraz z (5,6) na J (5,7)
    std::vector<GameEvent> events;
    resolveBounce(gs, {5, 6}, dice, 0, &events);

    EXPECT_EQ(divingCatchEvents(events), 0);
    EXPECT_FALSE(gs.ball.isHeld);
    EXPECT_EQ(gs.ball.position, (Position{5, 7}));
}

// Pozitivní kontrola na skutečné soupisce: Wood Elf TV1500 (`getDevelopedRoster("woodelf", 1500)`),
// Catcher AG4 s Catch, Dodge, Sprint, Block, Diving Catch. Soupeř na (11,5) dává zónu na pole dopadu
// (10,6), ne na hráčovo pole (10,7): "as if it had landed in his own square" ⇒ 7-4 = 3+.
TEST(DivingCatch, WoodElfTv1500CatcherCatchesWithModifiersOfHisOwnSquare) {
    const TeamRoster* roster = getDevelopedRoster("woodelf", 1500);
    ASSERT_NE(roster, nullptr);
    GameState team;
    setupHalf(team, *roster, *roster);
    const Player* elf = nullptr;
    for (const auto& p : team.players)
        if (p.teamSide == TeamSide::HOME && p.hasSkill(SkillName::DivingCatch)) elf = &p;
    ASSERT_NE(elf, nullptr) << "soupiska TV1500 elfů má hráče s Diving Catch";

    GameState gs = passSetup();
    placePlayer(gs, 1, {5, 5}, TeamSide::HOME);
    gs.ball = BallState::carried({5, 5}, 1);
    placePlayer(gs, 13, {11, 5}, TeamSide::AWAY);
    Player& catcher = gs.getPlayer(2);
    catcher = *elf;
    catcher.id = 2;
    catcher.teamSide = TeamSide::HOME;
    catcher.state = PlayerState::STANDING;
    catcher.position = {10, 7};
    ASSERT_EQ(catcher.stats.agility, 4);

    FixedDiceRoller dice(inaccurateToTenSix({3}));
    std::vector<GameEvent> events;
    auto result = resolvePass(gs, 1, {10, 5}, dice, &events);

    EXPECT_EQ(divingCatchEvents(events), 1);
    ASSERT_TRUE(gs.ball.isHeld);
    EXPECT_EQ(gs.ball.carrierId, 2) << "chytil na 3+ (modifikátory jeho pole)";
    EXPECT_EQ(gs.getPlayer(2).position, (Position{10, 7}));
    EXPECT_FALSE(result.turnover);
}
