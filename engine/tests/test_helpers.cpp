#include <gtest/gtest.h>
#include "bb/helpers.h"

using namespace bb;

// Helper to set up a player on-pitch
static void placePlayer(GameState& gs, int id, Position pos, TeamSide side,
                         int ma = 6, int st = 3, int ag = 3, int av = 8) {
    Player& p = gs.getPlayer(id);
    p.state = PlayerState::STANDING;
    p.position = pos;
    p.stats = {static_cast<int8_t>(ma), static_cast<int8_t>(st),
               static_cast<int8_t>(ag), static_cast<int8_t>(av)};
    p.movementRemaining = ma;
}

TEST(Helpers, CountTacklezonesNone) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    // No enemies nearby
    EXPECT_EQ(countTacklezones(gs, {10, 7}, TeamSide::HOME), 0);
}

TEST(Helpers, CountTacklezonesOneEnemy) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);

    EXPECT_EQ(countTacklezones(gs, {10, 7}, TeamSide::HOME), 1);
}

TEST(Helpers, CountTacklezonesTwoEnemies) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    placePlayer(gs, 13, {10, 8}, TeamSide::AWAY);

    EXPECT_EQ(countTacklezones(gs, {10, 7}, TeamSide::HOME), 2);
}

TEST(Helpers, CountTacklezonesProneNotCounted) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    gs.getPlayer(12).state = PlayerState::PRONE;

    EXPECT_EQ(countTacklezones(gs, {10, 7}, TeamSide::HOME), 0);
}

TEST(Helpers, CountTacklezonesLostTZNotCounted) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    gs.getPlayer(12).lostTacklezones = true;

    EXPECT_EQ(countTacklezones(gs, {10, 7}, TeamSide::HOME), 0);
}

TEST(Helpers, DodgeTargetBasicAG3) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    // CRP: AG3 needs 4+ from the table, +1 for making a dodge => 3+
    EXPECT_EQ(calculateDodgeTarget(gs, gs.getPlayer(1), {11, 7}, {10, 7}), 3);
}

TEST(Helpers, DodgeTargetAG4) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME, 6, 3, 4, 8);
    // AG4: 3+ from the table, +1 for the dodge => 2+
    EXPECT_EQ(calculateDodgeTarget(gs, gs.getPlayer(1), {11, 7}, {10, 7}), 2);
}

TEST(Helpers, DodgeTargetWithTZAtDest) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    placePlayer(gs, 12, {12, 7}, TeamSide::AWAY); // adjacent to dest (11,7)

    // AG3 4+, +1 dodge, -1 per TZ on the destination => 4+
    EXPECT_EQ(calculateDodgeTarget(gs, gs.getPlayer(1), {11, 7}, {10, 7}), 4);
}

TEST(Helpers, DodgeTargetWithDodgeSkill) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::Dodge);

    // AG3 4+, +1 dodge, -1 Dodge skill => 2+
    EXPECT_EQ(calculateDodgeTarget(gs, gs.getPlayer(1), {11, 7}, {10, 7}), 2);
}

TEST(Helpers, DodgeTargetDodgeNegatedByTackle) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::Dodge);
    placePlayer(gs, 12, {9, 7}, TeamSide::AWAY);
    gs.getPlayer(12).skills.add(SkillName::Tackle);

    // Tackle at source negates Dodge: AG3 4+, +1 dodge => 3+
    EXPECT_EQ(calculateDodgeTarget(gs, gs.getPlayer(1), {11, 7}, {10, 7}), 3);
}

TEST(Helpers, DodgeTargetStuntyAndTwoHeads) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::Stunty);
    gs.getPlayer(1).skills.add(SkillName::TwoHeads);

    // 7-3-1-1 = 2
    EXPECT_EQ(calculateDodgeTarget(gs, gs.getPlayer(1), {11, 7}, {10, 7}), 2);
}

TEST(Helpers, DodgeTargetBreakTackle) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME, 6, 4, 3, 8);
    gs.getPlayer(1).skills.add(SkillName::BreakTackle);

    // Uses ST4 instead of AG3: 7-4 = 3, +1 for the dodge => 2+
    EXPECT_EQ(calculateDodgeTarget(gs, gs.getPlayer(1), {11, 7}, {10, 7}), 2);
}

