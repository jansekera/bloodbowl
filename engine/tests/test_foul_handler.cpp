#include <gtest/gtest.h>
#include <algorithm>
#include "bb/foul_handler.h"
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

TEST(FoulHandler, BasicFoulArmourNotBroken) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    gs.getPlayer(12).state = PlayerState::PRONE;
    // Armor: 3+3=6 ≤ 8, not broken. Not doubles.
    FixedDiceRoller dice({3, 4});
    auto result = resolveFoul(gs, 1, 12, dice, nullptr);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(gs.getPlayer(12).state, PlayerState::PRONE); // unchanged
    EXPECT_TRUE(gs.getPlayer(1).hasActed);
    EXPECT_TRUE(gs.homeTeam.foulUsedThisTurn);
}

TEST(FoulHandler, FoulBreaksArmour) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    gs.getPlayer(12).state = PlayerState::PRONE;
    // Armor: 5+4=9 > 8, broken. Injury: 3+3=6 → stunned
    FixedDiceRoller dice({5, 4, 3, 4});  // 3,4 (ne 3,3): dublet na ZRANĚNÍ nově vylučuje, BB2016 l. 1878
    auto result = resolveFoul(gs, 1, 12, dice, nullptr);
    EXPECT_EQ(gs.getPlayer(12).state, PlayerState::STUNNED);
}

TEST(FoulHandler, DirtyPlayerBonus) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::DirtyPlayer);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    gs.getPlayer(12).state = PlayerState::PRONE;
    // Armor: 4+4+1(DP)=9 > 8, broken. Injury: 3+3=6 → stunned
    FixedDiceRoller dice({4, 4, 3, 4});  // 3,4 (ne 3,3): dublet na ZRANĚNÍ nově vylučuje, BB2016 l. 1878
    auto result = resolveFoul(gs, 1, 12, dice, nullptr);
    EXPECT_EQ(gs.getPlayer(12).state, PlayerState::STUNNED);
}

TEST(FoulHandler, DoublesEjection) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    gs.getPlayer(12).state = PlayerState::PRONE;
    // Armor: 3+3=6 (doubles!), not broken. Fouler ejected.
    FixedDiceRoller dice({3, 3});
    auto result = resolveFoul(gs, 1, 12, dice, nullptr);
    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::EJECTED);
}

TEST(FoulHandler, SneakyGitPreventsEjection) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::SneakyGit);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    gs.getPlayer(12).state = PlayerState::PRONE;
    // Armor: 3+3=6 (doubles), not broken. SneakyGit prevents ejection.
    FixedDiceRoller dice({3, 3});
    auto result = resolveFoul(gs, 1, 12, dice, nullptr);
    EXPECT_NE(gs.getPlayer(1).state, PlayerState::EJECTED);
}

TEST(FoulHandler, FoulAssists) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME); // fouler
    placePlayer(gs, 2, {11, 6}, TeamSide::HOME); // assist (adj to target, no enemy TZ)
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    gs.getPlayer(12).state = PlayerState::PRONE;
    // +1 assist. Armor: 4+3+1=8 ≤ 8, not broken (need > AV)
    FixedDiceRoller dice({4, 3});
    auto result = resolveFoul(gs, 1, 12, dice, nullptr);
    EXPECT_EQ(gs.getPlayer(12).state, PlayerState::PRONE);
}

TEST(FoulHandler, FoulOnStunnedTarget) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    gs.getPlayer(12).state = PlayerState::STUNNED;
    // Valid target
    FixedDiceRoller dice({5, 4, 3, 4}); // broken + stunned  // 3,4 (ne 3,3): dublet na ZRANĚNÍ nově vylučuje, BB2016 l. 1878
    auto result = resolveFoul(gs, 1, 12, dice, nullptr);
    EXPECT_TRUE(result.success);
}

TEST(FoulHandler, FoulOnStandingFails) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    FixedDiceRoller dice({});
    auto result = resolveFoul(gs, 1, 12, dice, nullptr);
    EXPECT_FALSE(result.success);
}

