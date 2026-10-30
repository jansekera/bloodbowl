#include <gtest/gtest.h>
#include "bb/ball_handler.h"
#include "bb/helpers.h"

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

TEST(BallHandler, PickupSuccess) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.ball = BallState::onGround({10, 7});
    // AG3: target 3. Roll 4 → success
    FixedDiceRoller dice({4});
    bool ok = resolvePickup(gs, 1, dice, nullptr);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(gs.ball.isHeld);
    EXPECT_EQ(gs.ball.carrierId, 1);
}

TEST(BallHandler, PickupFail) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.ball = BallState::onGround({10, 7});
    // AG3: target 3. Roll 2 → fail, ball bounces (D8=3 → East → (11,7)).
    FixedDiceRoller dice({2, 3});
    bool ok = resolvePickup(gs, 1, dice, nullptr);
    EXPECT_FALSE(ok);
    EXPECT_FALSE(gs.ball.isHeld);
    EXPECT_EQ(gs.ball.position, (Position{11, 7}));
}

TEST(BallHandler, PickupNoHandsBounces) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::NoHands);
    gs.ball = BallState::onGround({10, 7});
    // Automatic fail, ball still bounces (D8=3 → East → (11,7)).
    FixedDiceRoller dice({3});
    bool ok = resolvePickup(gs, 1, dice, nullptr);
    EXPECT_FALSE(ok);
    EXPECT_FALSE(gs.ball.isHeld);
    EXPECT_EQ(gs.ball.position, (Position{11, 7}));
}

TEST(BallHandler, PickupSureHandsReroll) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::SureHands);
    gs.ball = BallState::onGround({10, 7});
    // Roll 2 (fail), SureHands reroll: 4 (success)
    FixedDiceRoller dice({2, 4});
    bool ok = resolvePickup(gs, 1, dice, nullptr);
    EXPECT_TRUE(ok);
}


TEST(BallHandler, CatchSuccess) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.ball = BallState::onGround({10, 7});
    // AG3: target 4. Roll 5 → success
    FixedDiceRoller dice({5});
    bool ok = resolveCatch(gs, 1, dice, 0, nullptr);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(gs.ball.isHeld);
    EXPECT_EQ(gs.ball.carrierId, 1);
}

TEST(BallHandler, CatchFail) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.ball = BallState::onGround({10, 7});
    // AG3: target 4. Roll 2 → fail
    FixedDiceRoller dice({2});
    bool ok = resolveCatch(gs, 1, dice, 0, nullptr);
    EXPECT_FALSE(ok);
}

TEST(BallHandler, CatchWithModifier) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.ball = BallState::onGround({10, 7});
    // AG3, modifier +1: target = 7-3-1 = 3. Roll 3 → success
    FixedDiceRoller dice({3});
    bool ok = resolveCatch(gs, 1, dice, 1, nullptr);
    EXPECT_TRUE(ok);
}

TEST(BallHandler, BounceToEmptySquare) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    // Ball bounces from (10,7). D8=3 → East → (11,7)
    FixedDiceRoller dice({3});
    resolveBounce(gs, {10, 7}, dice, 0, nullptr);
    EXPECT_EQ(gs.ball.position, (Position{11, 7}));
    EXPECT_FALSE(gs.ball.isHeld);
}

TEST(BallHandler, BounceToPlayer) {
    GameState gs;
    placePlayer(gs, 1, {11, 7}, TeamSide::HOME);
    // Ball bounces from (10,7). D8=3 → East → (11,7) where player is
    // Catch attempt: AG3 target 4, roll 5 → success
    FixedDiceRoller dice({3, 5});
    resolveBounce(gs, {10, 7}, dice, 0, nullptr);
    EXPECT_TRUE(gs.ball.isHeld);
    EXPECT_EQ(gs.ball.carrierId, 1);
}

TEST(BallHandler, BounceToPlayerFailsCatch) {
    GameState gs;
    placePlayer(gs, 1, {11, 7}, TeamSide::HOME);
    // Bounce → (11,7), catch fail (roll 2), then bounce again
    // Second bounce: D8=3 → (12,7), no player there
    FixedDiceRoller dice({3, 2, 3});
    resolveBounce(gs, {10, 7}, dice, 0, nullptr);
    EXPECT_FALSE(gs.ball.isHeld);
    EXPECT_EQ(gs.ball.position, (Position{12, 7}));
}

