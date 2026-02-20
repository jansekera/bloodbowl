#include <gtest/gtest.h>
#include "bb/big_guy_handler.h"
#include "bb/move_handler.h"
#include "bb/action_resolver.h"
#include "bb/helpers.h"

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

static GameState makeGameState() {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    gs.activeTeam = TeamSide::HOME;
    gs.homeTeam.side = TeamSide::HOME;
    gs.awayTeam.side = TeamSide::AWAY;
    gs.homeTeam.rerolls = 0;
    gs.homeTeam.turnNumber = 1;
    return gs;
}

// ===== BONEHEAD TESTS =====

TEST(BigGuyHandler, BoneHeadPass) {
    auto gs = makeGameState();
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::BoneHead);

    FixedDiceRoller dice({4});  // Pass on 2+
    auto result = resolveBigGuyCheck(gs, 1, ActionType::MOVE, dice, nullptr);

    EXPECT_FALSE(result.actionBlocked);
    EXPECT_TRUE(result.proceed);
    EXPECT_FALSE(gs.getPlayer(1).lostTacklezones);
}

TEST(BigGuyHandler, BoneHeadFail) {
    auto gs = makeGameState();
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::BoneHead);

    FixedDiceRoller dice({1});  // Fail
    auto result = resolveBigGuyCheck(gs, 1, ActionType::MOVE, dice, nullptr);

    EXPECT_TRUE(result.actionBlocked);
    EXPECT_FALSE(result.proceed);
    EXPECT_TRUE(gs.getPlayer(1).lostTacklezones);
    EXPECT_TRUE(gs.getPlayer(1).hasActed);
    EXPECT_TRUE(gs.getPlayer(1).hasMoved);
}

// ===== REALLY STUPID TESTS =====

TEST(BigGuyHandler, ReallyStupidWithAlly) {
    auto gs = makeGameState();
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::ReallyStupid);
    placePlayer(gs, 2, {11, 7}, TeamSide::HOME);  // adjacent ally

    FixedDiceRoller dice({2});  // Pass with ally (need 2+)
    auto result = resolveBigGuyCheck(gs, 1, ActionType::MOVE, dice, nullptr);

    EXPECT_FALSE(result.actionBlocked);
}

TEST(BigGuyHandler, ReallyStupidAlone) {
    auto gs = makeGameState();
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::ReallyStupid);
    // No adjacent allies

    FixedDiceRoller dice({3});  // Fail alone (need 4+)
    auto result = resolveBigGuyCheck(gs, 1, ActionType::MOVE, dice, nullptr);

    EXPECT_TRUE(result.actionBlocked);
    EXPECT_TRUE(gs.getPlayer(1).lostTacklezones);
}

TEST(BigGuyHandler, ReallyStupidAlonePass) {
    auto gs = makeGameState();
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::ReallyStupid);

    FixedDiceRoller dice({4});  // Pass alone (need 4+)
    auto result = resolveBigGuyCheck(gs, 1, ActionType::MOVE, dice, nullptr);

    EXPECT_FALSE(result.actionBlocked);
}

// ===== WILD ANIMAL TESTS =====

TEST(BigGuyHandler, WildAnimalAutoPassBlock) {
    auto gs = makeGameState();
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::WildAnimal);

    FixedDiceRoller dice({});  // No roll needed for Block
    auto result = resolveBigGuyCheck(gs, 1, ActionType::BLOCK, dice, nullptr);

    EXPECT_FALSE(result.actionBlocked);
}

TEST(BigGuyHandler, WildAnimalFailMove) {
    auto gs = makeGameState();
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::WildAnimal);

    FixedDiceRoller dice({2});  // Fail on MOVE (need 3+)
    auto result = resolveBigGuyCheck(gs, 1, ActionType::MOVE, dice, nullptr);

    EXPECT_TRUE(result.actionBlocked);
    // WildAnimal keeps tacklezones
    EXPECT_FALSE(gs.getPlayer(1).lostTacklezones);
    EXPECT_TRUE(gs.getPlayer(1).hasActed);
}

// ===== TAKE ROOT TESTS =====

TEST(BigGuyHandler, TakeRootOnlyMove) {
    auto gs = makeGameState();
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::TakeRoot);

    // Block: no roll needed
    FixedDiceRoller dice({});
    auto result = resolveBigGuyCheck(gs, 1, ActionType::BLOCK, dice, nullptr);
    EXPECT_FALSE(result.actionBlocked);
}

