#include <gtest/gtest.h>
#include "bb/ball_and_chain_handler.h"
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

// scatterDirection: 1=N(0,-1), 2=NE(1,-1), 3=E(1,0), 4=SE(1,1),
//                   5=S(0,1), 6=SW(-1,1), 7=W(-1,0), 8=NW(-1,-1)

TEST(BallAndChainHandler, RandomMovement) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    placePlayer(gs, 1, {12, 7}, TeamSide::HOME, 4, 7, 1, 8);
    gs.getPlayer(1).skills.add(SkillName::BallAndChain);
    gs.getPlayer(1).skills.add(SkillName::NoHands);

    // MA=4, 4 moves. D8: 3,3,3,3 → each E(+1,0) → should end at (16,7)
    FixedDiceRoller dice({3, 3, 3, 3});
    auto result = resolveBallAndChain(gs, 1, dice, nullptr);
    EXPECT_TRUE(result.success);
    EXPECT_FALSE(result.turnover);
    EXPECT_EQ(gs.getPlayer(1).position, (Position{16, 7}));
}

TEST(BallAndChainHandler, OffPitchKO) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    placePlayer(gs, 1, {24, 7}, TeamSide::HOME, 4, 7, 1, 8);
    gs.getPlayer(1).skills.add(SkillName::BallAndChain);
    gs.getPlayer(1).skills.add(SkillName::NoHands);

    // D8=3 → E(+1,0) → (25,7). D8=3 → E(+1,0) → (26,7) off pitch → KO
    FixedDiceRoller dice({3, 3});
    auto result = resolveBallAndChain(gs, 1, dice, nullptr);
    EXPECT_FALSE(result.turnover); // Never turnover
    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::KO);
}

TEST(BallAndChainHandler, AutoBlockOccupied) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    placePlayer(gs, 1, {12, 7}, TeamSide::HOME, 2, 7, 1, 8);
    gs.getPlayer(1).skills.add(SkillName::BallAndChain);
    gs.getPlayer(1).skills.add(SkillName::NoHands);
    placePlayer(gs, 12, {13, 7}, TeamSide::AWAY); // Standing enemy

    // MA=2. Step 1: D8=3 → E(+1,0) → (13,7) occupied → auto-block
    // rollBlockDie uses rollD6: 6 → DD. Defender knocked down. Armor: 3+3=6
    // Step 2: D8=3 → E(+1,0) → (13,7) occupied by prone player, not standing → skip
    FixedDiceRoller dice({3, 6, 3, 3, 3});
    auto result = resolveBallAndChain(gs, 1, dice, nullptr);
    EXPECT_FALSE(result.turnover);
    EXPECT_EQ(gs.getPlayer(12).state, PlayerState::PRONE);
}

TEST(BallAndChainHandler, BCDownStops) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    placePlayer(gs, 1, {12, 7}, TeamSide::HOME, 4, 3, 1, 8);
    gs.getPlayer(1).skills.add(SkillName::BallAndChain);
    gs.getPlayer(1).skills.add(SkillName::NoHands);
    placePlayer(gs, 12, {13, 7}, TeamSide::AWAY);
    gs.getPlayer(12).skills.add(SkillName::Block);

    // Step 1: D8=3 → E(+1,0) → (13,7) occupied + standing → auto-block
    // rollBlockDie: D6=1 → AD. B&C player down. Armor: 3+3=6
    // B&C stops immediately.
    FixedDiceRoller dice({3, 1, 3, 3});
    auto result = resolveBallAndChain(gs, 1, dice, nullptr);
    EXPECT_FALSE(result.turnover); // Never turnover
    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::PRONE);
}

TEST(BallAndChainHandler, NoHandsBounce) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    placePlayer(gs, 1, {12, 7}, TeamSide::HOME, 2, 7, 1, 8);
    gs.getPlayer(1).skills.add(SkillName::BallAndChain);
    gs.getPlayer(1).skills.add(SkillName::NoHands);
    gs.ball = BallState::onGround({13, 7});

    // Step 1: D8=3 → E(+1,0) → (13,7) empty → move there. Ball on ground → bounce
    // Ball bounce: D8=3 → E(+1,0) → (14,7)
    // Step 2: D8=3 → E(+1,0) → (14,7) — ball on ground there → bounce again
    // Ball bounce: D8=3 → E(+1,0) → (15,7)
    FixedDiceRoller dice({3, 3, 3, 3});
    auto result = resolveBallAndChain(gs, 1, dice, nullptr);
    EXPECT_FALSE(result.turnover);
    EXPECT_FALSE(gs.ball.isHeld); // NoHands, can't pick up
}

TEST(BallAndChainHandler, NeverTurnover) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    placePlayer(gs, 1, {0, 7}, TeamSide::HOME, 2, 3, 1, 8);
    gs.getPlayer(1).skills.add(SkillName::BallAndChain);
    gs.getPlayer(1).skills.add(SkillName::NoHands);
    gs.ball = BallState::carried({0, 7}, 1);

    // D8=7 → W(-1,0) → (-1,7) off pitch → KO
    // Ball dropped before going off: bounce D8=3 → E(+1,0) → (1,7)
    FixedDiceRoller dice({7, 3});
    auto result = resolveBallAndChain(gs, 1, dice, nullptr);
    EXPECT_FALSE(result.turnover);
    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::KO);
}
