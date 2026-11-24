// P310 (uživatel 24.11.2026: „1 vše, 2 ano, 3 ano“ + „piling on dává našeho hráče na zem - cíl pro faul“) —
// Piling On, rules_bb2016.txt ř. 8361-8371:
//   "The player may use this skill after they have made a block as part a Block or Blitz action, but only if they
//   are currently standing adjacent to the victim and the victim was Knocked Down. You can use a team re-roll to
//   re-roll the Armour roll or Injury roll. Then the Piling On player is Placed Prone in their own square - no
//   Armour roll is made for them, and this does not cause a turnover unless the Piling On player is carrying the
//   ball. ... If a player with the Loner skill wishes to use Piling On, roll a D6. On a 1-3, the team re-roll is
//   spent, but they remain standing and cannot re-roll the Armour or Injury roll."
// Dřív C++ Piling On neznal vůbec. Parita s PHP tests/Engine/PilingOnTest.php.
#include <gtest/gtest.h>
#include "bb/block_handler.h"
#include "bb/coach_choice.h"
#include "bb/piling_on.h"
#include <vector>

using namespace bb;

namespace {

void place(GameState& gs, int id, Position pos, int av = 8) {
    Player& p = gs.getPlayer(id);
    p.state = PlayerState::STANDING;
    p.position = pos;
    p.stats = {6, 3, 3, static_cast<int8_t>(av)};
    p.movementRemaining = 6;
}

// Útočník 1 (Block + Piling On) vedle obránce 12; jeden týmový přehoz; míč drží soupeř daleko (AI smí přehoz utratit).
GameState pair(int rerolls = 1) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    gs.activeTeam = TeamSide::HOME;
    place(gs, 1, {10, 7});
    gs.getPlayer(1).skills.add(SkillName::Block);
    gs.getPlayer(1).skills.add(SkillName::PilingOn);
    place(gs, 12, {11, 7});
    place(gs, 13, {20, 2});
    gs.ball = BallState::carried({20, 2}, 13);
    gs.homeTeam.rerolls = rerolls;
    return gs;
}

ActionResult block(GameState& gs, DiceRollerBase& dice) {
    return resolveBlock(gs, BlockParams{1, 12, false, false}, dice, nullptr);
}

}  // namespace

// ── kouč-člověk (otázka 3: „ano“) ──

// Both Down, útočník s Blockem stojí vedle sraženého; brnění 1+1 vydrželo ⇒ otázka „přehodit brnění“ (roll 1).
TEST(P310PilingOn, CoachAsked_ArmourHeld) {
    GameState gs = pair();
    FixedDiceRoller dice({2, 1, 1});
    CoachChoiceScope scope(TeamSide::HOME, {CoachAnswer::NONE});   // kostky bloku nepřehazovat
    try { block(gs, dice); FAIL(); } catch (const CoachChoicePending& p) {
        EXPECT_EQ(p.question, CoachQuestion::USE_PILING_ON);
        EXPECT_EQ(p.playerId, 1);
        EXPECT_EQ(p.otherId, 12);
        EXPECT_EQ(p.roll, 1);
    }
}

// Ano: přehoz brnění 6+6 prolomí, zranění 4+4 = KO; přehoz utracen, útočník leží, bez turnoveru.
TEST(P310PilingOn, CoachYes_ArmourRerolled_AttackerPlacedProne_NoTurnover) {
    GameState gs = pair();
    FixedDiceRoller dice({2, 1, 1, 6, 6, 4, 4});
    CoachChoiceScope scope(TeamSide::HOME, {CoachAnswer::NONE, CoachAnswer::TEAM});
    auto r = block(gs, dice);
    EXPECT_FALSE(r.turnover);
    EXPECT_EQ(gs.getPlayer(12).state, PlayerState::KO);
    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::PRONE);
    EXPECT_EQ(gs.homeTeam.rerolls, 0);
    EXPECT_TRUE(gs.homeTeam.rerollUsedThisTurn);
    EXPECT_EQ(dice.remaining(), 0u);
}

TEST(P310PilingOn, CoachNo_NothingRerolled_AttackerStands) {
    GameState gs = pair();
    FixedDiceRoller dice({2, 1, 1});
    CoachChoiceScope scope(TeamSide::HOME, {CoachAnswer::NONE, CoachAnswer::NONE});   // kostky; Piling On ne
    block(gs, dice);
    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::STANDING);
    EXPECT_EQ(gs.getPlayer(12).state, PlayerState::PRONE);
    EXPECT_EQ(gs.homeTeam.rerolls, 1);
}

