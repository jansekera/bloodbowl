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

TEST(BallHandler, ThrowInSideExitStraight) {
    GameState gs;
    // Ball exited off the TOP edge (offPitchExit y<0). LRB6 throw-in
    // template for a side exit: D6 1-2/3-4/5-6 = diagonal/straight-in/
    // diagonal. D6=3 -> straight in (0,+1). Distance 2D6=3+2=5:
    // (10,0) + (0,1)*5 = (10,5). Mandatory final bounce: D8=3 (East) -> (11,5).
    FixedDiceRoller dice({3, 3, 2, 3});
    resolveThrowIn(gs, {10, 0}, {10, -1}, dice, nullptr);
    EXPECT_EQ(gs.ball.position, (Position{11, 5}));
    EXPECT_FALSE(gs.ball.isHeld);
}

TEST(BallHandler, ThrowInSideExitDiagonal) {
    GameState gs;
    // Same TOP exit, but D6=1 (<=2) -> diagonal SW (-1,+1), not straight.
    // Distance 2D6=4+3=7: (10,0) + (-1,1)*7 = (3,7). Final bounce D8=3
    // (East) -> (4,7).
    FixedDiceRoller dice({1, 4, 3, 3});
    resolveThrowIn(gs, {10, 0}, {10, -1}, dice, nullptr);
    EXPECT_EQ(gs.ball.position, (Position{4, 7}));
}

TEST(BallHandler, ThrowInCornerExit) {
    GameState gs;
    // Ball exited off the TOP-LEFT corner (x<0 and y<0). LRB6 corner
    // throw-in: D3 picks straight-along-one-edge / diagonal / straight-
    // along-the-other-edge. D6=5 -> D3=(5+1)/2=3 -> "S, along the left
    // edge" (0,+1). Distance 2D6=2+2=4: (0,0) + (0,1)*4 = (0,4). Final
    // bounce D8=3 (East) -> (1,4).
    FixedDiceRoller dice({5, 2, 2, 3});
    resolveThrowIn(gs, {0, 0}, {-1, -1}, dice, nullptr);
    EXPECT_EQ(gs.ball.position, (Position{1, 4}));
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
    placePlayer(gs, 1, {10, 5}, TeamSide::HOME);
    // stejne kostky jako ThrowInSideExitStraight: dopad na (10,5), kde stoji
    // hráč 1 => chyt AG3, cil 4, hod 5 => uspech. Zadny odraz.
    FixedDiceRoller dice({3, 3, 2, 5});
    resolveThrowIn(gs, {10, 0}, {10, -1}, dice, nullptr);
    EXPECT_TRUE(gs.ball.isHeld);
    EXPECT_EQ(gs.ball.carrierId, 1);
    EXPECT_EQ(gs.ball.position, (Position{10, 5}));
}

TEST(BallHandler, ThrowInBouncesOffAPRONEPlayerInsteadOfBeingCaught) {
    // druha strana hranice, l. 872-874: "If the ball lands in an empty square
    // or a square occupied by a PRONE OR STUNNED player, then it will bounce."
    GameState gs;
    placePlayer(gs, 1, {10, 5}, TeamSide::HOME);
    gs.getPlayer(1).state = PlayerState::PRONE;
    // dopad (10,5) na leziciho => zadny chyt, odraz D8=3 (vychod) -> (11,5)
    FixedDiceRoller dice({3, 3, 2, 3});
    resolveThrowIn(gs, {10, 0}, {10, -1}, dice, nullptr);
    EXPECT_FALSE(gs.ball.isHeld);
    EXPECT_EQ(gs.ball.position, (Position{11, 5}));
}

TEST(BallHandler, ThrowInThatLeavesThePitchAgainIsThrownInAgain) {
    // l. 875-877: "If a throw-in results in the ball going off the pitch AGAIN,
    // it will be THROWN IN AGAIN, centred on the last square it was in before
    // it left the pitch." Do 24.08. se misto toho orizlo na kraj hriste.
    GameState gs;
    // 1. vhazeni z (10,0) po vystupu nahoru: D6=1 => diagonala (-1,+1),
    //    2D6 = 6+6 = 12. Mic jde po poli: (9,1)...(0,10) je posledni pole
    //    V HRISTI, dalsi krok (-1,11) uz je ven -- vlevo.
    // 2. vhazeni tedy z (0,10), NE z (10,0), a sablonou pro LEVY kraj:
    //    D6=3 => rovne dovnitr (+1,0), 2D6 = 2+2 = 4 => (4,10).
    //    Prazdne pole => odraz D8=3 (vychod) -> (5,10).
    FixedDiceRoller dice({1, 6, 6, 3, 2, 2, 3});
    resolveThrowIn(gs, {10, 0}, {10, -1}, dice, nullptr);
    EXPECT_TRUE(gs.ball.position.isOnPitch());
    EXPECT_EQ(gs.ball.position, (Position{5, 10}));
}

TEST(BallHandler, ThrowInFinalBounceOntoPlayer) {
    GameState gs;
    placePlayer(gs, 1, {11, 5}, TeamSide::HOME);
    // Same as ThrowInSideExitStraight, but a player sits at the final
    // bounce destination (11,5) and must attempt a catch (AG3 target 4,
    // roll 5 -> success).
    FixedDiceRoller dice({3, 3, 2, 3, 5});
    resolveThrowIn(gs, {10, 0}, {10, -1}, dice, nullptr);
    EXPECT_TRUE(gs.ball.isHeld);
    EXPECT_EQ(gs.ball.carrierId, 1);
}
