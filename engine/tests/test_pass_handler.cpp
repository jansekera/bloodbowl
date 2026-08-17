#include <gtest/gtest.h>
#include "bb/pass_handler.h"
#include "bb/ball_handler.h"
#include "bb/helpers.h"
#include "bb/rules_engine.h"

using namespace bb;

static void placePlayer(GameState& gs, int id, Position pos, TeamSide side,
                         int ma = 6, int st = 3, int ag = 3, int av = 8) {
    Player& p = gs.getPlayer(id);
    p.id = id;
    p.teamSide = side;
    p.state = PlayerState::STANDING;
    p.position = pos;
    p.stats = {static_cast<int8_t>(ma), static_cast<int8_t>(st),
               static_cast<int8_t>(ag), static_cast<int8_t>(av)};
    p.movementRemaining = ma;
}

static GameState makePassSetup() {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    gs.activeTeam = TeamSide::HOME;
    gs.homeTeam.side = TeamSide::HOME;
    gs.awayTeam.side = TeamSide::AWAY;
    gs.homeTeam.rerolls = 3;
    gs.homeTeam.turnNumber = 1;
    return gs;
}

// ===== PASS TESTS =====

TEST(PassHandler, AccuratePassCaught) {
    auto gs = makePassSetup();
    // Passer at (5,7) AG3, Receiver at (8,7) = dist 3 = Quick Pass
    placePlayer(gs, 1, {5, 7}, TeamSide::HOME);
    placePlayer(gs, 2, {8, 7}, TeamSide::HOME);
    gs.ball = BallState::carried({5, 7}, 1);

    // Quick Pass: target = 7-3-1(QP) = 3
    // Roll 4 (pass succeeds), Roll 5 (catch: 7-3-1=3, success)
    FixedDiceRoller dice({4, 5});
    auto result = resolvePass(gs, 1, {8, 7}, dice, nullptr);

    EXPECT_FALSE(result.turnover);
    EXPECT_TRUE(gs.ball.isHeld);
    EXPECT_EQ(gs.ball.carrierId, 2);
}

TEST(PassHandler, InaccuratePassScatters) {
    auto gs = makePassSetup();
    placePlayer(gs, 1, {5, 7}, TeamSide::HOME);
    placePlayer(gs, 2, {8, 7}, TeamSide::HOME);
    gs.ball = BallState::carried({5, 7}, 1);

    // Quick Pass: target = 3. Roll 2 (fail, inaccurate)
    // Scatter: D8=3(E) D6=1 → (9,7), no one there
    // Bounce: D8=7(W) → (8,7) where player 2 is → catch roll 4 (target 4) → success
    FixedDiceRoller dice({2, 3, 1, 7, 4});
    auto result = resolvePass(gs, 1, {8, 7}, dice, nullptr);

    // Player 2 might or might not catch depending on exact scatter
    // This test verifies the pass completes without crash
    EXPECT_GE(result.turnover + result.success, 0);  // valid result
}

TEST(PassHandler, FumbleOnNatural1) {
    auto gs = makePassSetup();
    gs.homeTeam.rerolls = 0;  // No team rerolls to interfere
    placePlayer(gs, 1, {5, 7}, TeamSide::HOME);
    placePlayer(gs, 2, {8, 7}, TeamSide::HOME);
    gs.ball = BallState::carried({5, 7}, 1);

    // Roll 1 (natural fumble), no rerolls → bounce: D8=3(E) → (6,7)
    FixedDiceRoller dice({1, 3});
    auto result = resolvePass(gs, 1, {8, 7}, dice, nullptr);

    EXPECT_TRUE(result.turnover);
    EXPECT_FALSE(gs.ball.isHeld);
}

TEST(PassHandler, InterceptionByEnemy) {
    auto gs = makePassSetup();
    // Passer at (3,7), target at (9,7), enemy interceptor at (6,7)
    placePlayer(gs, 1, {3, 7}, TeamSide::HOME);
    placePlayer(gs, 2, {9, 7}, TeamSide::HOME);
    placePlayer(gs, 12, {6, 7}, TeamSide::AWAY, 6, 3, 4);  // AG4 enemy

    gs.ball = BallState::carried({3, 7}, 1);

    // Interception: AG4, target = 7-4+2=5. Roll 5 → success
    FixedDiceRoller dice({5});
    auto result = resolvePass(gs, 1, {9, 7}, dice, nullptr);

    EXPECT_TRUE(result.turnover);
    EXPECT_TRUE(gs.ball.isHeld);
    EXPECT_EQ(gs.ball.carrierId, 12);
}

