#include <gtest/gtest.h>
#include "bb/block_handler.h"
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

TEST(BlockHandler, TwoDiceAdvantage) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME, 6, 4, 3, 8); // ST4
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY); // ST3
    // ST4 vs ST3 = 2 dice attacker. Roll: DD, AD → choose DD
    // DD: push + knockdown. Armor roll on defender: 3+3=6 ≤ 8
    FixedDiceRoller dice({6, 1, 3, 3}); // DD, AD, armor d1, armor d2
    BlockParams params{1, 12, false, false};
    auto result = resolveBlock(gs, params, dice, nullptr);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(gs.getPlayer(12).state, PlayerState::PRONE);
}

TEST(BlockHandler, BlockSkillSavesOnBD) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::Block);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    // 1 die (equal ST). Roll BD (D6=2 → BD)
    // Attacker has Block, defender doesn't → only defender falls
    // Need armor roll for defender: 3+3=6 ≤ 8
    FixedDiceRoller dice({2, 3, 3});
    BlockParams params{1, 12, false, false};
    auto result = resolveBlock(gs, params, dice, nullptr);
    EXPECT_TRUE(result.success);
    EXPECT_FALSE(result.turnover);
    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::STANDING);
    EXPECT_EQ(gs.getPlayer(12).state, PlayerState::PRONE);
}

TEST(BlockHandler, BothDownNoBlockSkills) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    // Roll BD. No Block skill → both fall → turnover
    // Armor on attacker: 3+3=6. Armor on defender: 3+3=6
    FixedDiceRoller dice({2, 3, 3, 3, 3});
    BlockParams params{1, 12, false, false};
    auto result = resolveBlock(gs, params, dice, nullptr);
    EXPECT_TRUE(result.turnover);
    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::PRONE);
}

TEST(BlockHandler, WrestleBothProne) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    gs.getPlayer(12).skills.add(SkillName::Wrestle);
    // Roll BD. Defender has Wrestle → both prone, no armor, no turnover
    FixedDiceRoller dice({2});
    BlockParams params{1, 12, false, false};
    auto result = resolveBlock(gs, params, dice, nullptr);
    EXPECT_FALSE(result.turnover); // Wrestle = no turnover
    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::PRONE);
    EXPECT_EQ(gs.getPlayer(12).state, PlayerState::PRONE);
}

TEST(BlockHandler, DodgeSavesOnDS) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    gs.getPlayer(12).skills.add(SkillName::Dodge);
    // Roll DS (D6=5). Defender has Dodge → treated as PUSHED, not knocked down
    FixedDiceRoller dice({5});
    BlockParams params{1, 12, false, false};
    auto result = resolveBlock(gs, params, dice, nullptr);
    EXPECT_TRUE(result.success);
    // Defender pushed but still standing
    EXPECT_EQ(gs.getPlayer(12).state, PlayerState::STANDING);
}

TEST(BlockHandler, TackleNegatesDodgeOnDS) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::Tackle);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    gs.getPlayer(12).skills.add(SkillName::Dodge);
    // Roll DS. Tackle negates Dodge → defender knocked down
    // Armor: 3+3=6 ≤ 8
    FixedDiceRoller dice({5, 3, 3});
    BlockParams params{1, 12, false, false};
    auto result = resolveBlock(gs, params, dice, nullptr);
    EXPECT_EQ(gs.getPlayer(12).state, PlayerState::PRONE);
}

TEST(BlockHandler, PushbackBasic) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    // Roll PUSHED (D6=3). Pushed east → (12,7)
    FixedDiceRoller dice({3});
    BlockParams params{1, 12, false, false};
    auto result = resolveBlock(gs, params, dice, nullptr);
    EXPECT_EQ(gs.getPlayer(12).position, (Position{12, 7}));
    EXPECT_EQ(gs.getPlayer(12).state, PlayerState::STANDING);
    // Attacker follows up
    EXPECT_EQ(gs.getPlayer(1).position, (Position{11, 7}));
}

TEST(BlockHandler, PushOntoLooseBallBounces) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    gs.ball = BallState::onGround({12, 7});
    // Roll PUSHED (D6=3). Pushed east onto the loose ball at (12,7) -- it
    // scatters (D8=3 -> East -> (13,7)); no catch attempt, no turnover.
    FixedDiceRoller dice({3, 3});
    BlockParams params{1, 12, false, false};
    auto result = resolveBlock(gs, params, dice, nullptr);
    EXPECT_EQ(gs.getPlayer(12).position, (Position{12, 7}));
    EXPECT_FALSE(gs.ball.isHeld);
    EXPECT_EQ(gs.ball.position, (Position{13, 7}));
    EXPECT_FALSE(result.turnover);
}

