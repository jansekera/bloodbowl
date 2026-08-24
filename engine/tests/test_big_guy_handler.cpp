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

// Block/Blitz gets +2, so the check needs a 2+ -- but it IS rolled: a
// natural 1 fails even when hitting (a roll of 1 before modifiers always
// fails). The old code skipped the roll for Block/Blitz entirely.
TEST(BigGuyHandler, WildAnimalNaturalOneFailsEvenOnBlock) {
    auto gs = makeGameState();
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::WildAnimal);

    FixedDiceRoller dice({1});
    auto result = resolveBigGuyCheck(gs, 1, ActionType::BLOCK, dice, nullptr);

    EXPECT_TRUE(result.actionBlocked);
    EXPECT_FALSE(gs.getPlayer(1).lostTacklezones);  // keeps tackle zones
}

TEST(BigGuyHandler, WildAnimalPassesBlockOnTwo) {
    auto gs = makeGameState();
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::WildAnimal);

    FixedDiceRoller dice({2});  // 2 + 2 = 4 -> passes
    auto result = resolveBigGuyCheck(gs, 1, ActionType::BLITZ, dice, nullptr);

    EXPECT_FALSE(result.actionBlocked);
}

TEST(BigGuyHandler, WildAnimalFailMove) {
    auto gs = makeGameState();
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::WildAnimal);

    FixedDiceRoller dice({3});  // no bonus without Block/Blitz -> needs 4+
    auto result = resolveBigGuyCheck(gs, 1, ActionType::MOVE, dice, nullptr);

    EXPECT_TRUE(result.actionBlocked);
    // WildAnimal keeps tacklezones
    EXPECT_FALSE(gs.getPlayer(1).lostTacklezones);
    EXPECT_TRUE(gs.getPlayer(1).hasActed);
}

TEST(BigGuyHandler, WildAnimalPassesMoveOnFour) {
    auto gs = makeGameState();
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::WildAnimal);

    FixedDiceRoller dice({4});
    auto result = resolveBigGuyCheck(gs, 1, ActionType::MOVE, dice, nullptr);

    EXPECT_FALSE(result.actionBlocked);
}

// ===== TAKE ROOT TESTS =====

// ⛔ TA2 (24.08.2026): `TakeRootOnlyMove` drive CERTIFIKOVAL vadu -- prazdne
// kostky a komentar "Block: no roll needed" tvrdily, ze Treeman blokuje BEZ
// rizika. BB2016 l. 8572-8573: "Immediately after declaring AN ACTION with
// this player, roll a D6" -- tedy i na BLOCK. Nahrazeno testy hranice.

TEST(BigGuyHandler, TakeRootRollsOnEveryDeclaredAction) {
    for (ActionType at : {ActionType::MOVE, ActionType::BLOCK, ActionType::BLITZ,
                          ActionType::PASS, ActionType::HAND_OFF, ActionType::FOUL}) {
        auto gs = makeGameState();
        placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
        gs.getPlayer(1).skills.add(SkillName::TakeRoot);
        FixedDiceRoller dice({1});           // jedina kostka: hod se MUSI spotrebovat
        resolveBigGuyCheck(gs, 1, at, dice, nullptr);
        EXPECT_TRUE(gs.getPlayer(1).rooted)
            << "hod na Take Root se u akce " << static_cast<int>(at) << " vubec nehodil";
    }
}

TEST(BigGuyHandler, TakeRootFailedBlockActionMayStillBlock) {
    // l. 8580-8582: "The player may block adjacent players without
    // following-up as part of a Block Action."
    auto gs = makeGameState();
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::TakeRoot);
    FixedDiceRoller dice({1});
    auto result = resolveBigGuyCheck(gs, 1, ActionType::BLOCK, dice, nullptr);
    EXPECT_FALSE(result.actionBlocked);
    EXPECT_TRUE(gs.getPlayer(1).rooted);
    EXPECT_EQ(gs.getPlayer(1).movementRemaining, 0);   // l. 8574-8575: MA = 0
}

TEST(BigGuyHandler, TakeRootFailedBlitzMayNotBlock) {
    // l. 8582-8584: "if a player fails his Take Root roll as part of a Blitz
    // Action he may not block that turn"
    auto gs = makeGameState();
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::TakeRoot);
    FixedDiceRoller dice({1});
    auto result = resolveBigGuyCheck(gs, 1, ActionType::BLITZ, dice, nullptr);
    EXPECT_TRUE(result.actionBlocked);
    EXPECT_TRUE(gs.getPlayer(1).rooted);
}

TEST(BigGuyHandler, TakeRootFailMove) {
    auto gs = makeGameState();
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::TakeRoot);

    FixedDiceRoller dice({1});  // Fail on MOVE
    auto result = resolveBigGuyCheck(gs, 1, ActionType::MOVE, dice, nullptr);

    EXPECT_TRUE(result.actionBlocked);
}

TEST(BigGuyHandler, TakeRootPassesOnTwoPlus) {
    // druha strana hranice: 2+ a hraje se normalne
    auto gs = makeGameState();
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::TakeRoot);
    FixedDiceRoller dice({2});
    auto result = resolveBigGuyCheck(gs, 1, ActionType::MOVE, dice, nullptr);
    EXPECT_FALSE(result.actionBlocked);
    EXPECT_FALSE(gs.getPlayer(1).rooted);
}