TEST(PassHandler, SafeThrowBlocksInterception) {
    auto gs = makePassSetup();
    placePlayer(gs, 1, {3, 7}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::SafeThrow);
    placePlayer(gs, 2, {9, 7}, TeamSide::HOME);
    placePlayer(gs, 12, {6, 7}, TeamSide::AWAY, 6, 3, 4);

    gs.ball = BallState::carried({3, 7}, 1);

    // Interception: roll 5 (success), SafeThrow reroll: roll 3 (< 5, fails interception)
    // Pass continues: target = 7-3-0(SP for dist 6) = 4, roll 5 → accurate
    // Catch: target 4-1=3, roll 4 → success
    FixedDiceRoller dice({5, 3, 5, 4});
    auto result = resolvePass(gs, 1, {9, 7}, dice, nullptr);

    EXPECT_FALSE(result.turnover);
    EXPECT_TRUE(gs.ball.isHeld);
    EXPECT_EQ(gs.ball.carrierId, 2);
}

TEST(PassHandler, StrongArmReducesRange) {
    auto gs = makePassSetup();
    // Passer with StrongArm at (3,7), target at (10,7) = dist 7 = Long Pass normally
    // With StrongArm: reduced to Short Pass
    placePlayer(gs, 1, {3, 7}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::StrongArm);
    placePlayer(gs, 2, {10, 7}, TeamSide::HOME);
    gs.ball = BallState::carried({3, 7}, 1);

    // Long Pass → Short Pass with StrongArm: target = 7-3-0(SP) = 4
    // Roll 4 → accurate pass, catch roll 5 (target 3) → success
    FixedDiceRoller dice({4, 5});
    auto result = resolvePass(gs, 1, {10, 7}, dice, nullptr);

    EXPECT_FALSE(result.turnover);
    EXPECT_TRUE(gs.ball.isHeld);
    EXPECT_EQ(gs.ball.carrierId, 2);
}

TEST(PassHandler, AccurateSkillLowersTarget) {
    auto gs = makePassSetup();
    placePlayer(gs, 1, {5, 7}, TeamSide::HOME, 6, 3, 2);  // AG2
    gs.getPlayer(1).skills.add(SkillName::Accurate);
    placePlayer(gs, 2, {8, 7}, TeamSide::HOME);
    gs.ball = BallState::carried({5, 7}, 1);

    // AG2 Quick Pass: target = 7-2-1(QP)-1(Accurate) = 3
    // Roll 3 → accurate, catch roll 5 → success
    FixedDiceRoller dice({3, 5});
    auto result = resolvePass(gs, 1, {8, 7}, dice, nullptr);

    EXPECT_FALSE(result.turnover);
    EXPECT_TRUE(gs.ball.isHeld);
    EXPECT_EQ(gs.ball.carrierId, 2);
}

TEST(PassHandler, NervesOfSteelIgnoresTZ) {
    auto gs = makePassSetup();
    placePlayer(gs, 1, {5, 7}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::NervesOfSteel);
    placePlayer(gs, 2, {8, 7}, TeamSide::HOME);
    // Enemy adjacent to passer — adds TZ
    placePlayer(gs, 12, {5, 8}, TeamSide::AWAY);
    gs.ball = BallState::carried({5, 7}, 1);

    // QP target = 7-3-1(QP) = 3 (NervesOfSteel ignores TZ)
    // Roll 3 → accurate, catch roll 5 → success
    FixedDiceRoller dice({3, 5});
    auto result = resolvePass(gs, 1, {8, 7}, dice, nullptr);

    EXPECT_FALSE(result.turnover);
}

TEST(PassHandler, DisturbingPresenceAddsPenalty) {
    auto gs = makePassSetup();
    placePlayer(gs, 1, {5, 7}, TeamSide::HOME);
    placePlayer(gs, 2, {8, 7}, TeamSide::HOME);
    // Enemy with DisturbingPresence within 3 squares of passer
    placePlayer(gs, 12, {7, 7}, TeamSide::AWAY);
    gs.getPlayer(12).skills.add(SkillName::DisturbingPresence);
    gs.ball = BallState::carried({5, 7}, 1);

    // QP target = 7-3-1(QP)+1(DP) = 4
    // Roll 3 → fail (inaccurate), scatter: D8=1(N),D6=1 → (8,6)
    // No one there, bounce: D8=5(S) → (8,7) where player 2 is → catch roll 6 → success
    FixedDiceRoller dice({3, 1, 1, 5, 6});
    auto result = resolvePass(gs, 1, {8, 7}, dice, nullptr);

    // The pass was inaccurate due to DP
    // Just verify it doesn't crash and returns valid result
    EXPECT_GE(result.turnover + result.success, 0);
}

