#include <gtest/gtest.h>
#include "bb/ball_and_chain_handler.h"
#include "bb/helpers.h"
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
// P72 (30.09.2026) -- port PHP e373bf23 + 04e4946c; rozhodnuti uzivatele 25.09.
// (znovu 30.09.: "vzdy k souperi a pozor na svoje i lezici"). BB2016
// r. 7820-7850:
//  - smer kazdeho kroku = sablona vhazovani (natoceni voli kouc) + D6;
//    natoceni: k nejblizsimu STOJICIMU souperi, vyhnout se VSEM nasim;
//  - obsazene pole: blok podle beznych pravidel (sila + asistence), povinny
//    follow-up; lezici/omraceny = odtlacit + hod na brneni;
//  - mimo hriste: dav (hod na zraneni, ne automaticke KO), turnover NENI;
//  - sraženy B&C: rovnou hod na zraneni bez brneni, Stunned = KO; sraženy
//    pri BLOKU je hrac tymu na tahu => turnover (r. 368).
// Stare testy kodovaly D8, auto-KO v davu a "nikdy turnover".
// Sablona: D6 1-2 = vlevo od smeru, 3-4 = rovne, 5-6 = vpravo.
// ============================================================================

static void makeBnC(GameState& gs, int id, Position pos, int ma, int st) {
    placePlayer(gs, id, pos, TeamSide::HOME, ma, st, 1, 8);
    gs.getPlayer(id).skills.add(SkillName::BallAndChain);
    gs.getPlayer(id).skills.add(SkillName::NoHands);
}

static GameState bncState() {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    gs.activeTeam = TeamSide::HOME;
    return gs;
}

TEST(BallAndChainHandler, P72WalksTheTemplateTowardTheNearestStandingOpponent) {
    auto gs = bncState();
    makeBnC(gs, 1, {10, 7}, 2, 7);
    placePlayer(gs, 12, {20, 7}, TeamSide::AWAY);
    FixedDiceRoller dice({3, 3});                   // rovne, rovne
    auto r = resolveBallAndChain(gs, 1, dice, nullptr);
    EXPECT_FALSE(r.turnover);
    EXPECT_EQ(gs.getPlayer(1).position, (Position{12, 7}));
}

TEST(BallAndChainHandler, P72TurnsAwayFromOwnPlayersEvenProneOnes) {
    auto gs = bncState();
    makeBnC(gs, 1, {10, 7}, 1, 7);
    placePlayer(gs, 12, {20, 7}, TeamSide::AWAY);
    for (int id : {2, 3, 4}) {
        placePlayer(gs, id, {11, 6 + (id - 2)}, TeamSide::HOME);
        gs.getPlayer(id).state = PlayerState::PRONE;
    }
    FixedDiceRoller dice({3});
    resolveBallAndChain(gs, 1, dice, nullptr);
    EXPECT_EQ(gs.getPlayer(1).position, (Position{9, 7})) << "natocil se na vlastni lezici";
}

TEST(BallAndChainHandler, P72TheCrowdIsAnInjuryRollNotAnAutomaticKO) {
    auto gs = bncState();
    makeBnC(gs, 1, {0, 7}, 1, 3);
    for (int id : {2, 3, 4, 5, 6}) placePlayer(gs, id, {0, 0}, TeamSide::HOME);
    gs.getPlayer(2).position = {1, 6};
    gs.getPlayer(3).position = {1, 7};
    gs.getPlayer(4).position = {1, 8};
    gs.getPlayer(5).position = {0, 6};
    gs.getPlayer(6).position = {0, 8};
    // jen smer -x (tri pole v davu) je lepsi nez vlastni hraci
    FixedDiceRoller dice({3, 5, 5, 6, 6, 6, 6});    // rovne = dav; zraneni 10 = casualty
    auto r = resolveBallAndChain(gs, 1, dice, nullptr);
    EXPECT_FALSE(r.turnover) << "dav turnover neni (r. 369-370)";
    EXPECT_NE(gs.getPlayer(1).state, PlayerState::KO) << "dav hodil automaticke KO";
    EXPECT_FALSE(gs.getPlayer(1).isOnPitch());
}

