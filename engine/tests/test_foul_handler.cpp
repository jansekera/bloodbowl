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
    FixedDiceRoller dice({5, 4, 3, 3});
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
    FixedDiceRoller dice({4, 4, 3, 3});
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
    FixedDiceRoller dice({5, 4, 3, 3}); // broken + stunned
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
    // Armor: 5+4=9 > 8, broken. Injury: 3+3=6 → stunned.
    FixedDiceRoller dice({5, 4, 3, 3});
    std::vector<GameEvent> events;
    auto result = resolveFoul(gs, 1, 12, dice, &events);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(gs.getPlayer(12).state, PlayerState::STUNNED);
    auto it = std::find_if(events.begin(), events.end(), [](const GameEvent& e) {
        return e.type == GameEvent::Type::INJURY;
    });
    ASSERT_NE(it, events.end());
    EXPECT_EQ(it->playerId, 12);
    EXPECT_EQ(it->roll, 6);
    EXPECT_EQ(it->die1, 3);
    EXPECT_EQ(it->die2, 3);

    // FOUL and ARMOR_BREAK events also carry the individual armour dice.
    auto foulEvt = std::find_if(events.begin(), events.end(), [](const GameEvent& e) {
        return e.type == GameEvent::Type::FOUL;
    });
    ASSERT_NE(foulEvt, events.end());
    EXPECT_EQ(foulEvt->die1, 5);
    EXPECT_EQ(foulEvt->die2, 4);
}

TEST(FoulHandler, FoulDecayTakesWorseRoll) {
    // 2026-07-24 (item 3.6): the InjuryContext built for FOUL always set
    // hasDecay from the target's skills, but the old inline reimplementation
    // never passed ctx anywhere -- Decay was silently inert on FOUL-caused
    // injuries. Mirrors Injury.DecayTakesWorseRoll (test_injury.cpp) but
    // through resolveFoul, to confirm the shared-helper delegation actually
    // wires it up.
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    gs.getPlayer(12).state = PlayerState::PRONE;
    gs.getPlayer(12).skills.add(SkillName::Decay);
    // Armor: 5+4=9 > 8, broken. Injury roll 1: 3+3=6 (stunned).
    // Decay roll 2: 5+4=9 (KO). Takes worse: 9 → KO.
    FixedDiceRoller dice({5, 4, 3, 3, 5, 4});
    auto result = resolveFoul(gs, 1, 12, dice, nullptr);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(gs.getPlayer(12).state, PlayerState::KO);
}