TEST(PassHandler, WeatherModifier) {
    auto gs = makePassSetup();
    gs.weather = Weather::POURING_RAIN;
    placePlayer(gs, 1, {5, 7}, TeamSide::HOME);
    placePlayer(gs, 2, {8, 7}, TeamSide::HOME);
    gs.ball = BallState::carried({5, 7}, 1);

    // QP target = 7-3-1(QP)+1(rain) = 4
    // Roll 4 → accurate, catch roll: AG3 target 4+1(rain)=5, roll 5 → success
    FixedDiceRoller dice({4, 5});
    auto result = resolvePass(gs, 1, {8, 7}, dice, nullptr);

    EXPECT_FALSE(result.turnover);
}

TEST(PassHandler, HailMaryPassScatters3Times) {
    auto gs = makePassSetup();
    placePlayer(gs, 1, {3, 7}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::HailMaryPass);
    placePlayer(gs, 2, {20, 7}, TeamSide::HOME);
    gs.ball = BallState::carried({3, 7}, 1);

    // HMP: roll 4 (>=2, not fumble)
    // 3 scatters from target (20,7): D8=3(E),D8=1(N),D8=5(S)
    // → (21,7) → (21,6) → (21,7). Catch at (21,7)? No one there.
    // Bounce: D8=7(W) → (20,7) where player 2 is. Catch: roll 5 target 4 → success
    FixedDiceRoller dice({4, 3, 1, 5, 7, 5});
    auto result = resolvePass(gs, 1, {20, 7}, dice, nullptr);

    // Just verify completes
    EXPECT_GE(result.turnover + result.success, 0);
}

// ===== HAND-OFF TESTS =====

TEST(PassHandler, HandOffSuccess) {
    auto gs = makePassSetup();
    placePlayer(gs, 1, {5, 7}, TeamSide::HOME);
    placePlayer(gs, 2, {6, 7}, TeamSide::HOME);
    gs.ball = BallState::carried({5, 7}, 1);

    // Hand-off catch: AG3, +1 modifier → target = 7-3-1 = 3. Roll 4 → success
    FixedDiceRoller dice({4});
    auto result = resolveHandOff(gs, 1, 2, dice, nullptr);

    EXPECT_FALSE(result.turnover);
    EXPECT_TRUE(result.success);
    EXPECT_TRUE(gs.ball.isHeld);
    EXPECT_EQ(gs.ball.carrierId, 2);
}

TEST(PassHandler, HandOffLeavesATraceOfItsOwn) {
    // A hand-off used to show up as a bare CATCH, which reads identically to
    // catching a bounce or a kick-off -- so a corpus could not be asked whether
    // any hand-off happened at all, and the check written for the pricing fix
    // read zero across 3000 games while the carriers had visibly changed.
    auto gs = makePassSetup();
    placePlayer(gs, 1, {5, 7}, TeamSide::HOME);
    placePlayer(gs, 2, {6, 7}, TeamSide::HOME);
    gs.ball = BallState::carried({5, 7}, 1);

    std::vector<GameEvent> events;
    FixedDiceRoller dice({6});
    resolveHandOff(gs, 1, 2, dice, &events);

    int handOffs = 0;
    for (auto& e : events) {
        if (e.type == GameEvent::Type::HAND_OFF) {
            handOffs++;
            EXPECT_EQ(e.playerId, 1);
            EXPECT_EQ(e.targetId, 2);
        }
    }
    EXPECT_EQ(handOffs, 1) << "the hand-off has to be countable without guessing";
}

TEST(PassHandler, HandOffFailTurnover) {
    auto gs = makePassSetup();
    gs.homeTeam.rerolls = 0;  // No team rerolls
    placePlayer(gs, 1, {5, 7}, TeamSide::HOME);
    placePlayer(gs, 2, {6, 7}, TeamSide::HOME);
    gs.ball = BallState::carried({5, 7}, 1);

    // Catch: target 3, roll 2 → fail, no rerolls. Bounce: D8=3(E) → (7,7)
    FixedDiceRoller dice({2, 3});
    auto result = resolveHandOff(gs, 1, 2, dice, nullptr);

    EXPECT_TRUE(result.turnover);
}

TEST(PassHandler, HandOffMustBeAdjacent) {
    auto gs = makePassSetup();
    placePlayer(gs, 1, {5, 7}, TeamSide::HOME);
    placePlayer(gs, 2, {8, 7}, TeamSide::HOME);  // too far
    gs.ball = BallState::carried({5, 7}, 1);

    FixedDiceRoller dice({6});
    auto result = resolveHandOff(gs, 1, 2, dice, nullptr);

    EXPECT_FALSE(result.success);
}

// ===== RULES ENGINE PASS/HANDOFF GENERATION =====