TEST(BlockHandler, ChainPushOntoLooseBallBounces) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    // All 3 of the defender's primary pushback squares occupied, so the
    // default "prefer empty" logic falls through to pushSquares[0]=(12,7)
    // -- forcing the chain push.
    placePlayer(gs, 13, {12, 7}, TeamSide::AWAY); // chain-push occupant
    placePlayer(gs, 14, {12, 8}, TeamSide::AWAY);
    placePlayer(gs, 15, {12, 6}, TeamSide::AWAY);
    gs.ball = BallState::onGround({13, 7});
    // Roll PUSHED (D6=3). Defender pushed east into the occupant's square;
    // occupant chain-pushed further east onto the loose ball at (13,7) --
    // it scatters (D8=3 -> East -> (14,7)).
    FixedDiceRoller dice({3, 3});
    BlockParams params{1, 12, false, false};
    auto result = resolveBlock(gs, params, dice, nullptr);
    EXPECT_EQ(gs.getPlayer(13).position, (Position{13, 7}));
    EXPECT_FALSE(gs.ball.isHeld);
    EXPECT_EQ(gs.ball.position, (Position{14, 7}));
}

TEST(BlockHandler, FollowUpOntoLooseBallPicksUp) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    gs.ball = BallState::onGround({11, 7}); // defender stands on a loose ball
    // Roll PUSHED (D6=3). Defender pushed east to (12,7); the ball wasn't
    // carried, so it stays at (11,7). Attacker follows up onto (11,7) and
    // attempts a pickup: AG3 target 3, roll 4 -> success.
    FixedDiceRoller dice({3, 4});
    BlockParams params{1, 12, false, false};
    auto result = resolveBlock(gs, params, dice, nullptr);
    EXPECT_EQ(gs.getPlayer(1).position, (Position{11, 7}));
    EXPECT_TRUE(gs.ball.isHeld);
    EXPECT_EQ(gs.ball.carrierId, 1);
    EXPECT_FALSE(result.turnover);
}

TEST(BlockHandler, FollowUpOntoLooseBallFailedPickupTurnsOver) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    gs.ball = BallState::onGround({11, 7});
    // Roll PUSHED (D6=3). Follow-up pickup fails (roll 2), ball bounces
    // (D8=3 -> East -> (12,7)) instead of resting under the attacker --
    // and (12,7) is exactly where the pushed defender now stands, who
    // then attempts (and makes, AG3 target 4, roll 5) the catch.
    FixedDiceRoller dice({3, 2, 3, 5});
    BlockParams params{1, 12, false, false};
    auto result = resolveBlock(gs, params, dice, nullptr);
    EXPECT_EQ(gs.getPlayer(1).position, (Position{11, 7}));
    EXPECT_TRUE(gs.ball.isHeld);
    EXPECT_EQ(gs.ball.carrierId, 12);
    EXPECT_TRUE(result.turnover);
}

TEST(BlockHandler, KnockdownBounceCanLandOnFollowedUpAttacker) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    gs.ball = BallState::carried({11, 7}, 12);
    // Roll DEFENDER_DOWN (D6=6): defender pushed to (12,7) and knocked
    // down. Armor: 3+3=6, not broken (no injury roll). The attacker
    // follows up into (11,7) -- the defender's vacated square -- BEFORE
    // the knockdown code runs and drops+bounces the ball from (12,7):
    // D8=7 -> West -> lands right back on (11,7), where the attacker now
    // stands. AG3 target 4, roll 5 -> catch succeeds.
    FixedDiceRoller dice({6, 3, 3, 7, 5});
    BlockParams params{1, 12, false, false};
    auto result = resolveBlock(gs, params, dice, nullptr);
    EXPECT_EQ(gs.getPlayer(1).position, (Position{11, 7}));
    EXPECT_TRUE(gs.ball.isHeld);
    EXPECT_EQ(gs.ball.carrierId, 1);
}

