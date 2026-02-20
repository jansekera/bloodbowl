#include <gtest/gtest.h>
#include "bb/bomb_handler.h"
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

TEST(BombHandler, AccurateExplosion) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    gs.activeTeam = TeamSide::HOME;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME); // Bombardier AG3
    gs.getPlayer(1).skills.add(SkillName::Bombardier);
    placePlayer(gs, 12, {13, 7}, TeamSide::AWAY); // Target

    // Distance 3 = quick pass. AG3: target = 7-3-1 = 3. Roll 5 >= 3 → accurate
    // Explosion: player 12 at (13,7) is in 3x3 around (13,7) → knocked down
    // Armor: 3+3=6 ≤ 8 not broken
    FixedDiceRoller dice({5, 3, 3});
    auto result = resolveBombThrow(gs, 1, {13, 7}, dice, nullptr);
    EXPECT_TRUE(result.success);
    EXPECT_FALSE(result.turnover); // Never turnover
    EXPECT_EQ(gs.getPlayer(12).state, PlayerState::PRONE);
}

TEST(BombHandler, InaccurateTripleScatter) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    gs.activeTeam = TeamSide::HOME;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME, 6, 3, 2, 8); // AG2
    gs.getPlayer(1).skills.add(SkillName::Bombardier);
    placePlayer(gs, 12, {13, 7}, TeamSide::AWAY);

    // AG2: target = 7-2-1 = 4. Roll 2 < 4 → inaccurate
    // 3x scatter from target (13,7): D8=1,1,1 → each (1,0) → (16,7)
    // No player at explosion → no effect
    FixedDiceRoller dice({2, 1, 1, 1});
    auto result = resolveBombThrow(gs, 1, {13, 7}, dice, nullptr);
    EXPECT_TRUE(result.success);
    EXPECT_FALSE(result.turnover);
    EXPECT_EQ(gs.getPlayer(12).state, PlayerState::STANDING); // Not hit
}

TEST(BombHandler, Fumble) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    gs.activeTeam = TeamSide::HOME;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::Bombardier);
    placePlayer(gs, 12, {13, 7}, TeamSide::AWAY);

    // Natural 1 = fumble. Scatter from thrower (10,7): D8=1 → (11,7)
    // No player at (11,7) → no explosion effect
    FixedDiceRoller dice({1, 1});
    auto result = resolveBombThrow(gs, 1, {13, 7}, dice, nullptr);
    EXPECT_FALSE(result.turnover); // Never turnover
}

TEST(BombHandler, OffPitchFizzle) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    gs.activeTeam = TeamSide::HOME;
    placePlayer(gs, 1, {1, 7}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::Bombardier);

    // Fumble (roll=1), scatter D8=5 → (-1,0) → (0,7) still on pitch
    // Actually let's make it scatter off pitch: thrower at (0,7)
    gs.getPlayer(1).position = {0, 7};
    // Fumble, D8=5 → (-1,0) → (-1,7) off pitch → fizzle
    FixedDiceRoller dice({1, 5});
    auto result = resolveBombThrow(gs, 1, {5, 7}, dice, nullptr);
    EXPECT_FALSE(result.turnover);
}

TEST(BombHandler, ThrowerImmune) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    gs.activeTeam = TeamSide::HOME;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::Bombardier);

    // Fumble (roll=1), scatter D8=5 → (-1,0) → (9,7)
    // Explosion at (9,7). Thrower at (10,7) is adjacent → in 3x3 but IMMUNE
    FixedDiceRoller dice({1, 5});
    auto result = resolveBombThrow(gs, 1, {13, 7}, dice, nullptr);
    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::STANDING); // Thrower immune
}

TEST(BombHandler, NeverTurnover) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    gs.activeTeam = TeamSide::HOME;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::Bombardier);
    gs.ball = BallState::carried({10, 7}, 1);

    // Even if fumble with ball carrier, never turnover
    // Fumble: D8=1 → (11,7). No players there.
    FixedDiceRoller dice({1, 1});
    auto result = resolveBombThrow(gs, 1, {13, 7}, dice, nullptr);
    EXPECT_FALSE(result.turnover);
}
