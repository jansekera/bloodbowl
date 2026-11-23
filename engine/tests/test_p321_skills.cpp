// P321 (23.11.2026) — schopnosti ze soupisek, které engine neznal. rules_bb2016.txt:
//   Timmm-ber! ř. 8625-8636, Swoop ř. 8549-8564, Fan Favourite ř. 8107-8112. Parita s PHP
//   tests/Engine/{Timmmber,Swoop,FanFavourite}Test.php.
#include <gtest/gtest.h>
#include "bb/helpers.h"
#include "bb/kickoff_handler.h"
#include "bb/move_handler.h"
#include "bb/ttm_handler.h"
#include <vector>

using namespace bb;

namespace {

GameState emptyState() {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    gs.activeTeam = TeamSide::HOME;
    for (auto& p : gs.players) { p.state = PlayerState::OFF_PITCH; p.position = {-1, -1}; }
    return gs;
}

Player& put(GameState& gs, TeamSide side, int n, Position pos, int ma = 6, int st = 3, int ag = 3, int av = 8) {
    Player& p = gs.getPlayer(GameState::squadId(side, n));
    p.state = PlayerState::STANDING;
    p.position = pos;
    p.stats = {static_cast<int8_t>(ma), static_cast<int8_t>(st), static_cast<int8_t>(ag), static_cast<int8_t>(av)};
    p.movementRemaining = ma;
    return p;
}

// Ležící Treeman (MA2) se dvěma sousedními spoluhráči.
GameState treeman(bool timmmber) {
    auto gs = emptyState();
    Player& t = put(gs, TeamSide::HOME, 0, {10, 7}, 2, 6, 1, 10);
    t.state = PlayerState::PRONE;
    t.movementRemaining = 2;
    if (timmmber) t.skills.add(SkillName::Timmmber);
    put(gs, TeamSide::HOME, 1, {11, 7});
    put(gs, TeamSide::HOME, 2, {9, 7});
    return gs;
}

}  // namespace

// --- Timmm-ber! ---

TEST(P321Timmmber, TwoFreeHelpersLowerTheTargetToTwo) {
    auto gs = treeman(true);
    const int id = GameState::squadId(TeamSide::HOME, 0);
    EXPECT_EQ(standUpTarget(gs, gs.getPlayer(id)), 2);
    FixedDiceRoller dice({2});
    EXPECT_TRUE(resolveStandUp(gs, id, dice, nullptr).success);
    EXPECT_EQ(gs.getPlayer(id).state, PlayerState::STANDING);
}

TEST(P321Timmmber, WithoutTheSkillTheHelpersDoNothing) {   // pozitivní kontrola
    auto gs = treeman(false);
    EXPECT_EQ(standUpTarget(gs, gs.getPlayer(GameState::squadId(TeamSide::HOME, 0))), 4);
}

TEST(P321Timmmber, AOneAlwaysFails) {
    auto gs = treeman(true);
    gs.homeTeam.rerolls = 0;
    const int id = GameState::squadId(TeamSide::HOME, 0);
    FixedDiceRoller dice({1});
    EXPECT_FALSE(resolveStandUp(gs, id, dice, nullptr).success);
    EXPECT_EQ(gs.getPlayer(id).state, PlayerState::PRONE);
}

TEST(P321Timmmber, AHelperInAnEnemyTackleZoneOrDownDoesNotCount) {
    auto gs = treeman(true);
    put(gs, TeamSide::AWAY, 0, {12, 7});                                     // zóna na pomocníka (11,7)
    gs.getPlayer(GameState::squadId(TeamSide::HOME, 2)).state = PlayerState::PRONE;   // druhý leží
    EXPECT_EQ(standUpTarget(gs, gs.getPlayer(GameState::squadId(TeamSide::HOME, 0))), 4);
}

// --- Swoop ---

namespace {
GameState swoopThrow(bool swoop) {
    auto gs = emptyState();
    gs.homeTeam.rerolls = 0;
    gs.awayTeam.rerolls = 0;
    gs.ball = BallState::onGround({-1, -1});
    Player& t = put(gs, TeamSide::HOME, 0, {10, 7}, 4, 5, 1, 9);
    t.skills.add(SkillName::ThrowTeamMate);
    Player& g = put(gs, TeamSide::HOME, 1, {11, 7}, 6, 2, 3, 7);
    g.skills.add(SkillName::RightStuff);
    g.skills.add(SkillName::Stunty);
    if (swoop) g.skills.add(SkillName::Swoop);
    return gs;
}
}  // namespace