TEST(BlockHandler, CrowdSurf) {
    GameState gs;
    placePlayer(gs, 1, {24, 7}, TeamSide::HOME);
    placePlayer(gs, 12, {25, 7}, TeamSide::AWAY);
    // Defender at edge. Roll DD (D6=6) → pushed off pitch
    // Crowd surf injury: 3+3+1=7 → stunned → forced KO
    FixedDiceRoller dice({6, 3, 3});
    BlockParams params{1, 12, false, false};
    auto result = resolveBlock(gs, params, dice, nullptr);
    EXPECT_TRUE(result.success);
    // Defender crowd surfed
    EXPECT_EQ(gs.getPlayer(12).state, PlayerState::KO);
}

TEST(BlockHandler, StandFirmPrevents) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    gs.getPlayer(12).skills.add(SkillName::StandFirm);
    // Roll PUSHED. StandFirm blocks push
    FixedDiceRoller dice({3});
    BlockParams params{1, 12, false, false};
    auto result = resolveBlock(gs, params, dice, nullptr);
    EXPECT_EQ(gs.getPlayer(12).position, (Position{11, 7})); // didn't move
}

TEST(BlockHandler, JuggernautIgnoresStandFirmOnBlitz) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::Juggernaut);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    gs.getPlayer(12).skills.add(SkillName::StandFirm);
    // Roll PUSHED on blitz. Juggernaut ignores StandFirm
    FixedDiceRoller dice({3});
    BlockParams params{1, 12, true, false};
    auto result = resolveBlock(gs, params, dice, nullptr);
    EXPECT_NE(gs.getPlayer(12).position, (Position{11, 7})); // pushed
}

TEST(BlockHandler, MightyBlowAndClaw) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::MightyBlow);
    gs.getPlayer(1).skills.add(SkillName::Claw);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY, 6, 3, 3, 9);
    // Roll DD. Defender pushed + knocked down
    // Claw: armor broken on 8+. MB: +1 to armor and injury
    // Armor: 4+3+1=8, Claw=8 → broken! Injury: 3+3+1=7 → stunned
    FixedDiceRoller dice({6, 4, 3, 3, 3});
    BlockParams params{1, 12, false, false};
    auto result = resolveBlock(gs, params, dice, nullptr);
    EXPECT_EQ(gs.getPlayer(12).state, PlayerState::STUNNED);
}

TEST(BlockHandler, HornsBonusOnBlitz) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME); // ST3
    gs.getPlayer(1).skills.add(SkillName::Horns);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY); // ST3
    // With Horns on blitz: ST3+1=4 vs ST3 → 2 dice attacker
    // Roll: DD, AD → choose DD. Armor: 3+3=6
    FixedDiceRoller dice({6, 1, 3, 3});
    BlockParams params{1, 12, true, true};
    auto result = resolveBlock(gs, params, dice, nullptr);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(gs.getPlayer(12).state, PlayerState::PRONE);
}

TEST(BlockHandler, DauntlessSuccess) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME); // ST3
    gs.getPlayer(1).skills.add(SkillName::Dauntless);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY, 6, 5, 3, 9); // ST5
    // Effective: ST3 vs ST5 (defender stronger)
    // Dauntless: D6=4, 4+3=7 > 5 → treat as equal (1 die attacker)
    // Then block die: DD. Armor: 3+3=6 ≤ 9
    FixedDiceRoller dice({4, 6, 3, 3});
    BlockParams params{1, 12, false, false};
    auto result = resolveBlock(gs, params, dice, nullptr);
    EXPECT_EQ(gs.getPlayer(12).state, PlayerState::PRONE);
}

TEST(BlockHandler, FoulAppearanceBlocks) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    gs.getPlayer(12).skills.add(SkillName::FoulAppearance);
    // FA roll: 1 → attacker too revolted
    FixedDiceRoller dice({1});
    BlockParams params{1, 12, false, false};
    auto result = resolveBlock(gs, params, dice, nullptr);
    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.turnover);
    EXPECT_TRUE(gs.getPlayer(1).hasActed);
}

TEST(BlockHandler, StabArmorRoll) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::Stab);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    // Stab: armor roll. 5+4=9 > 8 → broken. Injury: 3+3=6 → stunned
    FixedDiceRoller dice({5, 4, 3, 3});
    BlockParams params{1, 12, false, false};
    auto result = resolveBlock(gs, params, dice, nullptr);
    EXPECT_TRUE(result.success);
    EXPECT_FALSE(result.turnover); // Stab never causes turnover
}