TEST(FoulHandler, FoulEmitsInjuryEvent) {
    // 2026-07-24 (item 3.6): FOUL used to reimplement injury resolution
    // inline and never emit an INJURY event, unlike every other injury-
    // causing path (BLOCK, bomb, ball-and-chain). Now delegates to the
    // shared resolveInjuryRoll helper -- assert the event actually shows up.
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    gs.getPlayer(12).state = PlayerState::PRONE;
    // Armor: 5+4=9 > 8, broken. Injury: 3+4=7 → stunned (dřív 3+3=6; 3,3 je
    // nově dublet a vyloučilo by faulujícího, BB2016 l. 1878).
    FixedDiceRoller dice({5, 4, 3, 4});  // 3,4 (ne 3,3): dublet na ZRANĚNÍ nově vylučuje, BB2016 l. 1878
    std::vector<GameEvent> events;
    auto result = resolveFoul(gs, 1, 12, dice, &events);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(gs.getPlayer(12).state, PlayerState::STUNNED);
    auto it = std::find_if(events.begin(), events.end(), [](const GameEvent& e) {
        return e.type == GameEvent::Type::INJURY;
    });
    ASSERT_NE(it, events.end());
    EXPECT_EQ(it->playerId, 12);
    EXPECT_EQ(it->roll, 7);
    EXPECT_EQ(it->die1, 3);
    EXPECT_EQ(it->die2, 4);

    // FOUL and ARMOR_BREAK events also carry the individual armour dice.
    auto foulEvt = std::find_if(events.begin(), events.end(), [](const GameEvent& e) {
        return e.type == GameEvent::Type::FOUL;
    });
    ASSERT_NE(foulEvt, events.end());
    EXPECT_EQ(foulEvt->die1, 5);
    EXPECT_EQ(foulEvt->die2, 4);
}

TEST(FoulHandler, FoulDecayDoesNotAffectTheInjuryRoll) {
    // 2026-07-24 (item 3.6) wired hasDecay through resolveFoul; 2026-08-10
    // rules parity then established that Decay must not touch the Injury
    // roll at all (CRP: it doubles the CASUALTY roll after a Casualty
    // result). This keeps the delegation covered while asserting the
    // corrected contract.
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    gs.getPlayer(12).state = PlayerState::PRONE;
    gs.getPlayer(12).skills.add(SkillName::Decay);
    // Armour: 5+4=9 > 8, broken. Injury: 3+3=6 -> Stunned, and that stands;
    // the trailing 5,4 must not be consumed as a second injury roll.
    FixedDiceRoller dice({5, 4, 3, 4, 5, 4});  // 3,4 (ne 3,3): dublet na ZRANĚNÍ nově vylučuje, BB2016 l. 1878
    auto result = resolveFoul(gs, 1, 12, dice, nullptr);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(gs.getPlayer(12).state, PlayerState::STUNNED);
}

// BB2016 ř. 1878-1882: "if the Armour AND/OR Injury roll is a doubles (i.e.
// two 1s, or two 2s, etc), the referee has spotted the foul, and the player
// taking the Foul Action is sent off... In addition, his team suffers a
// turnover." Do 21.08. se koukalo JEN na armour kostky, protože hod na
// zranění dělá sdílená BLOKOVÁ funkce, která kostky nevracela.
TEST(FoulHandler, DoublesOnInjuryRollEjectsAndCausesTurnover) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    gs.getPlayer(12).state = PlayerState::PRONE;

    // armour 5+4 = 9 > AV 8 => prorazí; zranění 3+3 = DUBLET
    FixedDiceRoller dice({5, 4, 3, 3});
    auto result = resolveFoul(gs, 1, 12, dice, nullptr);

    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::EJECTED) << "dublet na zranění vylučuje";
    EXPECT_TRUE(result.turnover) << "a tým má turnover";
}

TEST(FoulHandler, GuardDoesNotAssistAFoul) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);      // faulující
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    gs.getPlayer(12).state = PlayerState::PRONE;      // oběť
    // náš asistent vedle oběti, ale V TZ stojícího soupeře => bez Guardu neasistuje
    placePlayer(gs, 2, {11, 8}, TeamSide::HOME);
    gs.getPlayer(2).skills.add(SkillName::Guard);
    placePlayer(gs, 13, {12, 8}, TeamSide::AWAY);     // markuje našeho asistenta

    // armour 4+4 = 8, AV 8 => neprorazí BEZ asistence; s Guardem by bylo 9 a prorazilo
    FixedDiceRoller dice({4, 4});
    resolveFoul(gs, 1, 12, dice, nullptr);
    EXPECT_EQ(gs.getPlayer(12).state, PlayerState::PRONE)
        << "BB2016 ř. 8160: Guard se u faulu použít NESMÍ";
}