TEST(P321Swoop, ScattersWithTheThrowInTemplateAndLandsWithPlusOne) {
    // Přesnost: AG1, QP +1, TTM −1 ⇒ 6+, hod 6. Šablona z cíle (13,7): shoda ⇒ dopředu, D6 3 ⇒ (14,7);
    // z (14,7) nejblíž cíli dozadu, 3 ⇒ (13,7); znovu dopředu ⇒ (14,7). Přistání AG3 4+ − 1 = 3+, hod 3.
    auto gs = swoopThrow(true);
    FixedDiceRoller dice({6, 3, 3, 3, 3});
    resolveThrowTeamMate(gs, GameState::squadId(TeamSide::HOME, 0), GameState::squadId(TeamSide::HOME, 1),
                         {13, 7}, dice, nullptr);
    const Player& g = gs.getPlayer(GameState::squadId(TeamSide::HOME, 1));
    EXPECT_EQ(g.position, (Position{14, 7}));
    EXPECT_EQ(g.state, PlayerState::STANDING);
}

TEST(P321Swoop, WithoutSwoopALandingThreeFails) {   // pozitivní kontrola: D8 3/3/3 (východ) ⇒ (16,7), 3 < 4
    auto gs = swoopThrow(false);
    FixedDiceRoller dice({6, 3, 3, 3, 3, 1, 1});
    resolveThrowTeamMate(gs, GameState::squadId(TeamSide::HOME, 0), GameState::squadId(TeamSide::HOME, 1),
                         {13, 7}, dice, nullptr);
    EXPECT_EQ(gs.getPlayer(GameState::squadId(TeamSide::HOME, 1)).state, PlayerState::PRONE);
}

TEST(P321Swoop, WithStuntyTheTackleZoneOnTheTargetSquareCounts) {
    auto gs = emptyState();
    Player& s = put(gs, TeamSide::HOME, 0, {10, 7});
    s.skills.add(SkillName::Stunty);
    Player& w = put(gs, TeamSide::HOME, 1, {10, 3});
    w.skills.add(SkillName::Stunty);
    w.skills.add(SkillName::Swoop);
    put(gs, TeamSide::AWAY, 0, {12, 7});
    put(gs, TeamSide::AWAY, 1, {12, 3});
    EXPECT_EQ(calculateDodgeTarget(gs, w, {11, 3}, {10, 3}), calculateDodgeTarget(gs, s, {11, 7}, {10, 7}) + 1);
}

// --- Fan Favourite ---

namespace {
GameState fans(int oolGani) {
    auto gs = emptyState();
    put(gs, TeamSide::HOME, 0, {5, 5});
    put(gs, TeamSide::AWAY, 0, {20, 7});
    for (int i = 0; i < oolGani; ++i) put(gs, TeamSide::HOME, 1 + i, {5, 7 + i}).skills.add(SkillName::FanFavourite);
    return gs;
}
}  // namespace

TEST(P321FanFavourite, CheeringFansTieIsBrokenByFame) {
    auto gs = fans(1);
    const int home = gs.homeTeam.rerolls, away = gs.awayTeam.rerolls;
    FixedDiceRoller dice({5, 6});   // D3 obou 3; domácí + 1 FAME
    Position landing{13, 7};
    resolveKickoffEvent(gs, KickoffEvent::CHEERING, TeamSide::HOME, landing, dice, nullptr);
    EXPECT_EQ(gs.homeTeam.rerolls, home + 1);
    EXPECT_EQ(gs.awayTeam.rerolls, away);
}

TEST(P321FanFavourite, WithoutItCheeringFansTieGivesBoth) {   // pozitivní kontrola
    auto gs = fans(0);
    const int home = gs.homeTeam.rerolls, away = gs.awayTeam.rerolls;
    FixedDiceRoller dice({5, 6});
    Position landing{13, 7};
    resolveKickoffEvent(gs, KickoffEvent::CHEERING, TeamSide::HOME, landing, dice, nullptr);
    EXPECT_EQ(gs.homeTeam.rerolls, home + 1);
    EXPECT_EQ(gs.awayTeam.rerolls, away + 1);
}

TEST(P321FanFavourite, PitchInvasionUsesTheOtherTeamsFameAndNeverOnAOne) {
    // Domácí mají 2 Fan Favourite ⇒ jejich fanoušci házejí na HOSTA s +2. Pořadí hodů: domácí hráči (3), host.
    auto gs = fans(2);
    FixedDiceRoller dice({5, 5, 5, 4});
    Position landing{13, 7};
    resolveKickoffEvent(gs, KickoffEvent::PITCH_INVASION, TeamSide::HOME, landing, dice, nullptr);
    EXPECT_EQ(gs.getPlayer(GameState::squadId(TeamSide::HOME, 0)).state, PlayerState::STANDING);
    EXPECT_EQ(gs.getPlayer(GameState::squadId(TeamSide::AWAY, 0)).state, PlayerState::STUNNED);

    auto gs2 = fans(5);
    FixedDiceRoller dice2({5, 5, 5, 5, 5, 5, 1});
    resolveKickoffEvent(gs2, KickoffEvent::PITCH_INVASION, TeamSide::HOME, landing, dice2, nullptr);
    EXPECT_EQ(gs2.getPlayer(GameState::squadId(TeamSide::AWAY, 0)).state, PlayerState::STANDING);
}