TEST(Helpers, DodgeTargetPrehensileTail) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    placePlayer(gs, 12, {9, 7}, TeamSide::AWAY);
    gs.getPlayer(12).skills.add(SkillName::PrehensileTail);

    // AG3 4+, +1 dodge, +1 from PrehensileTail at source => 4+
    EXPECT_EQ(calculateDodgeTarget(gs, gs.getPlayer(1), {11, 7}, {10, 7}), 4);
}

TEST(Helpers, DodgeTargetClamped) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME, 6, 3, 5, 8);
    gs.getPlayer(1).skills.add(SkillName::Dodge);
    gs.getPlayer(1).skills.add(SkillName::Stunty);
    gs.getPlayer(1).skills.add(SkillName::TwoHeads);

    // 7-5-1-1-1 = -1, clamped to 2
    EXPECT_EQ(calculateDodgeTarget(gs, gs.getPlayer(1), {11, 7}, {10, 7}), 2);
}

TEST(Helpers, PickupTargetBasic) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    // AG3, 0 TZ: 6-3 = 3
    EXPECT_EQ(calculatePickupTarget(gs, gs.getPlayer(1)), 3);
}

TEST(Helpers, PickupTargetWithTZ) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    // 6-3+1 = 4
    EXPECT_EQ(calculatePickupTarget(gs, gs.getPlayer(1)), 4);
}

TEST(Helpers, PickupTargetBigHandIgnoresTZ) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    gs.getPlayer(1).skills.add(SkillName::BigHand);
    // BigHand ignores TZ and weather
    EXPECT_EQ(calculatePickupTarget(gs, gs.getPlayer(1)), 3);
}

TEST(Helpers, PickupTargetPouringRain) {
    GameState gs;
    gs.weather = Weather::POURING_RAIN;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    // 6-3+1 = 4
    EXPECT_EQ(calculatePickupTarget(gs, gs.getPlayer(1)), 4);
}

TEST(Helpers, CatchTargetBasic) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    // AG3, no TZ, no modifier: 7-3 = 4
    EXPECT_EQ(calculateCatchTarget(gs, gs.getPlayer(1), 0), 4);
}

TEST(Helpers, CatchTargetExtraArms) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::ExtraArms);
    // 7-3-1 = 3
    EXPECT_EQ(calculateCatchTarget(gs, gs.getPlayer(1), 0), 3);
}

TEST(Helpers, CatchTargetNervesOfSteel) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    gs.getPlayer(1).skills.add(SkillName::NervesOfSteel);
    // NOS ignores TZ
    EXPECT_EQ(calculateCatchTarget(gs, gs.getPlayer(1), 0), 4);
}

TEST(Helpers, AssistsBasic) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);  // blocker
    placePlayer(gs, 2, {10, 6}, TeamSide::HOME);  // assist — adjacent to target
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);  // target

    // CRP: assisting player must not be in TZ "except the player being blocked"
    // Player 2 at (10,6) is in TZ of player 12 at (11,7), but 12 is excluded → can assist
    EXPECT_EQ(countAssists(gs, {11, 7}, TeamSide::HOME, 1, 12, 12), 1);
}

TEST(Helpers, AssistsInEnemyTZCannotAssist) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    placePlayer(gs, 2, {10, 6}, TeamSide::HOME);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    placePlayer(gs, 13, {11, 6}, TeamSide::AWAY);  // marks player 2

    // Player 2 is in TZ of player 13 (not excluded) → cannot assist
    EXPECT_EQ(countAssists(gs, {11, 7}, TeamSide::HOME, 1, 12, 12), 0);
}

TEST(Helpers, AssistsGuardInEnemyTZ) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    placePlayer(gs, 2, {10, 6}, TeamSide::HOME);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    placePlayer(gs, 13, {11, 6}, TeamSide::AWAY);
    gs.getPlayer(2).skills.add(SkillName::Guard);

    // Guard allows assist even in enemy TZ
    EXPECT_EQ(countAssists(gs, {11, 7}, TeamSide::HOME, 1, 12, 12), 1);
}

TEST(Helpers, BlockDiceInfoEqual) {
    auto info = getBlockDiceInfo(3, 3);
    EXPECT_EQ(info.count, 1);
    EXPECT_TRUE(info.attackerChooses);
}

TEST(Helpers, BlockDiceInfoAttackerStronger) {
    auto info = getBlockDiceInfo(4, 3);
    EXPECT_EQ(info.count, 2);
    EXPECT_TRUE(info.attackerChooses);
}

TEST(Helpers, BlockDiceInfoAttackerMuchStronger) {
    auto info = getBlockDiceInfo(7, 3);
    EXPECT_EQ(info.count, 3);
    EXPECT_TRUE(info.attackerChooses);
}