TEST(BigGuyHandler, TakeRootFailMove) {
    auto gs = makeGameState();
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::TakeRoot);

    FixedDiceRoller dice({1});  // Fail on MOVE
    auto result = resolveBigGuyCheck(gs, 1, ActionType::MOVE, dice, nullptr);

    EXPECT_TRUE(result.actionBlocked);
}

// ===== BLOODLUST TESTS =====

TEST(BigGuyHandler, BloodlustPass) {
    auto gs = makeGameState();
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::Bloodlust);

    FixedDiceRoller dice({2});  // Pass (2+)
    auto result = resolveBigGuyCheck(gs, 1, ActionType::MOVE, dice, nullptr);

    EXPECT_FALSE(result.actionBlocked);
}

TEST(BigGuyHandler, BloodlustBiteThrall) {
    auto gs = makeGameState();
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::Bloodlust);
    placePlayer(gs, 2, {11, 7}, TeamSide::HOME);  // Adjacent Thrall (no Bloodlust)

    FixedDiceRoller dice({1});  // Fail → bite
    auto result = resolveBigGuyCheck(gs, 1, ActionType::MOVE, dice, nullptr);

    EXPECT_FALSE(result.actionBlocked);  // Action proceeds
    EXPECT_TRUE(result.proceed);
    EXPECT_EQ(gs.getPlayer(2).state, PlayerState::KO);  // Thrall KO'd
}

TEST(BigGuyHandler, BloodlustNoThrall) {
    auto gs = makeGameState();
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::Bloodlust);
    // No adjacent Thrall

    FixedDiceRoller dice({1});
    auto result = resolveBigGuyCheck(gs, 1, ActionType::MOVE, dice, nullptr);

    EXPECT_TRUE(result.actionBlocked);
    EXPECT_FALSE(result.proceed);
    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::KO);
}

// ===== LEAP TESTS =====

TEST(MoveHandler, LeapSuccess) {
    auto gs = makeGameState();
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::Leap);

    // Leap to (12, 7) = dist 2. AG3: target = 7-3=4. Roll 5 → success
    FixedDiceRoller dice({5});
    auto result = resolveLeap(gs, 1, {12, 7}, dice, nullptr);

    EXPECT_TRUE(result.success);
    EXPECT_FALSE(result.turnover);
    EXPECT_EQ(gs.getPlayer(1).position, (Position{12, 7}));
}

TEST(MoveHandler, LeapFail) {
    auto gs = makeGameState();
    gs.homeTeam.rerolls = 0;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::Leap);

    // Leap AG3: target 4. Roll 2 → fail
    // Armor: 2D6=7 vs AV8 → no break
    FixedDiceRoller dice({2, 3, 4});
    auto result = resolveLeap(gs, 1, {12, 7}, dice, nullptr);

    EXPECT_TRUE(result.turnover);
    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::PRONE);
    EXPECT_EQ(gs.getPlayer(1).position, (Position{12, 7}));
}

TEST(MoveHandler, LeapWithVeryLongLegs) {
    auto gs = makeGameState();
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::Leap);
    gs.getPlayer(1).skills.add(SkillName::VeryLongLegs);

    // AG3 + VLL: target = 7-3-1=3. Roll 3 → success
    FixedDiceRoller dice({3});
    auto result = resolveLeap(gs, 1, {12, 7}, dice, nullptr);

    EXPECT_TRUE(result.success);
    EXPECT_FALSE(result.turnover);
}

TEST(MoveHandler, LeapIgnoresTZ) {
    auto gs = makeGameState();
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::Leap);
    // Enemy adjacent to start — no dodge needed for leap
    placePlayer(gs, 12, {10, 8}, TeamSide::AWAY);

    // Leap to (12,7), but TZ at (12,7)? No, enemy at (10,8) is 2 squares away from (12,7)
    // Target = 7-3 = 4. Roll 4 → success
    FixedDiceRoller dice({4});
    auto result = resolveLeap(gs, 1, {12, 7}, dice, nullptr);

    EXPECT_TRUE(result.success);
    EXPECT_FALSE(result.turnover);
}

// ===== TENTACLES TESTS =====