TEST(BallHandler, HandleBallOnPlayerDown) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.ball = BallState::carried({10, 7}, 1);
    // Ball bounces from player's position. D8=3 → (11,7)
    FixedDiceRoller dice({3});
    handleBallOnPlayerDown(gs, 1, dice, nullptr);
    EXPECT_FALSE(gs.ball.isHeld);
    EXPECT_EQ(gs.ball.position, (Position{11, 7}));
}

TEST(BallHandler, BounceOffPronePlayerContinues) {
    GameState gs;
    placePlayer(gs, 1, {11, 7}, TeamSide::HOME);
    gs.getPlayer(1).state = PlayerState::PRONE;
    // Bounce -> (11,7), a prone player -- no catch attempt (automatic
    // fail), ball keeps bouncing. Second bounce: D8=3 -> (12,7), empty.
    FixedDiceRoller dice({3, 3});
    resolveBounce(gs, {10, 7}, dice, 0, nullptr);
    EXPECT_FALSE(gs.ball.isHeld);
    EXPECT_EQ(gs.ball.position, (Position{12, 7}));
}

TEST(BallHandler, BounceChainsThroughMultiplePronePlayers) {
    GameState gs;
    placePlayer(gs, 1, {11, 7}, TeamSide::HOME);
    gs.getPlayer(1).state = PlayerState::PRONE;
    placePlayer(gs, 2, {12, 7}, TeamSide::HOME);
    gs.getPlayer(2).state = PlayerState::PRONE;
    // Bounce -> (11,7) prone, -> (12,7) prone, -> (13,7) empty. Each
    // D8=3 (East).
    FixedDiceRoller dice({3, 3, 3});
    resolveBounce(gs, {10, 7}, dice, 0, nullptr);
    EXPECT_FALSE(gs.ball.isHeld);
    EXPECT_EQ(gs.ball.position, (Position{13, 7}));
}

TEST(BallHandler, HandleBallOnPlayerDownFallsOnLooseBall) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.ball = BallState::onGround({10, 7});
    // Player (not the carrier) falls onto the loose ball's square -- it
    // scatters from under them. D8=3 -> East -> (11,7).
    FixedDiceRoller dice({3});
    handleBallOnPlayerDown(gs, 1, dice, nullptr);
    EXPECT_FALSE(gs.ball.isHeld);
    EXPECT_EQ(gs.ball.position, (Position{11, 7}));
}

TEST(BallHandler, HandleBallOnPlayerDownOffPitchSentinelNoOp) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).position = {-1, -1}; // e.g. ejected/KO'd off pitch
    // Ball defaults to on-ground at {-1,-1} (unset) -- must not be treated
    // as "player fell on the ball", or every off-pitch removal would
    // spuriously trigger a bounce roll. No dice supplied: throws if any
    // roll is consumed.
    FixedDiceRoller dice({});
    handleBallOnPlayerDown(gs, 1, dice, nullptr);
    EXPECT_FALSE(gs.ball.isHeld);
}

TEST(BallHandler, HandleBallOnPlayerDownNotCarrier) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.ball = BallState::carried({12, 7}, 2);
    // Player 1 doesn't have ball — no effect
    FixedDiceRoller dice({});
    handleBallOnPlayerDown(gs, 1, dice, nullptr);
    EXPECT_TRUE(gs.ball.isHeld);
    EXPECT_EQ(gs.ball.carrierId, 2);
}

// T5.24 / P145 (13.10.2026) -- BB2016 FAQ l. 9307-9314: "The square with the
// Blood Bowl logo over it COUNTS AS THE FIRST SQUARE of the ball's movement.
// So, if the result of the 2D6 roll was a 2, the ball would be placed in the
// square with the Blood Bowl logo over it, then move ONE square in the relevant
// direction." Do 13.10. letel mic plnych 2D6 poli od posledniho pole v hristi.
TEST(BallHandler, ThrowInOfTwoMovesTheBallOneSquareFromTheLastPitchSquare) {
    GameState gs;
    // presne priklad z FAQ: vystup nahoru z (10,0), D6=3 => rovne dovnitr,
    // 2D6 = 1+1 = 2 => JEDNO pole: (10,1). Odraz D8=3 (vychod) -> (11,1).
    FixedDiceRoller dice({3, 1, 1, 3});
    resolveThrowIn(gs, {10, 0}, {10, -1}, dice, nullptr);
    EXPECT_EQ(gs.ball.position, (Position{11, 1}));
}

