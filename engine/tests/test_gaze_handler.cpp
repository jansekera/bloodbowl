#include <gtest/gtest.h>
#include "bb/gaze_handler.h"
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

// ============================================================================
// TA5 (24.08.2026): puvodni testy certifikovaly dve vady naraz -- cil se
// pocital jako min(6, 2 + TZ) UPLNE BEZ AG (pro AG4 upira to nahodou vychazelo
// spravne, pro AG3 uz ne), zapocitaval se i tacklezone OBETI, kterou pravidlo
// vyslovne vyjima, a neuspech byl turnover. BB2016 l. 8181-8189.
// ============================================================================

TEST(GazeHandler, TargetIsAnAgilityRollNotAFlatTwo) {
    // l. 8181-8182: "Make an AGILITY ROLL for the player with hypnotic gaze".
    // AG3 => cil 4. Obet je jediny soused, a jeji TZ se NEPOCITA (l. 8183-8184
    // "other than the victim's"), takze zadny modifikator.
    GameState gs;
    gs.phase = GamePhase::PLAY;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME, 6, 3, 3, 8);   // AG3
    gs.getPlayer(1).skills.add(SkillName::HypnoticGaze);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);

    FixedDiceRoller dice({3});   // 3 < 4 => neuspech (pri stare mechanice cil 3 => uspech)
    auto result = resolveHypnoticGaze(gs, 1, 12, dice, nullptr);
    EXPECT_FALSE(gs.getPlayer(12).lostTacklezones);

    GameState gs2;
    gs2.phase = GamePhase::PLAY;
    placePlayer(gs2, 1, {10, 7}, TeamSide::HOME, 6, 3, 3, 8);
    gs2.getPlayer(1).skills.add(SkillName::HypnoticGaze);
    placePlayer(gs2, 12, {11, 7}, TeamSide::AWAY);
    FixedDiceRoller dice2({4});  // druha strana hranice
    resolveHypnoticGaze(gs2, 1, 12, dice2, nullptr);
    EXPECT_TRUE(gs2.getPlayer(12).lostTacklezones);
}

TEST(GazeHandler, FailureHasNoEffectAndIsNotATurnover) {
    // l. 8188-8189: "If the roll fails, then the hypnotic gaze HAS NO EFFECT."
    // Katalog turnoveru (l. 366-382) gaze vubec nezna.
    GameState gs;
    gs.phase = GamePhase::PLAY;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME, 6, 3, 3, 8);
    gs.getPlayer(1).skills.add(SkillName::HypnoticGaze);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);

    FixedDiceRoller dice({2});
    auto result = resolveHypnoticGaze(gs, 1, 12, dice, nullptr);
    EXPECT_FALSE(result.turnover);
    EXPECT_FALSE(gs.getPlayer(12).lostTacklezones);
    EXPECT_TRUE(gs.getPlayer(1).hasActed);   // akce se spotrebuje
}

TEST(GazeHandler, TheVICTIMSTacklezoneIsNotCountedButOthersAre) {
    // l. 8183-8184: "-1 for each opposing tackle zone on the player with
    // hypnotic gaze OTHER THAN THE VICTIM'S". Tri soupeři vedle, z toho jeden
    // je obet => modifikator jen -2. AG3 => cil 4 + 2 = 6.
    GameState gs;
    gs.phase = GamePhase::PLAY;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME, 6, 3, 3, 8);
    gs.getPlayer(1).skills.add(SkillName::HypnoticGaze);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);   // obet -- NEPOCITA se
    placePlayer(gs, 13, {10, 6}, TeamSide::AWAY);
    placePlayer(gs, 14, {10, 8}, TeamSide::AWAY);

    FixedDiceRoller dice({5});   // 5 < 6 => neuspech
    resolveHypnoticGaze(gs, 1, 12, dice, nullptr);
    EXPECT_FALSE(gs.getPlayer(12).lostTacklezones);

    GameState gs2;
    gs2.phase = GamePhase::PLAY;
    placePlayer(gs2, 1, {10, 7}, TeamSide::HOME, 6, 3, 3, 8);
    gs2.getPlayer(1).skills.add(SkillName::HypnoticGaze);
    placePlayer(gs2, 12, {11, 7}, TeamSide::AWAY);
    placePlayer(gs2, 13, {10, 6}, TeamSide::AWAY);
    placePlayer(gs2, 14, {10, 8}, TeamSide::AWAY);
    FixedDiceRoller dice2({6});
    resolveHypnoticGaze(gs2, 1, 12, dice2, nullptr);
    EXPECT_TRUE(gs2.getPlayer(12).lostTacklezones);
}

TEST(GazeHandler, GazerActed) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::HypnoticGaze);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);

    // Success
    FixedDiceRoller dice({6});
    resolveHypnoticGaze(gs, 1, 12, dice, nullptr);
    EXPECT_TRUE(gs.getPlayer(1).hasActed);
}
