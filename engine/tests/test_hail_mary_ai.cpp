#include <gtest/gtest.h>
#include "bb/rules_engine.h"
#include "bb/macro_actions.h"
#include "bb/helpers.h"

using namespace bb;

// P266 (uživatel 9.11.2026: „přidej možnost hrát hail mary když je oponent daleko“, upřesnil „obojí“):
//   (a) bomba s Hail Mary Pass na soupeře mimo pravítko (ř. 7952-7954, 8164-8169);
//   (b) přihrávka s HMP spoluhráči mimo pravítko, když u cíle nejsou soupeři — míč se 3× rozptýlí,
//       takže soupeř do 3 polí od cíle by ho mohl sebrat.
// Ve vánici HMP ne (ř. 8170-8171) a ve vánici jen rychlá/krátká (ř. 1490-1494) — platí i pro bombu.

static void place(GameState& gs, int id, Position pos, TeamSide side) {
    Player& p = gs.getPlayer(id);
    p.state = PlayerState::STANDING;
    p.position = pos;
    p.stats = {6, 3, 3, 8};
    p.movementRemaining = 6;
}

static GameState base() {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    gs.activeTeam = TeamSide::HOME;
    gs.homeTeam.turnNumber = 3;
    gs.awayTeam.turnNumber = 3;
    return gs;
}

static bool hasAction(const GameState& gs, ActionType t, int playerId, Position target) {
    std::vector<Action> acts;
    getAvailableActions(gs, acts);
    for (const auto& a : acts)
        if (a.type == t && a.playerId == playerId && a.target.x == target.x && a.target.y == target.y) return true;
    return false;
}

// ───────────── (a) bomba ─────────────
static GameState bombState(bool hmp) {
    GameState gs = base();
    place(gs, 1, {3, 7}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::Bombardier);
    if (hmp) gs.getPlayer(1).skills.add(SkillName::HailMaryPass);
    place(gs, 12, {22, 7}, TeamSide::AWAY);   // 19 polí — mimo pravítko
    place(gs, 13, {14, 7}, TeamSide::AWAY);   // 11 polí — dlouhá bomba (pozitivní kontrola pravítka)
    return gs;
}

TEST(HailMaryAI, P266BombOfferedAtAFarOpponentWithHmp) {
    EXPECT_TRUE(hasAction(bombState(true), ActionType::BOMB_THROW, 1, {22, 7}));
}

TEST(HailMaryAI, P266NoFarBombWithoutHmp) {
    const GameState gs = bombState(false);
    EXPECT_FALSE(hasAction(gs, ActionType::BOMB_THROW, 1, {22, 7}));
    EXPECT_TRUE(hasAction(gs, ActionType::BOMB_THROW, 1, {14, 7}));   // pravítko dál platí
}

TEST(HailMaryAI, P266NoHmpBombAndNoLongBombInABlizzard) {
    GameState gs = bombState(true);
    gs.weather = Weather::BLIZZARD;
    EXPECT_FALSE(hasAction(gs, ActionType::BOMB_THROW, 1, {22, 7}));
    // dřív nabídky bomby počasí ignorovaly (resolver pak hod odmítl) — teď passTargetLegal
    EXPECT_FALSE(hasAction(gs, ActionType::BOMB_THROW, 1, {14, 7}));
}

// ───────────── (b) přihrávka ─────────────
static GameState passState(bool hmp) {
    GameState gs = base();
    place(gs, 1, {3, 7}, TeamSide::HOME);
    if (hmp) gs.getPlayer(1).skills.add(SkillName::HailMaryPass);
    place(gs, 2, {22, 7}, TeamSide::HOME);    // 19 polí vpřed — mimo pravítko
    place(gs, 12, {3, 1}, TeamSide::AWAY);    // soupeř daleko od cíle
    gs.ball.isHeld = true;
    gs.ball.carrierId = 1;
    gs.ball.position = {3, 7};
    return gs;
}

TEST(HailMaryAI, P266HmpPassOfferedWhenNoOpponentNearTheTarget) {
    EXPECT_TRUE(hasAction(passState(true), ActionType::PASS, 1, {22, 7}));
}

TEST(HailMaryAI, P266NoHmpPassWhenAnOpponentIsWithinThreeOfTheTarget) {
    GameState gs = passState(true);
    gs.getPlayer(12).position = {20, 9};      // 2 pole od cíle
    EXPECT_FALSE(hasAction(gs, ActionType::PASS, 1, {22, 7}));
}

TEST(HailMaryAI, P266ProneOpponentNearTheTargetDoesNotBlock) {
    GameState gs = passState(true);
    gs.getPlayer(12).position = {20, 9};
    gs.getPlayer(12).state = PlayerState::PRONE;   // ležící míč nesebere
    EXPECT_TRUE(hasAction(gs, ActionType::PASS, 1, {22, 7}));
}

TEST(HailMaryAI, P266NoHmpPassWithoutTheSkillOrInABlizzard) {
    EXPECT_FALSE(hasAction(passState(false), ActionType::PASS, 1, {22, 7}));
    GameState gs = passState(true);
    gs.weather = Weather::BLIZZARD;
    EXPECT_FALSE(hasAction(gs, ActionType::PASS, 1, {22, 7}));
}

static bool hasPassMacro(const GameState& gs, int carrierId, int targetId) {
    std::vector<Macro> ms;
    getAvailableMacros(gs, ms);
    for (const auto& m : ms)
        if (m.type == MacroType::PASS_ACTION && m.playerId == carrierId && m.targetId == targetId) return true;
    return false;
}

TEST(HailMaryAI, P266PassMacroOffersHmpToAFarTeammateInTheClear) {
    EXPECT_TRUE(hasPassMacro(passState(true), 1, 2));
    EXPECT_FALSE(hasPassMacro(passState(false), 1, 2));   // pozitivní kontrola: bez HMP nic
    GameState gs = passState(true);
    gs.getPlayer(12).position = {21, 5};
    EXPECT_FALSE(hasPassMacro(gs, 1, 2));
}

TEST(HailMaryAI, P266NoHmpPassBackwards) {
    GameState gs = passState(true);
    gs.getPlayer(1).position = {23, 7};
    gs.ball.position = {23, 7};
    gs.getPlayer(2).position = {3, 7};         // 20 polí ZA nosičem
    EXPECT_FALSE(hasPassMacro(gs, 1, 2));
}