TEST(BallHandler, ThrowInSideExitStraight) {
    GameState gs;
    // Ball exited off the TOP edge (offPitchExit y<0). LRB6 throw-in
    // template for a side exit: D6 1-2/3-4/5-6 = diagonal/straight-in/
    // diagonal. D6=3 -> straight in (0,+1). Distance 2D6=3+2=5, the first
    // square being (10,0) itself (FAQ l. 9307-9314): (10,0) + (0,1)*4 = (10,4).
    // Mandatory final bounce: D8=3 (East) -> (11,4).
    FixedDiceRoller dice({3, 3, 2, 3});
    resolveThrowIn(gs, {10, 0}, {10, -1}, dice, nullptr);
    EXPECT_EQ(gs.ball.position, (Position{11, 4}));
    EXPECT_FALSE(gs.ball.isHeld);
}

TEST(BallHandler, ThrowInSideExitDiagonal) {
    GameState gs;
    // Same TOP exit, but D6=1 (<=2) -> diagonal SW (-1,+1), not straight.
    // Distance 2D6=4+3=7, first square (10,0): (10,0) + (-1,1)*6 = (4,6).
    // Final bounce D8=3 (East) -> (5,6).
    FixedDiceRoller dice({1, 4, 3, 3});
    resolveThrowIn(gs, {10, 0}, {10, -1}, dice, nullptr);
    EXPECT_EQ(gs.ball.position, (Position{5, 6}));
}

TEST(BallHandler, ThrowInCornerExit) {
    GameState gs;
    // Ball exited off the TOP-LEFT corner (x<0 and y<0). LRB6 corner
    // throw-in: D3 picks straight-along-one-edge / diagonal / straight-
    // along-the-other-edge. D6=5 -> D3=(5+1)/2=3 -> "S, along the left
    // edge" (0,+1). Distance 2D6=2+2=4, first square (0,0): (0,0) + (0,1)*3
    // = (0,3). Final bounce D8=3 (East) -> (1,3).
    FixedDiceRoller dice({5, 2, 2, 3});
    resolveThrowIn(gs, {0, 0}, {-1, -1}, dice, nullptr);
    EXPECT_EQ(gs.ball.position, (Position{1, 3}));
}

// ============================================================================
// F9 (24.08.2026): ctyri puvodni ThrowIn testy pokryvaly vyhradne drahy, kde se
// kod s pravidly SHODOVAL (dopad na prazdne pole -> odraz). Nepokryte bylo
// presne to, kde byla vada. BB2016 l. 871-877.
// ============================================================================

TEST(BallHandler, ThrowInIsCaughtByAStandingPlayerInTheLandingSquare) {
    // l. 871-872: "If the ball is thrown into a square occupied by a STANDING
    // player, that player MUST attempt to catch the ball." Do 24.08. se
    // z pole dopadu VZDY odrazelo, takze vhozeny mic nikdo nikdy nechytil.
    GameState gs;
    placePlayer(gs, 1, {10, 4}, TeamSide::HOME);
    // stejne kostky jako ThrowInSideExitStraight: dopad na (10,4), kde stoji
    // hráč 1 => chyt AG3, cil 4, hod 5 => uspech. Zadny odraz.
    FixedDiceRoller dice({3, 3, 2, 5});
    resolveThrowIn(gs, {10, 0}, {10, -1}, dice, nullptr);
    EXPECT_TRUE(gs.ball.isHeld);
    EXPECT_EQ(gs.ball.carrierId, 1);
    EXPECT_EQ(gs.ball.position, (Position{10, 4}));
}