TEST(BallAndChainHandler, P72KnockedDownInABlockIsAnInjuryRollStunnedIsKOAndATurnover) {
    auto gs = bncState();
    makeBnC(gs, 1, {10, 7}, 1, 3);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    FixedDiceRoller dice({3, 1, 3, 3});             // rovne => blok; AD; zraneni 6 = Stunned
    auto r = resolveBallAndChain(gs, 1, dice, nullptr);
    EXPECT_TRUE(r.turnover) << "sraženy pri bloku = turnover (r. 368)";
    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::KO) << "Stunned se u B&C pocita jako KO";
}

TEST(BallAndChainHandler, P72BlocksWithStrengthDiceAndMustFollowUp) {
    auto gs = bncState();
    makeBnC(gs, 1, {10, 7}, 1, 7);                  // ST7 proti ST3 = 3 kostky
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    FixedDiceRoller dice({3, 1, 1, 6, 1, 1, 1, 1}); // rovne; AD AD DD => DD; brneni 1+1
    auto r = resolveBallAndChain(gs, 1, dice, nullptr);
    EXPECT_FALSE(r.turnover);
    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::STANDING) << "hazela se jedna kostka";
    EXPECT_EQ(gs.getPlayer(12).state, PlayerState::PRONE);
    EXPECT_EQ(gs.getPlayer(1).position, (Position{11, 7})) << "povinny follow-up";
}

TEST(BallAndChainHandler, P72AProneOrStunnedPlayerInTheWayIsPushedAndArmourRolled) {
    auto gs = bncState();
    makeBnC(gs, 1, {10, 7}, 1, 5);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    gs.getPlayer(12).state = PlayerState::PRONE;
    placePlayer(gs, 13, {20, 7}, TeamSide::AWAY);   // nejblizsi STOJICI
    FixedDiceRoller dice({3, 1, 1, 1, 1});          // rovne; brneni 1+1
    resolveBallAndChain(gs, 1, dice, nullptr);
    EXPECT_EQ(gs.getPlayer(1).position, (Position{11, 7}));
    EXPECT_NE(gs.getPlayer(12).position, (Position{11, 7}));
    EXPECT_EQ(gs.getPlayer(12).state, PlayerState::PRONE);
}

TEST(BallAndChainHandler, NeverEntersASquareStillOccupied) {
    // ⛔ INVARIANT: dva hraci na jednom poli jsou rozbity stav.
    auto gs = bncState();
    makeBnC(gs, 1, {10, 7}, 4, 5);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    gs.getPlayer(12).state = PlayerState::PRONE;
    placePlayer(gs, 13, {20, 7}, TeamSide::AWAY);
    FixedDiceRoller dice(std::vector<int>(80, 3));
    resolveBallAndChain(gs, 1, dice, nullptr);
    for (int a : {1, 12, 13})
        for (int b : {1, 12, 13})
            if (a < b && gs.getPlayer(a).isOnPitch() && gs.getPlayer(b).isOnPitch())
                EXPECT_NE(gs.getPlayer(a).position, gs.getPlayer(b).position) << a << " a " << b;
}

TEST(BallAndChainHandler, NoHandsBounce) {
    auto gs = bncState();
    makeBnC(gs, 1, {10, 7}, 1, 7);
    placePlayer(gs, 12, {20, 7}, TeamSide::AWAY);
    gs.ball = BallState::onGround({11, 7});
    FixedDiceRoller dice({3, 3, 3, 3});             // rovne na mic; odraz D8 3 = E
    resolveBallAndChain(gs, 1, dice, nullptr);
    EXPECT_FALSE(gs.ball.isHeld);                   // No Hands
    EXPECT_EQ(gs.getPlayer(1).position, (Position{11, 7}));
}