TEST(MoveHandler, TentaclesCaught) {
    auto gs = makeGameState();
    gs.homeTeam.rerolls = 0;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME, 6, 3, 3, 8);
    placePlayer(gs, 12, {10, 8}, TeamSide::AWAY, 6, 5, 1, 9);
    gs.getPlayer(12).skills.add(SkillName::Tentacles);

    // Tentacles check: mover D6 + ST3 vs tentacles D6 + ST5
    // Mover rolls 2 (2+3=5), tentacles rolls 1 (1+5=6), 5 < 6 → caught
    FixedDiceRoller dice({2, 1});
    auto result = resolveMoveStep(gs, 1, {11, 7}, dice, nullptr);

    EXPECT_TRUE(result.success);  // Not a turnover, movement just ends
    EXPECT_FALSE(result.turnover);
    EXPECT_EQ(gs.getPlayer(1).position, (Position{10, 7}));  // Stayed at original pos
}

TEST(MoveHandler, TentaclesEscape) {
    auto gs = makeGameState();
    gs.homeTeam.rerolls = 0;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME, 6, 4, 3, 8);
    placePlayer(gs, 12, {10, 8}, TeamSide::AWAY, 6, 3, 1, 9);
    gs.getPlayer(12).skills.add(SkillName::Tentacles);

    // Tentacles: mover rolls 4 (4+4=8) vs tentacles rolls 2 (2+3=5), 8 > 5 → escaped
    // Then dodge: AG3, 1 TZ at dest? No, enemy at (10,8) is adjacent to (10,7) not (11,7).
    // Actually from (10,7), dest (11,7): is enemy at (10,8) adjacent to (10,7)? Yes.
    // So needsDodge=true. Dodge target = 7-3 + TZ@(11,7) = 4 + 0 = 4. Roll 5 → success
    FixedDiceRoller dice({4, 2, 5});
    auto result = resolveMoveStep(gs, 1, {11, 7}, dice, nullptr);

    EXPECT_TRUE(result.success);
    EXPECT_FALSE(result.turnover);
    EXPECT_EQ(gs.getPlayer(1).position, (Position{11, 7}));
}

// ===== SHADOWING TESTS =====

TEST(MoveHandler, ShadowingFollow) {
    auto gs = makeGameState();
    gs.homeTeam.rerolls = 0;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME, 6, 3, 4, 8);  // MA6 AG4
    placePlayer(gs, 12, {10, 8}, TeamSide::AWAY, 8, 3, 3, 8);  // MA8 with Shadowing
    gs.getPlayer(12).skills.add(SkillName::Shadowing);

    // needsDodge = true (enemy at (10,8) has TZ on (10,7))
    // Dodge: AG4, target = 7-4 + TZ@(11,7)=0 = 3. Roll 4 → success
    // Shadowing: D6 + MA8 - MA6 = D6+2. If >= 6 → follows. Roll 4 → 4+2=6 → follows!
    FixedDiceRoller dice({4, 4});
    auto result = resolveMoveStep(gs, 1, {11, 7}, dice, nullptr);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(gs.getPlayer(12).position, (Position{10, 7}));  // Followed to vacated square
}

TEST(MoveHandler, ShadowingFail) {
    auto gs = makeGameState();
    gs.homeTeam.rerolls = 0;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME, 8, 3, 4, 8);  // MA8 AG4
    placePlayer(gs, 12, {10, 8}, TeamSide::AWAY, 6, 3, 3, 8);  // MA6 with Shadowing
    gs.getPlayer(12).skills.add(SkillName::Shadowing);

    // Dodge: AG4, target 3. Roll 4 → success
    // Shadowing: D6 + MA6 - MA8 = D6-2. Need 6 → roll must be 8+ → impossible
    FixedDiceRoller dice({4, 6});
    auto result = resolveMoveStep(gs, 1, {11, 7}, dice, nullptr);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(gs.getPlayer(12).position, (Position{10, 8}));  // Stayed put
}

// ===== BIG GUY INTEGRATION VIA ACTION RESOLVER =====

TEST(BigGuyHandler, BoneHeadBlockedViaResolver) {
    auto gs = makeGameState();
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::BoneHead);

    Action action{ActionType::MOVE, 1, -1, {11, 7}};

    // BoneHead roll: 1 (fail) → action blocked
    FixedDiceRoller dice({1});
    auto result = resolveAction(gs, action, dice, nullptr);

    EXPECT_TRUE(result.success);  // Not a turnover
    EXPECT_FALSE(result.turnover);
    EXPECT_TRUE(gs.getPlayer(1).hasActed);
    EXPECT_EQ(gs.getPlayer(1).position, (Position{10, 7}));  // Didn't move
}