TEST(BlockHandler, ChainsawSuccess) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::Chainsaw);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    // Chainsaw: D6=3 (2+ success) → armor on defender: 5+4=9 > 8 → injured 3+3
    FixedDiceRoller dice({3, 5, 4, 3, 3});
    BlockParams params{1, 12, false, false};
    auto result = resolveBlock(gs, params, dice, nullptr);
    EXPECT_TRUE(result.success);
}

TEST(BlockHandler, ChainsawKickback) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::Chainsaw);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    // Chainsaw: D6=1 → kickback on attacker. Armor: 3+3=6 ≤ 8 not broken
    FixedDiceRoller dice({1, 3, 3});
    BlockParams params{1, 12, false, false};
    auto result = resolveBlock(gs, params, dice, nullptr);
    EXPECT_TRUE(result.turnover);
    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::PRONE);
}

TEST(BlockHandler, FrenzyDoubleBlock) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::Frenzy);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    // 1st block: PUSHED (D6=3). Defender pushed to (12,7). Attacker follows to (11,7).
    // Both still standing + adjacent → mandatory 2nd block
    // 2nd block: DD (D6=6). Defender knocked down. Armor: 3+3=6.
    FixedDiceRoller dice({3, 6, 3, 3});
    BlockParams params{1, 12, false, false};
    auto result = resolveBlock(gs, params, dice, nullptr);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(gs.getPlayer(12).state, PlayerState::PRONE);
}

TEST(BlockHandler, AttackerDown) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    // Roll AD (D6=1). Attacker falls → turnover. Armor: 3+3=6
    FixedDiceRoller dice({1, 3, 3});
    BlockParams params{1, 12, false, false};
    auto result = resolveBlock(gs, params, dice, nullptr);
    EXPECT_TRUE(result.turnover);
    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::PRONE);
    EXPECT_TRUE(gs.getPlayer(1).hasActed);
}

TEST(BlockHandler, StripBall) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::StripBall);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    gs.ball = BallState::carried({11, 7}, 12);
    // Roll PUSHED. Defender pushed to (12,7). StripBall → ball drops at (12,7)
    // Ball bounces from (12,7): D8=3 → (13,7)
    FixedDiceRoller dice({3, 3});
    BlockParams params{1, 12, false, false};
    auto result = resolveBlock(gs, params, dice, nullptr);
    EXPECT_FALSE(gs.ball.isHeld);
}

TEST(BlockHandler, StripBallNegatedBySureHands) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::StripBall);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    gs.getPlayer(12).skills.add(SkillName::SureHands);
    gs.ball = BallState::carried({11, 7}, 12);
    // Roll PUSHED. Defender pushed to (12,7). Sure Hands negates Strip Ball (BB2016)
    // → ball stays with the carrier, and a SKILL_USED(SureHands) event is emitted.
    FixedDiceRoller dice({3, 3});
    BlockParams params{1, 12, false, false};
    std::vector<GameEvent> events;
    auto result = resolveBlock(gs, params, dice, &events);
    EXPECT_TRUE(gs.ball.isHeld);
    EXPECT_EQ(gs.ball.carrierId, 12);
    bool sawSureHands = false;
    for (const auto& e : events) {
        if (e.type == GameEvent::Type::SKILL_USED &&
            e.roll == static_cast<int>(SkillName::SureHands)) {
            sawSureHands = true;
        }
    }
    EXPECT_TRUE(sawSureHands);
}

TEST(BlockHandler, AutoChoosePrefersDDOverAD) {
    Player att, def;
    att.id = 1; att.teamSide = TeamSide::HOME;
    def.id = 12; def.teamSide = TeamSide::AWAY;

    BlockDiceFace faces[] = {BlockDiceFace::ATTACKER_DOWN, BlockDiceFace::DEFENDER_DOWN};
    auto chosen = autoChooseBlockDie(faces, 2, true, att, def);
    EXPECT_EQ(chosen, BlockDiceFace::DEFENDER_DOWN);
}

TEST(BlockHandler, DefenderChoosesWorstForAttacker) {
    Player att, def;
    att.id = 1; att.teamSide = TeamSide::HOME;
    def.id = 12; def.teamSide = TeamSide::AWAY;

    BlockDiceFace faces[] = {BlockDiceFace::PUSHED, BlockDiceFace::DEFENDER_DOWN};
    auto chosen = autoChooseBlockDie(faces, 2, false, att, def);
    EXPECT_EQ(chosen, BlockDiceFace::PUSHED); // less bad for defender
}