TEST(Helpers, BlockDiceInfoDefenderStronger) {
    auto info = getBlockDiceInfo(3, 4);
    EXPECT_EQ(info.count, 2);
    EXPECT_FALSE(info.attackerChooses);
}

TEST(Helpers, BlockDiceInfoDefenderMuchStronger) {
    auto info = getBlockDiceInfo(3, 7);
    EXPECT_EQ(info.count, 3);
    EXPECT_FALSE(info.attackerChooses);
}

TEST(Helpers, PushbackSquaresEast) {
    Position out[3];
    int count = getPushbackSquares({10, 7}, {11, 7}, out);
    EXPECT_EQ(count, 3);
    // Straight east, NE, SE
    EXPECT_EQ(out[0], (Position{12, 7}));
    EXPECT_EQ(out[1], (Position{12, 8})); // CW from east = SE
    EXPECT_EQ(out[2], (Position{12, 6})); // CCW from east = NE
}

TEST(Helpers, PushbackSquaresAtEdge) {
    Position out[3];
    // Defender at right edge
    int count = getPushbackSquares({24, 7}, {25, 7}, out);
    // All pushback squares would be x=26, off pitch
    EXPECT_EQ(count, 0);
}

TEST(Helpers, ScatterDirections) {
    EXPECT_EQ(scatterDirection(1), (Position{0, -1}));  // N
    EXPECT_EQ(scatterDirection(3), (Position{1, 0}));   // E
    EXPECT_EQ(scatterDirection(5), (Position{0, 1}));   // S
    EXPECT_EQ(scatterDirection(7), (Position{-1, 0}));  // W
}

TEST(Helpers, AttemptRollSuccess) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    FixedDiceRoller dice({4}); // roll 4, target 4 → success
    bool ok = attemptRoll(gs, 1, dice, 4, SkillName::SKILL_COUNT, false, false, nullptr);
    EXPECT_TRUE(ok);
}

TEST(Helpers, AttemptRollFailNoReroll) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    FixedDiceRoller dice({3}); // roll 3, target 4 → fail, no reroll available
    bool ok = attemptRoll(gs, 1, dice, 4, SkillName::SKILL_COUNT, false, false, nullptr);
    EXPECT_FALSE(ok);
}

TEST(Helpers, AttemptRollSkillReroll) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::Dodge);
    FixedDiceRoller dice({2, 5}); // first roll 2 (fail), skill reroll 5 (success)
    bool ok = attemptRoll(gs, 1, dice, 4, SkillName::Dodge, false, false, nullptr);
    EXPECT_TRUE(ok);
}

TEST(Helpers, AttemptRollSkillNegated) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::Dodge);
    FixedDiceRoller dice({2}); // fail, skill negated
    bool ok = attemptRoll(gs, 1, dice, 4, SkillName::Dodge, true, false, nullptr);
    EXPECT_FALSE(ok);
}

TEST(Helpers, AttemptRollProReroll) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::Pro);
    // roll 2 (fail), no skill, Pro check: 4 (pass), reroll: 5 (success)
    FixedDiceRoller dice({2, 4, 5});
    bool ok = attemptRoll(gs, 1, dice, 4, SkillName::SKILL_COUNT, false, false, nullptr);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(gs.getPlayer(1).proUsedThisTurn);
}

TEST(Helpers, AttemptRollProFails) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::Pro);
    // roll 2 (fail), Pro check: 3 (fail) → no reroll
    FixedDiceRoller dice({2, 3});
    bool ok = attemptRoll(gs, 1, dice, 4, SkillName::SKILL_COUNT, false, false, nullptr);
    EXPECT_FALSE(ok);
}

TEST(Helpers, AttemptRollTeamReroll) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.homeTeam.rerolls = 1;
    // roll 2 (fail), team reroll: 5 (success)
    FixedDiceRoller dice({2, 5});
    bool ok = attemptRoll(gs, 1, dice, 4, SkillName::SKILL_COUNT, false, true, nullptr);
    EXPECT_TRUE(ok);
    EXPECT_EQ(gs.homeTeam.rerolls, 0);
    EXPECT_TRUE(gs.homeTeam.rerollUsedThisTurn);
}