TEST(BallHandler, ThrowInBouncesOffAPRONEPlayerInsteadOfBeingCaught) {
    // druha strana hranice, l. 872-874: "If the ball lands in an empty square
    // or a square occupied by a PRONE OR STUNNED player, then it will bounce."
    GameState gs;
    placePlayer(gs, 1, {10, 4}, TeamSide::HOME);
    gs.getPlayer(1).state = PlayerState::PRONE;
    // dopad (10,4) na leziciho => zadny chyt, odraz D8=3 (vychod) -> (11,4)
    FixedDiceRoller dice({3, 3, 2, 3});
    resolveThrowIn(gs, {10, 0}, {10, -1}, dice, nullptr);
    EXPECT_FALSE(gs.ball.isHeld);
    EXPECT_EQ(gs.ball.position, (Position{11, 4}));
}

TEST(BallHandler, ThrowInThatLeavesThePitchAgainIsThrownInAgain) {
    // l. 875-877: "If a throw-in results in the ball going off the pitch AGAIN,
    // it will be THROWN IN AGAIN, centred on the last square it was in before
    // it left the pitch." Do 24.08. se misto toho orizlo na kraj hriste.
    GameState gs;
    // 1. vhazeni z (10,0) po vystupu nahoru: D6=1 => diagonala (-1,+1),
    //    2D6 = 6+6 = 12, tedy 11 kroku (prvni pole je (10,0), FAQ l. 9307-9314).
    //    Mic jde po poli: (9,1)...(0,10) je posledni pole V HRISTI (10. krok),
    //    11. krok (-1,11) uz je ven -- vlevo.
    // 2. vhazeni tedy z (0,10), NE z (10,0), a sablonou pro LEVY kraj:
    //    D6=3 => rovne dovnitr (+1,0), 2D6 = 2+2 = 4 => 3 kroky => (3,10).
    //    Prazdne pole => odraz D8=3 (vychod) -> (4,10).
    FixedDiceRoller dice({1, 6, 6, 3, 2, 2, 3});
    resolveThrowIn(gs, {10, 0}, {10, -1}, dice, nullptr);
    EXPECT_TRUE(gs.ball.position.isOnPitch());
    EXPECT_EQ(gs.ball.position, (Position{4, 10}));
}

TEST(BallHandler, ThrowInFinalBounceOntoPlayer) {
    GameState gs;
    placePlayer(gs, 1, {11, 4}, TeamSide::HOME);
    // Same as ThrowInSideExitStraight, but a player sits at the final
    // bounce destination (11,4) and must attempt a catch (AG3 target 4,
    // roll 5 -> success).
    FixedDiceRoller dice({3, 3, 2, 3, 5});
    resolveThrowIn(gs, {10, 0}, {10, -1}, dice, nullptr);
    EXPECT_TRUE(gs.ball.isHeld);
    EXPECT_EQ(gs.ball.carrierId, 1);
}

// Převod PHP opravy (P223 review LOW, `BallResolverTest::testBounceChainThroughThrowInsWith
// FailedCatchesStopsAtTheSafetyCap`): vhazování a dopad (`resolveThrowIn` → `resolveLanding`)
// startovaly hloubku odrazů od 0 ⇒ smyčka „odraz ven → vhazování → nechytí → odraz ven“ obcházela
// pojistku `depth > 200`. A na (6,0) a B na (6,1), AG1, oba nechytají (1); každý odraz na sever,
// vhazování kolmo dovnitř o jedno pole na B. Kostek je na 300 kol smyčky — bez pojistky dojdou.
TEST(BallHandler, BounceChainThroughThrowInsWithFailedCatchesStopsAtTheSafetyCap) {
    GameState gs;
    placePlayer(gs, 1, {6, 0}, TeamSide::HOME, 6, 3, 1);
    placePlayer(gs, 2, {6, 1}, TeamSide::HOME, 6, 3, 1);
    gs.ball = BallState::onGround({6, 0});
    // odraz z A ven (1); vhazování: šablona 3, 2D6 = 1+1 ⇒ B; B nechytí (1), odraz (1) ⇒ A;
    // A nechytí (1), odraz (1) ⇒ ven; …
    std::vector<int> rolls{1};
    for (int i = 0; i < 300; ++i) rolls.insert(rolls.end(), {3, 1, 1, 1, 1, 1, 1});
    FixedDiceRoller dice(rolls);
    EXPECT_NO_THROW(resolveBounce(gs, {6, 0}, dice, 0, nullptr));
    EXPECT_FALSE(gs.ball.isHeld);
    EXPECT_GT(dice.remaining(), 0u) << "řetěz odrazů se zastavil na pojistce";
}