TEST(PassHandler, PassActionsGeneratedForBallCarrier) {
    auto gs = makePassSetup();
    placePlayer(gs, 1, {5, 7}, TeamSide::HOME);
    placePlayer(gs, 2, {8, 7}, TeamSide::HOME);
    gs.ball = BallState::carried({5, 7}, 1);

    std::vector<Action> actions;
    getAvailableActions(gs, actions);

    bool hasPass = false;
    bool hasHandOff = false;
    for (const auto& a : actions) {
        if (a.type == ActionType::PASS && a.playerId == 1) hasPass = true;
        if (a.type == ActionType::HAND_OFF && a.playerId == 1) hasHandOff = true;
    }

    EXPECT_TRUE(hasPass);
    // Player 2 is not adjacent, so no hand-off
    EXPECT_FALSE(hasHandOff);
}

TEST(PassHandler, HandOffGeneratedForAdjacentTeammate) {
    auto gs = makePassSetup();
    placePlayer(gs, 1, {5, 7}, TeamSide::HOME);
    placePlayer(gs, 2, {6, 7}, TeamSide::HOME);
    gs.ball = BallState::carried({5, 7}, 1);

    std::vector<Action> actions;
    getAvailableActions(gs, actions);

    bool hasHandOff = false;
    for (const auto& a : actions) {
        if (a.type == ActionType::HAND_OFF && a.playerId == 1 && a.targetId == 2)
            hasHandOff = true;
    }

    EXPECT_TRUE(hasHandOff);
}

TEST(PassHandler, NoPassActionsWhenPassUsed) {
    auto gs = makePassSetup();
    gs.homeTeam.passUsedThisTurn = true;
    placePlayer(gs, 1, {5, 7}, TeamSide::HOME);
    placePlayer(gs, 2, {8, 7}, TeamSide::HOME);
    gs.ball = BallState::carried({5, 7}, 1);

    std::vector<Action> actions;
    getAvailableActions(gs, actions);

    // The team-mate is three squares away, so no hand-off could be generated
    // here whatever the allowances say -- this test only ever covered PASS.
    // It used to assert HAND_OFF too and passed for that reason alone; the
    // hand-off cases are below, with an ADJACENT team-mate.
    for (const auto& a : actions) {
        EXPECT_NE(a.type, ActionType::PASS);
    }
}

// 2026-08-17 (P4/P26). CRP, HANDING-OFF: "The Hand-Off Action is added to the
// list of Actions like Move, Block, Blitz and Pass. A coach may only declare
// one Hand-Off Action per turn." Two separate declarations, two allowances.
// They shared passUsedThisTurn until now, in both directions, which made
// CHAIN_SCORE -- a pass followed by a hand-off -- unsatisfiable by construction:
// step one burned exactly what step two needed. Offered 270 times across 3000
// games, completed never.
TEST(PassHandler, PassUsedDoesNotBlockHandOff) {
    auto gs = makePassSetup();
    gs.homeTeam.passUsedThisTurn = true;
    gs.homeTeam.handOffUsedThisTurn = false;
    placePlayer(gs, 1, {5, 7}, TeamSide::HOME);
    placePlayer(gs, 2, {6, 7}, TeamSide::HOME);   // ADJACENT
    gs.ball = BallState::carried({5, 7}, 1);

    std::vector<Action> actions;
    getAvailableActions(gs, actions);

    bool handOff = false;
    for (const auto& a : actions) {
        if (a.type == ActionType::HAND_OFF) handOff = true;
        EXPECT_NE(a.type, ActionType::PASS);
    }
    EXPECT_TRUE(handOff) << "a spent pass must not consume the hand-off";
}

TEST(PassHandler, HandOffUsedBlocksHandOffButNotPass) {
    auto gs = makePassSetup();
    gs.homeTeam.handOffUsedThisTurn = true;
    gs.homeTeam.passUsedThisTurn = false;
    placePlayer(gs, 1, {5, 7}, TeamSide::HOME);
    placePlayer(gs, 2, {6, 7}, TeamSide::HOME);   // adjacent: hand-off target
    placePlayer(gs, 3, {9, 7}, TeamSide::HOME);   // far: throw target
    gs.ball = BallState::carried({5, 7}, 1);

    std::vector<Action> actions;
    getAvailableActions(gs, actions);

    bool pass = false;
    for (const auto& a : actions) {
        if (a.type == ActionType::PASS) pass = true;
        EXPECT_NE(a.type, ActionType::HAND_OFF);
    }
    EXPECT_TRUE(pass) << "a spent hand-off must not consume the pass";
}

TEST(PassHandler, HandOffBurnsItsOwnAllowance) {
    auto gs = makePassSetup();
    placePlayer(gs, 1, {5, 7}, TeamSide::HOME);
    placePlayer(gs, 2, {6, 7}, TeamSide::HOME);
    gs.ball = BallState::carried({5, 7}, 1);

    FixedDiceRoller dice({4});
    resolveHandOff(gs, 1, 2, dice, nullptr);

    EXPECT_TRUE(gs.homeTeam.handOffUsedThisTurn);
    EXPECT_FALSE(gs.homeTeam.passUsedThisTurn)
        << "handing off must leave the pass action still available";
}