// Brnění padlo (6+6), zranění 1+1 jen Stunned ⇒ otázka „přehodit zranění“ (roll 2); ano ⇒ 4+4 = KO.
TEST(P310PilingOn, CoachYes_StunnedInjuryRerolled) {
    GameState gs = pair();
    {
        GameState q = pair();
        FixedDiceRoller d({2, 6, 6, 1, 1});
        CoachChoiceScope scope(TeamSide::HOME, {CoachAnswer::NONE});   // kostky bloku nepřehazovat
        try { block(q, d); FAIL(); } catch (const CoachChoicePending& p) {
            EXPECT_EQ(p.question, CoachQuestion::USE_PILING_ON);
            EXPECT_EQ(p.roll, 2);
        }
    }
    FixedDiceRoller dice({2, 6, 6, 1, 1, 4, 4});
    CoachChoiceScope scope(TeamSide::HOME, {CoachAnswer::NONE, CoachAnswer::TEAM});
    block(gs, dice);
    EXPECT_EQ(gs.getPlayer(12).state, PlayerState::KO);
    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::PRONE);
}

// „re-roll the Armour roll OR Injury roll“: po přehozeném brnění se Stunned už znovu nepřehazuje (bez otázky).
TEST(P310PilingOn, OnlyOneReroll_ArmourThenStunnedNotAskedAgain) {
    GameState gs = pair(2);
    FixedDiceRoller dice({2, 1, 1, 6, 6, 1, 1});
    CoachChoiceScope scope(TeamSide::HOME, {CoachAnswer::NONE, CoachAnswer::TEAM});
    block(gs, dice);
    EXPECT_EQ(gs.getPlayer(12).state, PlayerState::STUNNED);
    EXPECT_EQ(gs.homeTeam.rerolls, 1);
}

// Bez týmového přehozu, nebo když už byl v tahu použit, se Piling On použít nedá — kouče se ani neptá.
TEST(P310PilingOn, NoTeamRerollLeft_NotOffered) {
    GameState gs = pair(0);
    FixedDiceRoller dice({2, 1, 1});
    CoachChoiceScope scope(TeamSide::HOME, {CoachAnswer::NONE});   // kostky bloku nepřehazovat
    EXPECT_NO_THROW(block(gs, dice));
    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::STANDING);
}

TEST(P310PilingOn, RerollUsedThisTurn_NotOffered) {
    GameState gs = pair(2);
    gs.homeTeam.rerollUsedThisTurn = true;
    FixedDiceRoller dice({2, 1, 1});
    CoachChoiceScope scope(TeamSide::HOME, {CoachAnswer::NONE});   // kostky bloku nepřehazovat
    EXPECT_NO_THROW(block(gs, dice));
    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::STANDING);
}

// „only if they are currently standing adjacent to the victim“: Defender Down bez follow-upu ⇒ obránce o pole dál.
TEST(P310PilingOn, NotAdjacentWithoutFollowUp_NotOffered) {
    GameState gs = pair();
    FixedDiceRoller dice({6, 1, 1});
    CoachChoiceScope scope(TeamSide::HOME, {CoachAnswer::NONE, CoachAnswer::NONE});   // kostky, follow-up ne
    EXPECT_NO_THROW(block(gs, dice));
    EXPECT_EQ(gs.getPlayer(12).position.x, 12);
    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::STANDING);
}

// Loner 1-3: „the team re-roll is spent, but they remain standing and cannot re-roll“.
TEST(P310PilingOn, LonerFails_RerollSpent_AttackerStands_NoReroll) {
    GameState gs = pair();
    gs.getPlayer(1).skills.add(SkillName::Loner);
    FixedDiceRoller dice({2, 1, 1, 3});
    CoachChoiceScope scope(TeamSide::HOME, {CoachAnswer::NONE, CoachAnswer::TEAM});
    block(gs, dice);
    EXPECT_EQ(gs.homeTeam.rerolls, 0);
    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::STANDING);
    EXPECT_EQ(gs.getPlayer(12).state, PlayerState::PRONE);
    EXPECT_EQ(dice.remaining(), 0u);
}

// „does not cause a turnover unless the Piling On player is carrying the ball“.
TEST(P310PilingOn, CarrierPilesOn_Turnover_BallLoose) {
    GameState gs = pair();
    gs.ball = BallState::carried({10, 7}, 1);
    FixedDiceRoller dice({2, 1, 1, 1, 1, 1, 1, 1, 1});   // brnění drží, přehoz drží, odraz
    CoachChoiceScope scope(TeamSide::HOME, {CoachAnswer::NONE, CoachAnswer::TEAM});
    auto r = block(gs, dice);
    EXPECT_TRUE(r.turnover);
    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::PRONE);
    EXPECT_FALSE(gs.ball.isHeld && gs.ball.carrierId == 1);
}

// ── AI ──

// (b) Stunned: zisk ~42 % > riziko faulu ⇒ přehodí zranění (míč drží soupeř).
TEST(P310PilingOn, Ai_StunnedInjury_Rerolls) {
    GameState gs = pair(2);   // po Piling On zbude přehoz (uživatel 24.11. (2))
    FixedDiceRoller dice({2, 6, 6, 1, 1, 4, 4});
    block(gs, dice);
    EXPECT_EQ(gs.getPlayer(12).state, PlayerState::KO);
    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::PRONE);
}