// --- P64 (port PHP a7603e8f): team re-roll on a catch only in own turn,
// never on a kick-off landing. BB2016 l. 929-933 and l. 1261-1263.
static GameState catchFixture(TeamSide active) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    gs.activeTeam = active;
    gs.homeTeam.rerolls = 2;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.ball = BallState::onGround({10, 7});
    return gs;
}

TEST(BallHandler, CatchTeamRerollInOwnTurn) {
    // Positive control: without it the two tests below would pass for any
    // reason that stops a re-roll (e.g. no re-rolls in the fixture at all).
    GameState gs = catchFixture(TeamSide::HOME);
    FixedDiceRoller dice({2, 5});   // AG3 catch target 3: 2 fails, re-roll 5
    EXPECT_TRUE(resolveCatch(gs, 1, dice, 0, nullptr));
    EXPECT_EQ(gs.homeTeam.rerolls, 1);
}

TEST(BallHandler, CatchNoTeamRerollInOpponentTurn) {
    GameState gs = catchFixture(TeamSide::AWAY);
    FixedDiceRoller dice({2, 5});
    EXPECT_FALSE(resolveCatch(gs, 1, dice, 0, nullptr));
    EXPECT_EQ(gs.homeTeam.rerolls, 2);
}

TEST(BallHandler, CatchNoTeamRerollDuringKickoff) {
    GameState gs = catchFixture(TeamSide::HOME);
    {
        KickoffScope scope(gs);
        FixedDiceRoller dice({2, 5});
        EXPECT_FALSE(resolveCatch(gs, 1, dice, 0, nullptr));
    }
    EXPECT_EQ(gs.homeTeam.rerolls, 2);
    EXPECT_FALSE(gs.kickoffInProgress);   // scope cleared the flag
}

// P165 (07.10.2026): Diving Catch — ř. 8063-8065 "The player may add 1 to any catch roll from
// an ACCURATE PASS targeted to his square." Engine dával +1 na KAŽDÉ chytání (odskok, předání,
// výkop). AG3: přesná přihrávka 7-3-1 = 3+, s Diving Catch 2+; odskok 7-3 = 4+ i s Diving Catch.
TEST(BallHandler, DivingCatchHelpsOnlyAnAccuratePassToHisSquare) {
    auto withBall = [] {
        GameState gs;
        placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
        gs.getPlayer(1).skills.add(SkillName::DivingCatch);
        gs.ball = BallState::onGround({10, 7});
        return gs;
    };
    {   // přesná přihrávka: hod 2 stačí (bez dovednosti by bylo potřeba 3)
        GameState gs = withBall();
        FixedDiceRoller dice({2});
        EXPECT_TRUE(resolveCatch(gs, 1, dice, 1, nullptr, /*accuratePass=*/true));
    }
    {   // odskočený míč: 4+, hod 3 nestačí — Diving Catch tu nepomáhá
        GameState gs = withBall();
        FixedDiceRoller dice({3, 1, 1, 1, 1, 1, 1, 1});
        EXPECT_FALSE(resolveCatch(gs, 1, dice, 0, nullptr));
    }
    {   // předání (+1 za předání, ale není to přesná PŘIHRÁVKA): 3+, hod 2 nestačí
        GameState gs = withBall();
        FixedDiceRoller dice({2, 1, 1, 1, 1, 1, 1, 1});
        EXPECT_FALSE(resolveCatch(gs, 1, dice, 1, nullptr));
    }
}

// ---------------------------------------------------------------------------
// NÁLEZ 19 (audit parity 08.10.2026) — CHYTÁNÍ BEZ TACKLE ZÓN. Pravidla ř. 7983-7986 (Bone-head):
// „The player loses his tackle zones and MAY NOT CATCH, INTERCEPT OR PASS, assist another player
// on a block or foul, or voluntarily move until he manages to roll a 2 or better“; totéž Really
// Stupid a Hypnotic Gaze (ř. 8185-8188: „loses his tackle zones and may not catch, intercept or
// pass the ball“). Dřív `resolveCatch` hlídal jen stav (stojí / leží).
// ---------------------------------------------------------------------------
TEST(BallHandler, N19PlayerWhoLostHisTacklezonesMayNotCatch) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).lostTacklezones = true;
    gs.ball = BallState::onGround({10, 7});

    FixedDiceRoller dice({6});                       // hod, který by chycení dal
    EXPECT_FALSE(resolveCatch(gs, 1, dice, 0, nullptr));
    EXPECT_FALSE(gs.ball.isHeld);
    EXPECT_EQ(dice.remaining(), 1u) << "na chycení se vůbec nehází";
}

