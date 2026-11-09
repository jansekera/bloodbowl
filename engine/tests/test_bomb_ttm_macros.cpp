#include <gtest/gtest.h>
#include "bb/rules_engine.h"
#include "bb/macro_actions.h"
#include "bb/ttm_handler.h"
#include "bb/helpers.h"
#include "bb/dice.h"

using namespace bb;

// P268 (uživatel 9.11.2026: „bomba i hod spoluhráčem“): hlavní AI (MCTS na makroakcích) neměla makro pro bombu ani
// pro hod spoluhráčem — v kouři s goblini TV1500 (P267) 0 bomb a 0 hodů v 6 hrách.
//  - BOMB: bombař (nepohnul se, ř. 7949-7955) hodí na pole, kde výbuch srazí soupeře a nás ne
//    (cílové pole vždy, sousední na 4+, ř. 7968-7972; sražený náš = turnover, ř. 7956-7958).
//  - THROW_TEAM_MATE: Troll/Ripper hodí sousedního nosiče míče s Right Stuff aspoň 4 sloupce vpřed (jen rychlá/
//    krátká, ř. 8608-8609; vždy 3 rozptyly; po přistání smí nosič ještě táhnout — Right Stuff).
// Vedlejší vady téhož principu: nabídky TTM po mřížce do vzdálenosti 13 (i dlouhé, které resolver odmítne)
// a resolver TTM spotřeboval akci házeče i týmovou přihrávku DŘÍV, než zjistil, že cíl je mimo dosah.

static void place(GameState& gs, int id, Position pos, TeamSide side, std::initializer_list<SkillName> skills = {},
                  PlayerStats st = {6, 3, 3, 8}) {
    Player& p = gs.getPlayer(id);
    p.state = PlayerState::STANDING;
    p.position = pos;
    p.stats = st;
    p.movementRemaining = st.movement;
    for (SkillName s : skills) p.skills.add(s);
}

static GameState base() {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    gs.activeTeam = TeamSide::HOME;
    gs.homeTeam.turnNumber = 3;
    gs.awayTeam.turnNumber = 3;
    gs.homeTeam.rerolls = 0;
    return gs;
}

static std::vector<Macro> macros(const GameState& gs) {
    std::vector<Macro> ms;
    getAvailableMacros(gs, ms);
    return ms;
}

static const Macro* find(const std::vector<Macro>& ms, MacroType t, int playerId) {
    for (const auto& m : ms)
        if (m.type == t && m.playerId == playerId) return &m;
    return nullptr;
}

// ───────────── BOMB ─────────────
static GameState bombState() {
    GameState gs = base();
    place(gs, 1, {5, 7}, TeamSide::HOME, {SkillName::Bombardier});
    place(gs, 12, {10, 7}, TeamSide::AWAY);
    place(gs, 13, {10, 8}, TeamSide::AWAY);
    gs.ball.isHeld = false;
    gs.ball.position = {2, 2};
    return gs;
}

TEST(BombTtmMacros, P268BombMacroOfferedAtAnOpponent) {
    const auto ms = macros(bombState());
    const Macro* m = find(ms, MacroType::BOMB, 1);
    ASSERT_NE(m, nullptr);
    // cíl na soupeři (shluk 12+13 ⇒ jeden z nich ve středu, druhý vedle)
    EXPECT_TRUE((m->targetPos == Position{10, 7}) || (m->targetPos == Position{10, 8}));
}

TEST(BombTtmMacros, P268NoBombWhereOurPlayerWouldBeHit) {
    GameState gs = bombState();
    gs.getPlayer(13).position = {20, 1};
    place(gs, 2, {11, 7}, TeamSide::HOME);   // náš vedle jediného cíle
    EXPECT_EQ(find(macros(gs), MacroType::BOMB, 1), nullptr);
}

TEST(BombTtmMacros, P268NoBombAfterTheBombardierMoved) {
    GameState gs = bombState();
    gs.getPlayer(1).hasMoved = true;           // ř. 7953-7955: nesmí se pohnout před hodem
    EXPECT_EQ(find(macros(gs), MacroType::BOMB, 1), nullptr);
}