TEST(BlockHandler, JuggernautConvertsBDToPushOnBlitz) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::Juggernaut);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    // Roll BD (D6=2). Juggernaut on blitz → treated as PUSHED
    FixedDiceRoller dice({2});
    BlockParams params{1, 12, true, false};
    auto result = resolveBlock(gs, params, dice, nullptr);
    EXPECT_FALSE(result.turnover);
    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::STANDING);
    EXPECT_EQ(gs.getPlayer(12).state, PlayerState::STANDING);
}

// --- MultipleBlock tests ---

TEST(BlockHandler, MultipleBlockBothResolve) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME, 6, 4, 3, 8); // ST4
    gs.getPlayer(1).skills.add(SkillName::MultipleBlock);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY); // ST3
    placePlayer(gs, 13, {11, 8}, TeamSide::AWAY); // ST3

    // Block 1: def1 gets +2 ST → ST5 vs ST4 = 2 dice defender chooses
    // Roll: DD, DD → defender chooses DD (worst for attacker but only option)
    // Armor: 3+3=6
    // Block 2: same
    // Roll: DD, DD → armor: 3+3=6
    FixedDiceRoller dice({6, 6, 3, 3, 6, 6, 3, 3});
    auto result = resolveMultipleBlock(gs, 1, 12, 13, dice, nullptr);
    // Both defenders hit
    EXPECT_EQ(gs.getPlayer(12).state, PlayerState::PRONE);
    EXPECT_EQ(gs.getPlayer(13).state, PlayerState::PRONE);
}

TEST(BlockHandler, MultipleBlockNoFollowUp) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME, 6, 6, 3, 8); // ST6
    gs.getPlayer(1).skills.add(SkillName::MultipleBlock);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY); // ST3+2=5; def2 at (11,8) assists → 6vs6=1die
    placePlayer(gs, 13, {11, 8}, TeamSide::AWAY);

    Position attOrigPos = gs.getPlayer(1).position;
    // Block 1: 1 die att (6 vs 6 with assist). DD → push+knockdown. Armor: 3+3
    // Block 2: after def1 pushed away, no assist. 6 vs 5 → 2 dice att. DD,DD. Armor: 3+3
    FixedDiceRoller dice({6, 3, 3, 6, 6, 3, 3});
    auto result = resolveMultipleBlock(gs, 1, 12, 13, dice, nullptr);
    // Attacker should NOT have followed up
    EXPECT_EQ(gs.getPlayer(1).position, attOrigPos);
}

TEST(BlockHandler, MultipleBlockAttackerDownSkipsSecond) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME, 6, 3, 3, 8); // ST3
    gs.getPlayer(1).skills.add(SkillName::MultipleBlock);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY); // ST3+2=5 → 2-die def
    placePlayer(gs, 13, {11, 8}, TeamSide::AWAY);

    // Block 1: 2 dice def. Roll: AD, AD → def chooses AD → attacker down, turnover
    // Armor: 3+3=6
    FixedDiceRoller dice({1, 1, 3, 3});
    auto result = resolveMultipleBlock(gs, 1, 12, 13, dice, nullptr);
    EXPECT_TRUE(result.turnover);
    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::PRONE);
    EXPECT_EQ(gs.getPlayer(13).state, PlayerState::STANDING); // 2nd untouched
}

TEST(BlockHandler, MultipleBlockFAFailSkipsBlock) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME, 6, 6, 3, 8);
    gs.getPlayer(1).skills.add(SkillName::MultipleBlock);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    gs.getPlayer(12).skills.add(SkillName::FoulAppearance);
    placePlayer(gs, 13, {11, 8}, TeamSide::AWAY);

    // Block 1: FA check → roll 1 → skip block 1
    // Block 2: ST6 vs ST5 = 1 die att. DD → knockdown. Armor: 3+3
    FixedDiceRoller dice({1, 6, 3, 3});
    auto result = resolveMultipleBlock(gs, 1, 12, 13, dice, nullptr);
    EXPECT_EQ(gs.getPlayer(12).state, PlayerState::STANDING); // Skipped
    EXPECT_EQ(gs.getPlayer(13).state, PlayerState::PRONE); // Hit
}