// Odraz na takového hráče: nechytá, míč se odráží dál (ř. 903-906). Kostky: D8 = 5 (jih) ⇒ z (10,6)
// na (10,7), kde stojí; dál 6 — stará mechanika ji vzala jako úspěšný hod na chycení, nová jako
// D8 dalšího odrazu (6 = jihozápad) ⇒ (9,8).
TEST(BallHandler, N19BallBouncingOntoAPlayerWithoutTacklezonesBouncesOn) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).lostTacklezones = true;

    FixedDiceRoller dice({5, 6});
    resolveBounce(gs, {10, 6}, dice, 0, nullptr);

    EXPECT_FALSE(gs.ball.isHeld);
    EXPECT_EQ(gs.ball.position, (Position{9, 8}));
}

// Pozitivní kontrola: tentýž hráč SE zónami týmiž kostkami míč chytí.
TEST(BallHandler, N19SamePlayerWithTacklezonesCatchesTheBounce) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);

    FixedDiceRoller dice({5, 6});
    resolveBounce(gs, {10, 6}, dice, 0, nullptr);

    EXPECT_TRUE(gs.ball.isHeld);
    EXPECT_EQ(gs.ball.carrierId, 1);
}

// P145 (e), 13.10.2026: PICKUP a CATCH nesou v `die1` padlou kostku (v `roll`
// zůstává cílové číslo) -- viz test_move_handler.cpp.
TEST(BallHandler, P145PickupAndCatchEventsCarryTheDieThatWasRolled) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.ball = BallState::onGround({10, 7});
    FixedDiceRoller dice({5});
    std::vector<GameEvent> evs;
    ASSERT_TRUE(resolvePickup(gs, 1, dice, &evs));
    ASSERT_FALSE(evs.empty());
    EXPECT_EQ(evs.back().type, GameEvent::Type::PICKUP);
    EXPECT_EQ(evs.back().die1, 5);

    GameState gc;
    placePlayer(gc, 1, {10, 7}, TeamSide::HOME);
    gc.ball = BallState::onGround({10, 7});
    FixedDiceRoller diceC({6});
    std::vector<GameEvent> evc;
    ASSERT_TRUE(resolveCatch(gc, 1, diceC, 0, &evc));
    ASSERT_FALSE(evc.empty());
    EXPECT_EQ(evc.back().type, GameEvent::Type::CATCH);
    EXPECT_EQ(evc.back().die1, 6);
}

// P145 (a), výpis: vhazování a odraz jsou obojí BALL_BOUNCE a ve výpisu partie
// 02.10. se nedaly rozlišit (D8 1-8 × 2D6 2-12 se překrývají). Vhazování nese
// v `die1` kostku šablony (D6), odraz tam má 0.
TEST(BallHandler, P145ThrowInEventCarriesTheTemplateDieAndABounceDoesNot) {
    GameState gs;
    FixedDiceRoller dice({3, 3, 2, 3});   // šablona 3, 2D6 = 5, odraz D8 = 3
    std::vector<GameEvent> evs;
    resolveThrowIn(gs, {10, 0}, {10, -1}, dice, &evs);
    ASSERT_EQ(evs.size(), 2u);
    EXPECT_EQ(evs[0].type, GameEvent::Type::BALL_BOUNCE);
    EXPECT_EQ(evs[0].roll, 5);
    EXPECT_EQ(evs[0].die1, 3) << "vhazování: kostka šablony";
    EXPECT_EQ(evs[0].to, (Position{10, 4}));
    EXPECT_EQ(evs[1].type, GameEvent::Type::BALL_BOUNCE);
    EXPECT_EQ(evs[1].die1, 0) << "odraz: žádná kostka šablony";
}