TEST(BombTtmMacros, P268BombMacroExpandsToABombThrow) {
    GameState gs = bombState();
    const Macro* m = find(macros(gs), MacroType::BOMB, 1);
    ASSERT_NE(m, nullptr);
    const Macro mc = *m;
    FixedDiceRoller dice(std::vector<int>(40, 6));   // přesný hod, sousedé 4+ zasaženi, zbroj neprorazí? 6+6 > 8
    auto r = greedyExpandMacro(gs, mc, dice);
    ASSERT_FALSE(r.actions.empty());
    EXPECT_EQ(r.actions.front().type, ActionType::BOMB_THROW);
    EXPECT_TRUE(gs.getPlayer(1).hasActed);
    EXPECT_NE(gs.getPlayer(12).state, PlayerState::STANDING);
}

// ───────────── THROW_TEAM_MATE ─────────────
static GameState ttmState() {
    GameState gs = base();
    place(gs, 1, {5, 7}, TeamSide::HOME, {SkillName::ThrowTeamMate}, {4, 5, 1, 9});   // Troll
    place(gs, 2, {6, 7}, TeamSide::HOME, {SkillName::RightStuff, SkillName::Stunty}, {6, 2, 3, 7});
    place(gs, 12, {3, 1}, TeamSide::AWAY);
    gs.ball.isHeld = true;
    gs.ball.carrierId = 2;
    gs.ball.position = {6, 7};
    return gs;
}

TEST(BombTtmMacros, P268TtmMacroThrowsTheCarrierForward) {
    const auto ms = macros(ttmState());
    const Macro* m = find(ms, MacroType::THROW_TEAM_MATE, 1);
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->targetId, 2);
    // aspoň 4 sloupce vpřed (3 rozptyly; po přistání goblin smí táhnout dál), v dosahu rychlá/krátká
    EXPECT_GE(m->targetPos.x, 6 + 4);
    PassRange band;
    ASSERT_TRUE(passRangeFromOffset(m->targetPos.x - 5, m->targetPos.y - 7, band));
    EXPECT_TRUE(band == PassRange::QUICK_PASS || band == PassRange::SHORT_PASS);
}

TEST(BombTtmMacros, P268NoTtmWithoutRightStuffOrAfterThePassAction) {
    GameState gs = ttmState();
    gs.getPlayer(2).skills = SkillSet{};
    EXPECT_EQ(find(macros(gs), MacroType::THROW_TEAM_MATE, 1), nullptr);
    GameState gs2 = ttmState();
    gs2.homeTeam.passUsedThisTurn = true;         // TTM je přihrávková akce týmu
    EXPECT_EQ(find(macros(gs2), MacroType::THROW_TEAM_MATE, 1), nullptr);
}

TEST(BombTtmMacros, P268TtmMacroExpandsToAThrow) {
    GameState gs = ttmState();
    const Macro* m = find(macros(gs), MacroType::THROW_TEAM_MATE, 1);
    ASSERT_NE(m, nullptr);
    const Macro mc = *m;
    FixedDiceRoller dice(std::vector<int>(40, 6));
    auto r = greedyExpandMacro(gs, mc, dice);
    ASSERT_FALSE(r.actions.empty());
    EXPECT_EQ(r.actions.front().type, ActionType::THROW_TEAM_MATE);
    EXPECT_TRUE(gs.homeTeam.passUsedThisTurn);
}

// ───────────── vedlejší vady (týž princip) ─────────────
TEST(BombTtmMacros, P268TtmOffersOnlyQuickOrShort) {
    std::vector<Action> acts;
    getAvailableActions(ttmState(), acts);
    int n = 0;
    for (const auto& a : acts) {
        if (a.type != ActionType::THROW_TEAM_MATE) continue;
        ++n;
        PassRange band;
        ASSERT_TRUE(passRangeFromOffset(a.target.x - 5, a.target.y - 7, band));
        EXPECT_TRUE(band == PassRange::QUICK_PASS || band == PassRange::SHORT_PASS) << a.target.x << "," << a.target.y;
    }
    EXPECT_GT(n, 0);   // pozitivní kontrola
}

TEST(BombTtmMacros, P268IllegalTtmTargetDoesNotSpendTheAction) {
    GameState gs = ttmState();
    FixedDiceRoller dice({});
    auto r = resolveThrowTeamMate(gs, 1, 2, {17, 7}, dice, nullptr);   // 12 polí = dlouhá
    EXPECT_FALSE(r.success);
    EXPECT_FALSE(gs.getPlayer(1).hasActed);
    EXPECT_FALSE(gs.homeTeam.passUsedThisTurn);
}