TEST(BigGuyHandler, TakeRootPersistsAcrossTurns) {
    // l. 8574-8576: MA je 0 "until a drive ends, or he is Knocked Down or
    // Placed Prone" -- tedy NE jen na tu jednu akci.
    auto gs = makeGameState();
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::TakeRoot);
    gs.getPlayer(1).stats.movement = 2;
    FixedDiceRoller dice({1});
    resolveBigGuyCheck(gs, 1, ActionType::MOVE, dice, nullptr);
    ASSERT_TRUE(gs.getPlayer(1).rooted);

    gs.resetPlayersForNewTurn(TeamSide::HOME);
    EXPECT_TRUE(gs.getPlayer(1).rooted)   << "zakorenení skoncilo uz pristi kolo";
    EXPECT_EQ(gs.getPlayer(1).movementRemaining, 0);
}

TEST(BigGuyHandler, TakeRootEndsWhenKnockedDown) {
    auto gs = makeGameState();
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::TakeRoot);
    gs.getPlayer(1).stats.movement = 2;
    FixedDiceRoller dice({1});
    resolveBigGuyCheck(gs, 1, ActionType::MOVE, dice, nullptr);
    ASSERT_TRUE(gs.getPlayer(1).rooted);

    gs.getPlayer(1).state = PlayerState::PRONE;     // "or he is Knocked Down"
    gs.resetPlayersForNewTurn(TeamSide::HOME);
    EXPECT_FALSE(gs.getPlayer(1).rooted);
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

// ============================================================================
// TA8 / TA9 (24.08.2026): oba skilly mely UPLNE JINOU kostkovou mechaniku, nez
// pisi pravidla, a testy tu vadnou verzi certifikovaly vcetne vypoctu v
// komentari. Prepsano na text BB2016.
// ============================================================================

TEST(MoveHandler, TentaclesCaughtOnFiveOrLess) {
    // l. 8588-8591: "The opposing player rolls 2D6 ADDING THEIR OWN player's ST
    // and SUBTRACTING the Tentacles player's ST. If the final result is 5 OR
    // LESS, then the moving player is held firm."
    // Do 24.08. se hral protihod D6 vs D6 -- jine rozdeleni i jina citlivost.
    auto gs = makeGameState();
    gs.homeTeam.rerolls = 0;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME, 6, 3, 3, 8);   // ST3 utika
    placePlayer(gs, 12, {10, 8}, TeamSide::AWAY, 6, 5, 1, 9);  // ST5 chapadla
    gs.getPlayer(12).skills.add(SkillName::Tentacles);

    // 2D6 = 3+4 = 7; 7 + ST3 - ST5 = 5 => 5 nebo min => CHYCEN
    FixedDiceRoller dice({3, 4});
    auto result = resolveMoveStep(gs, 1, {11, 7}, dice, nullptr);

    EXPECT_TRUE(result.success);      // neni to turnover, jen konci pohyb
    EXPECT_FALSE(result.turnover);
    EXPECT_EQ(gs.getPlayer(1).position, (Position{10, 7}));
}

TEST(MoveHandler, TentaclesEscapedOnSixOrMore) {
    // druha strana hranice: tentyz rozdil sil, o jednicku vyssi hod
    auto gs = makeGameState();
    gs.homeTeam.rerolls = 0;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME, 6, 3, 3, 8);
    placePlayer(gs, 12, {10, 8}, TeamSide::AWAY, 6, 5, 1, 9);
    gs.getPlayer(12).skills.add(SkillName::Tentacles);

    // 2D6 = 4+4 = 8; 8 + 3 - 5 = 6 => unik. Pak dodge: AG3, cil 4, hod 5.
    FixedDiceRoller dice({4, 4, 5});
    auto result = resolveMoveStep(gs, 1, {11, 7}, dice, nullptr);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(gs.getPlayer(1).position, (Position{11, 7}));
}

// ===== SHADOWING TESTS =====

TEST(MoveHandler, ShadowingFollowsOnSevenOrLess) {
    // l. 8458-8464: "The opposing player rolls 2D6 ADDING THEIR OWN player's
    // movement allowance and SUBTRACTING the Shadowing player's movement
    // allowance. If the final result is 7 OR LESS, the player with Shadowing
    // may move into the square vacated." Do 24.08. tu byl JEDEN D6 a znamenka
    // OBRACENE, takze RYCHLY hráč se stinovani branil HUR misto lip.
    auto gs = makeGameState();
    gs.homeTeam.rerolls = 0;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME, 6, 3, 4, 8);   // MA6 utika
    placePlayer(gs, 12, {10, 8}, TeamSide::AWAY, 8, 3, 3, 8);  // MA8 stinuje
    gs.getPlayer(12).skills.add(SkillName::Shadowing);

    // dodge AG4 cil 3, hod 4 => uspech.
    // 2D6 = 4+3 = 7; 7 + MA6 - MA8 = 5 => 7 nebo min => NASLEDUJE
    FixedDiceRoller dice({4, 4, 3});
    auto result = resolveMoveStep(gs, 1, {11, 7}, dice, nullptr);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(gs.getPlayer(12).position, (Position{10, 7}));
}

TEST(MoveHandler, ShadowingFailsOnEightOrMore) {
    // ⭐ A tady je videt, ze znamenka mela byt obracene: utikajici je RYCHLEJSI
    // (MA8 proti MA6), takze se ma ubranit -- a s opravou se ubrani.
    auto gs = makeGameState();
    gs.homeTeam.rerolls = 0;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME, 8, 3, 4, 8);   // MA8 utika
    placePlayer(gs, 12, {10, 8}, TeamSide::AWAY, 6, 3, 3, 8);  // MA6 stinuje
    gs.getPlayer(12).skills.add(SkillName::Shadowing);

    // dodge AG4 cil 3, hod 4 => uspech.
    // 2D6 = 4+4 = 8; 8 + MA8 - MA6 = 10 => 8 nebo vic => NENASLEDUJE
    FixedDiceRoller dice({4, 4, 4});
    auto result = resolveMoveStep(gs, 1, {11, 7}, dice, nullptr);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(gs.getPlayer(12).position, (Position{10, 8}));
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