// Otázka 2: dokud náš nosič nedohrál, přehoz se šetří.
TEST(P310PilingOn, Ai_OurCarrierStillToAct_KeepsReroll) {
    GameState gs = pair(2);   // po Piling On zbude přehoz (uživatel 24.11. (2))
    place(gs, 2, {3, 3});
    gs.ball = BallState::carried({3, 3}, 2);
    FixedDiceRoller dice({2, 6, 6, 1, 1});
    block(gs, dice);
    EXPECT_EQ(gs.getPlayer(12).state, PlayerState::STUNNED);
    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::STANDING);
    EXPECT_EQ(gs.homeTeam.rerolls, 2);
}

TEST(P310PilingOn, Ai_OurCarrierDone_UsesReroll) {
    GameState gs = pair(2);   // po Piling On zbude přehoz (uživatel 24.11. (2))
    place(gs, 2, {3, 3});
    gs.getPlayer(2).hasActed = true;
    gs.ball = BallState::carried({3, 3}, 2);
    FixedDiceRoller dice({2, 6, 6, 1, 1, 4, 4});
    block(gs, dice);
    EXPECT_EQ(gs.getPlayer(12).state, PlayerState::KO);
}

TEST(P310PilingOn, Ai_LooseBall_KeepsReroll) {
    GameState gs = pair(2);   // po Piling On zbude přehoz (uživatel 24.11. (2))
    gs.ball = BallState::onGround({3, 3});
    FixedDiceRoller dice({2, 6, 6, 1, 1});
    block(gs, dice);
    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::STANDING);
}

// (a) brnění vydrželo: AV9 útočník bez soupeřů vedle ⇒ přehodí (zisk 0,116 > faul 0,069) ...
TEST(P310PilingOn, Ai_ArmourHeld_NoFoulThreat_Rerolls) {
    GameState gs = pair(2);   // po Piling On zbude přehoz (uživatel 24.11. (2))
    gs.getPlayer(1).stats.armour = 9;
    FixedDiceRoller dice({2, 1, 1, 6, 6, 4, 4});
    block(gs, dice);
    EXPECT_EQ(gs.getPlayer(12).state, PlayerState::KO);
    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::PRONE);
}

// ... se dvěma stojícími soupeři vedle (asistence k faulu, 0,174) ne — „cíl pro faul“.
TEST(P310PilingOn, Ai_ArmourHeld_TwoFoulAssists_Declines) {
    GameState gs = pair(2);   // po Piling On zbude přehoz (uživatel 24.11. (2))
    gs.getPlayer(1).stats.armour = 9;
    place(gs, 14, {9, 6});
    place(gs, 15, {9, 8});
    FixedDiceRoller dice({2, 2, 1, 1});   // dva asistující soupeři ⇒ dvě kostky bloku (obě Both Down)
    block(gs, dice);
    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::STANDING);
    EXPECT_EQ(gs.homeTeam.rerolls, 2);
}

// Uživatel 24.11. (2): „zvlášť na tah s možností dát TD šetřit aspoň jeden reroll“ — s posledním přehozem ne ...
TEST(P310PilingOn, Ai_LastRerollKeptForATouchdownTurn) {
    GameState gs = pair(1);
    gs.homeTeam.turnNumber = 5;
    FixedDiceRoller dice({2, 6, 6, 1, 1});
    block(gs, dice);
    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::STANDING);
    EXPECT_EQ(gs.homeTeam.rerolls, 1);
}

// ... v našem posledním tahu poločasu ano (přehozy se o poločase obnovují).
TEST(P310PilingOn, Ai_LastTurnOfHalf_UsesLastReroll) {
    GameState gs = pair(1);
    gs.homeTeam.turnNumber = 8;
    FixedDiceRoller dice({2, 6, 6, 1, 1, 4, 4});
    block(gs, dice);
    EXPECT_EQ(gs.getPlayer(12).state, PlayerState::KO);
    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::PRONE);
}

// „2 ano“ (neotevřít cestu k míči): útočník vedle soupeřova nosiče ho kryje — ležící by ho pustil.
TEST(P310PilingOn, Ai_NextToTheCarrier_StaysUp) {
    GameState gs = pair(2);
    place(gs, 13, {9, 7});
    gs.ball = BallState::carried({9, 7}, 13);
    FixedDiceRoller dice({2, 2, 6, 6, 1, 1});   // nosič vedle útočníka asistuje obránci ⇒ dvě kostky
    block(gs, dice);
    EXPECT_EQ(gs.getPlayer(12).state, PlayerState::STUNNED);
    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::STANDING);
}

// Pravidlo AI čte čísla: Stunty sražený 7 = KO (ř. 8534-8536), Thick Skull 8 = Stunned (ř. 8595-8598).
TEST(P310PilingOn, InjuryOutChance_StuntyAndThickSkull) {
    Player p;
    EXPECT_NEAR(injuryOutChance(p, 0), 15.0 / 36, 1e-9);
    p.skills.add(SkillName::Stunty);
    EXPECT_NEAR(injuryOutChance(p, 0), 21.0 / 36, 1e-9);
    Player t;
    t.skills.add(SkillName::ThickSkull);
    EXPECT_NEAR(injuryOutChance(t, 0), 10.0 / 36, 1e-9);
}