TEST(Helpers, AttemptRollLonerGate) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::Loner);
    gs.homeTeam.rerolls = 1;
    // roll 2 (fail), Loner gate: 3 (fail) → reroll wasted
    FixedDiceRoller dice({2, 3});
    bool ok = attemptRoll(gs, 1, dice, 4, SkillName::SKILL_COUNT, false, true, nullptr);
    EXPECT_FALSE(ok);
    EXPECT_EQ(gs.homeTeam.rerolls, 0); // reroll consumed
}

// ⛔ TA1 (24.08.2026): tenhle test dřív CERTIFIKOVAL vadu -- kostkami
// {2,1,4,2,5} projel řetěz Dodge reroll -> Pro -> týmový reroll, tj. TŘI
// rerolly JEDNOHO hodu, a asertoval EXPECT_TRUE. BB2016 l. 925-927:
// "VERY IMPORTANT: No matter how many re-rolls you have, or what type they
// are, you may never re-roll a single dice roll more than once."
// Nahrazeno testy hranice -- na obou stranách.

TEST(Helpers, AttemptRollOnlyOneRerollPerDiceRoll) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::Dodge);
    gs.getPlayer(1).skills.add(SkillName::Pro);
    gs.homeTeam.rerolls = 1;
    // BB2016 l. 925-927: hod 2 (fail) -> Dodge reroll 1 (fail) a KONEC.
    // Kostky 4/2/5 zůstanou nespotřebované -- kdyby kód pokračoval na Pro
    // a týmový reroll, vyšlo by mu success a test spadne.
    FixedDiceRoller dice({2, 1, 4, 2, 5});
    bool ok = attemptRoll(gs, 1, dice, 4, SkillName::Dodge, false, true, nullptr);
    EXPECT_FALSE(ok);
    EXPECT_EQ(gs.homeTeam.rerolls, 1);              // týmový reroll se NESMÍ sáhnout
    EXPECT_FALSE(gs.homeTeam.rerollUsedThisTurn);
    EXPECT_FALSE(gs.getPlayer(1).proUsedThisTurn);  // ani Pro se nezkouší
}

TEST(Helpers, AttemptRollSkillRerollSucceedsStops) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::Dodge);
    gs.homeTeam.rerolls = 1;
    // druhá strana hranice: hod 2 (fail) -> Dodge reroll 5 (success)
    FixedDiceRoller dice({2, 5});
    bool ok = attemptRoll(gs, 1, dice, 4, SkillName::Dodge, false, true, nullptr);
    EXPECT_TRUE(ok);
    EXPECT_EQ(gs.homeTeam.rerolls, 1);
}

TEST(Helpers, AttemptRollTeamRerollOnlyWhenNoSkillReroll) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::Dodge);
    gs.getPlayer(1).dodgeRerollUsedThisTurn = true;  // Dodge už tohle kolo padl (l. 8089-8090)
    gs.homeTeam.rerolls = 1;
    // skill reroll není k dispozici => týmový reroll je PRVNÍ reroll toho hodu, a smí
    FixedDiceRoller dice({2, 5});
    bool ok = attemptRoll(gs, 1, dice, 4, SkillName::Dodge, false, true, nullptr);
    EXPECT_TRUE(ok);
    EXPECT_EQ(gs.homeTeam.rerolls, 0);
}

TEST(Helpers, AttemptRollProGateFailStopsOriginal) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::Pro);
    gs.homeTeam.rerolls = 0;   // žádný týmový reroll na přehození Pro hodu
    // BB2016 l. 8386-8389: Pro brána 1-3 => "the original result stands and may
    // not be re-rolled with a skill or team re-roll".
    FixedDiceRoller dice({2, 3, 5});
    bool ok = attemptRoll(gs, 1, dice, 4, SkillName::SKILL_COUNT, false, true, nullptr);
    EXPECT_FALSE(ok);
    EXPECT_TRUE(gs.getPlayer(1).proUsedThisTurn);
}

TEST(Helpers, AttemptRollTeamRerollRerollsTheProRollItself) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::Pro);
    gs.homeTeam.rerolls = 1;
    // l. 8388-8389: "however you can re-roll the Pro roll with a Team re-roll"
    // hod 2 (fail) -> Pro brána 3 (fail) -> týmový reroll BRÁNY: 5 (pass)
    // -> Pro reroll původního hodu: 4 (success)
    FixedDiceRoller dice({2, 3, 5, 4});
    bool ok = attemptRoll(gs, 1, dice, 4, SkillName::SKILL_COUNT, false, true, nullptr);
    EXPECT_TRUE(ok);
    EXPECT_EQ(gs.homeTeam.rerolls, 0);
}
