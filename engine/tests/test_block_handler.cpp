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

// F11 (24.08.2026): `WrestleBothProne` vys testuje NENOSICE a je spravne, ale
// je to happy path -- vada lezela v pripadu, ktery zadny test nemel. BB2016
// l. 8677-8678: "Use of this skill does not cause a turnover UNLESS THE ACTIVE
// PLAYER WAS HOLDING THE BALL." Kod vracel ok() bezpodmínečně.

TEST(BlockHandler, WrestleIsTurnoverWhenTheActivePlayerHeldTheBall) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    gs.getPlayer(12).skills.add(SkillName::Wrestle);
    gs.ball.isHeld = true;               // blokujici (aktivni) hráč nese míč
    gs.ball.carrierId = 1;
    gs.ball.position = {10, 7};
    FixedDiceRoller dice({2, 3, 4, 5, 6, 1, 2, 3});  // BD + odraz uvolneneho mice
    BlockParams params{1, 12, false, false};
    auto result = resolveBlock(gs, params, dice, nullptr);
    EXPECT_TRUE(result.turnover);
    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::PRONE);
    EXPECT_EQ(gs.getPlayer(12).state, PlayerState::PRONE);
}

TEST(BlockHandler, WrestleIsNoTurnoverWhenTheDEFENDERHeldTheBall) {
    // druha strana hranice: mic drzi nekdo z NEaktivniho tymu -> zadny turnover
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    gs.getPlayer(12).skills.add(SkillName::Wrestle);
    gs.ball.isHeld = true;
    gs.ball.carrierId = 12;
    gs.ball.position = {11, 7};
    FixedDiceRoller dice({2, 3, 4, 5, 6, 1, 2, 3});  // BD + odraz uvolneneho mice
    BlockParams params{1, 12, false, false};
    auto result = resolveBlock(gs, params, dice, nullptr);
    EXPECT_FALSE(result.turnover);
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

TEST(BlockHandler, ACarrierIsNotPushedIntoTheEndZoneHeIsAttacking) {
    GameState gs;
    placePlayer(gs, 1, {2, 8}, TeamSide::HOME);
    placePlayer(gs, 12, {1, 7}, TeamSide::AWAY);
    gs.ball = BallState::carried({1, 7}, 12);
    // g0289 mirrored. Pushing north-west off (2,8) offers (0,6), (1,6) and
    // (0,7); two of those are the away end zone, and "straight back first"
    // used to take (0,6) -- which CRP scores for him even on our own turn.
    // (1,6) is the one square that declines the gift.
    FixedDiceRoller dice({3, 3, 3, 3, 3, 3});    // PUSHED (+ zásoba na následné hody)
    BlockParams params{1, 12, false, false};
    resolveBlock(gs, params, dice, nullptr);
    EXPECT_EQ(gs.getPlayer(12).position, (Position{1, 6}));
    EXPECT_FALSE(gs.getPlayer(12).position.isInEndZone(true));
    EXPECT_TRUE(gs.ball.isHeld);
}

TEST(BlockHandler, WhenEveryPushSquareScoresThePushHappensAnyway) {
    GameState gs;
    placePlayer(gs, 1, {2, 7}, TeamSide::HOME);
    placePlayer(gs, 12, {1, 7}, TeamSide::AWAY);
    gs.ball = BallState::carried({1, 7}, 12);
    // Straight west the three squares are (0,6), (0,7) and (0,8) -- all of them
    // the end zone. Nothing to decline to, so the rules take their course and
    // the guard must not deadlock or refuse to move him.
    FixedDiceRoller dice({3});
    BlockParams params{1, 12, false, false};
    resolveBlock(gs, params, dice, nullptr);
    EXPECT_EQ(gs.getPlayer(12).position.x, 0);
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
    // Crowd surf injury 3+3=6 -> Stunned -> Reserves (CRP), not KO.
    FixedDiceRoller dice({6, 3, 3});
    BlockParams params{1, 12, false, false};
    auto result = resolveBlock(gs, params, dice, nullptr);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(gs.getPlayer(12).state, PlayerState::OFF_PITCH);
    EXPECT_FALSE(gs.getPlayer(12).isOnPitch());
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
    // Roll DD. Defender pushed + knocked down.
    // Armour 4+3=7 does not break AV9 and is not the 8 Claw wants, so Mighty
    // Blow is spent there to make it 8 -- and is therefore NOT available to the
    // injury roll, which stands at 3+3=6. Stunned either way; the point of the
    // case is that Claw and Mighty Blow still compose on the armour roll.
    FixedDiceRoller dice({6, 4, 3, 3, 3});
    BlockParams params{1, 12, false, false};
    auto result = resolveBlock(gs, params, dice, nullptr);
    EXPECT_EQ(gs.getPlayer(12).state, PlayerState::STUNNED);
}

TEST(BlockHandler, MightyBlowIsKeptForTheInjuryWhenArmourBreaksWithoutIt) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::MightyBlow);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY, 6, 3, 3, 8);   // AV8
    // DD, then armour 4+5=9, which already beats AV8. Nothing to spend there,
    // so the +1 goes to the injury roll: 4+4=8, +1 = 9 -> KO rather than Stunned.
    std::vector<GameEvent> events;
    FixedDiceRoller dice({6, 4, 5, 4, 4});
    BlockParams params{1, 12, false, false};
    resolveBlock(gs, params, dice, &events);

    for (auto& e : events) {
        if (e.type == GameEvent::Type::ARMOR_BREAK) {
            EXPECT_EQ(e.roll, 9) << "unspent Mighty Blow must not inflate the armour roll";
        }
    }
    EXPECT_EQ(gs.getPlayer(12).state, PlayerState::KO);
}

TEST(BlockHandler, MightyBlowIsSpentOnArmourOnlyWhenArmourNeedsIt) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::MightyBlow);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY, 6, 3, 3, 8);   // AV8
    // DD, then armour 4+4=8, which does NOT beat AV8 -- the +1 is what breaks it.
    // Having been spent, it cannot also lift the injury: 4+4=8 stays a KO
    // boundary rather than becoming 9.
    std::vector<GameEvent> events;
    FixedDiceRoller dice({6, 4, 4, 4, 4});
    BlockParams params{1, 12, false, false};
    resolveBlock(gs, params, dice, &events);

    for (auto& e : events) {
        if (e.type == GameEvent::Type::ARMOR_BREAK) {
            EXPECT_EQ(e.roll, 9) << "the +1 is what took 8 past AV8";
            EXPECT_TRUE(e.success);
        }
        if (e.type == GameEvent::Type::INJURY) {
            EXPECT_EQ(e.roll, 8) << "Mighty Blow was already spent on the armour";
        }
    }
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

// ⛔ TA7 (24.08.2026): `ChainsawKickback` certifikoval DVE vady naraz --
// komentar sam pocital "3+3=6 <= 8 not broken" a pak asertoval PRONE
// + turnover. BB2016 l. 8001-8006: k hodu na zbroj se PRICITA 3, a kdyz
// zbroj neprorazi, "the attack HAS NO EFFECT".

TEST(BlockHandler, ChainsawKickbackWithoutBreakingArmourHasNoEffect) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::Chainsaw);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    // D6=1 => kickback na nositele. Zbroj: 2+2 = 4, +3 = 7 <= AV8 => neproraženo
    FixedDiceRoller dice({1, 2, 2});
    BlockParams params{1, 12, false, false};
    auto result = resolveBlock(gs, params, dice, nullptr);
    EXPECT_FALSE(result.turnover);
    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::STANDING);
}

TEST(BlockHandler, ChainsawKickbackThatBreaksArmourFloorsTheWielder) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::Chainsaw);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    // D6=1 => kickback. Zbroj: 3+3 = 6, +3 = 9 > AV8 => proraženo.
    // Hod na zraneni 2D6 = 1+2 = 3 => Stunned.
    FixedDiceRoller dice({1, 3, 3, 1, 2});
    BlockParams params{1, 12, false, false};
    auto result = resolveBlock(gs, params, dice, nullptr);
    EXPECT_TRUE(result.turnover);      // srazeny hráč aktivniho tymu
    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::STUNNED);
}

TEST(BlockHandler, ChainsawAddsThreeToTheVictimsArmourRoll) {
    // l. 8003-8004: bez toho +3 mela pila probojnost obycejneho bloku.
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::Chainsaw);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    // D6=2 => zasah. Zbroj: 3+3 = 6 -- bez +3 by to AV8 NEprorazilo,
    // s +3 je to 9 > 8 => prorazi. Zraneni 2D6 = 1+2 = 3 => Stunned.
    FixedDiceRoller dice({2, 3, 3, 1, 2});
    BlockParams params{1, 12, false, false};
    auto result = resolveBlock(gs, params, dice, nullptr);
    EXPECT_FALSE(result.turnover);     // obet je souper
    EXPECT_EQ(gs.getPlayer(12).state, PlayerState::STUNNED);
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

// CRP: a block thrown during a Blitz costs 1 MP; out of movement it takes
// a GFI (2+), and with no GFI left it cannot be thrown. User-supplied
// scenario (30.07): first GFI gets the blitzer adjacent, the second GFI
// pays for the first block, so Frenzy's mandatory second block is DENIED.
TEST(BlockHandler, BlitzFrenzySecondBlockDeniedWithoutMovement) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::Frenzy);
    gs.getPlayer(1).movementRemaining = -1;  // first GFI already spent arriving
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);

    std::vector<GameEvent> events;
    // GFI for the block (roll 6), then 1st block PUSHED (D6=3): defender
    // pushed to (12,7), attacker follows -- both standing and adjacent, but
    // movementRemaining is now -2 = GFI limit, so the 2nd block must NOT
    // be thrown (FixedDiceRoller would throw on any extra roll).
    FixedDiceRoller dice({6, 3});
    BlockParams params{1, 12, true, false};
    auto result = resolveBlock(gs, params, dice, nullptr, false, false);
    // events not passed above -- rerun pattern kept simple: assert via state
    EXPECT_TRUE(result.success);
    EXPECT_FALSE(result.turnover);
    EXPECT_EQ(gs.getPlayer(1).movementRemaining, -2);
    EXPECT_EQ(gs.getPlayer(1).position, (Position{11, 7}));  // followed up once
    EXPECT_EQ(gs.getPlayer(12).position, (Position{12, 7}));  // pushed once, not twice
    EXPECT_EQ(dice.remaining(), 0u);  // no dice consumed beyond GFI + 1 block die
}

// M1/N10 (25.08.2026): BB2016 l. 347-350 -- "He may make one block during the
// move. The block may be made AT ANY POINT during the move." resolveBlock set
// hasActed on every path, so a blitzer could never move after his block: no
// hit-and-run, and no "the carrier opens his own lane with a blitz and runs
// through it". The user reported this on 22.07 and it sat for 33 days; M9
// measured the ceiling on 24.08 (4.09 blitzes a game end stuck in contact with
// movement left and somewhere to go, AV7 pieces 1.5x more often than AV9).
//
// These three pin the boundary rather than the fix: a blitz leaves the
// activation open, a Block Action does not, and going down closes it either way.
TEST(BlockHandler, BlitzLeavesTheActivationOpenAfterTheBlock) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).movementRemaining = 4;
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);

    FixedDiceRoller dice({3});   // PUSHED, both stay standing
    BlockParams params{1, 12, true, false};
    auto result = resolveBlock(gs, params, dice, nullptr);

    EXPECT_TRUE(result.success);
    EXPECT_FALSE(result.turnover);
    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::STANDING);
    EXPECT_EQ(gs.getPlayer(1).movementRemaining, 3);  // the block cost 1 MP
    EXPECT_FALSE(gs.getPlayer(1).hasActed)
        << "a blitzer with movement left must still be able to move after the block";
}

TEST(BlockHandler, BlockActionStillEndsTheActivation) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).movementRemaining = 4;
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);

    FixedDiceRoller dice({3});   // same PUSHED, but a Block Action this time
    BlockParams params{1, 12, false, false};
    resolveBlock(gs, params, dice, nullptr);

    EXPECT_TRUE(gs.getPlayer(1).hasActed)
        << "a Block Action is the whole activation -- only a Blitz continues";
}

TEST(BlockHandler, BlitzerWhoGoesDownCannotKeepMoving) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).movementRemaining = 4;
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);

    // BOTH_DOWN with neither player holding Block: the attacker falls, so the
    // activation is over no matter how much movement is left on paper.
    FixedDiceRoller dice({2, 3, 3, 3, 3});
    BlockParams params{1, 12, true, false};
    resolveBlock(gs, params, dice, nullptr);

    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::PRONE);
    EXPECT_TRUE(gs.getPlayer(1).hasActed);
}

TEST(BlockHandler, BlitzBlockGfiFailKnocksAttackerDown) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).movementRemaining = 0;  // block itself needs a GFI
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);

    // GFI roll 1 = fail -> attacker falls before the block. Armor 3+3=6 <= 8.
    FixedDiceRoller dice({1, 3, 3});
    BlockParams params{1, 12, true, false};
    auto result = resolveBlock(gs, params, dice, nullptr, false, false);
    EXPECT_TRUE(result.turnover);
    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::PRONE);
    EXPECT_EQ(gs.getPlayer(12).state, PlayerState::STANDING);  // never blocked
}

TEST(BlockHandler, PlainBlockCostsNoMovement) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);

    FixedDiceRoller dice({3});  // PUSHED
    BlockParams params{1, 12, false, false};
    auto result = resolveBlock(gs, params, dice, nullptr, false, false);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(gs.getPlayer(1).movementRemaining, 6);  // unchanged
}

// --- Push backs: "must be pushed back into an empty square if possible" -----

TEST(BlockHandler, SideStepStillHasToLandOnAnEmptySquare) {
    // Side Step lets the defender pick the square, but not an occupied one:
    // it used to skip the empty-square check entirely and chain-push a
    // bystander while two empty squares stood open.
    GameState gs;
    gs.phase = GamePhase::PLAY;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME, 6, 4, 3, 8);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    gs.getPlayer(12).skills.add(SkillName::SideStep);
    placePlayer(gs, 13, {12, 7}, TeamSide::AWAY);   // straight back is blocked
    gs.ball.isHeld = false;
    gs.ball.position = {0, 0};

    FixedDiceRoller dice({3, 3, 3, 3, 3});          // 2 dice, both PUSHED
    BlockParams params{1, 12, false, false};
    resolveBlock(gs, params, dice, nullptr);

    EXPECT_NE(gs.getPlayer(12).position, (Position{12, 7}));
    EXPECT_EQ(gs.getPlayer(12).position.x, 12);      // one of the empty diagonals
    EXPECT_EQ(gs.getPlayer(13).position, (Position{12, 7}));  // bystander unmoved
}

TEST(BlockHandler, ChainPushKeepsChainingAndNeverStacksPlayers) {
    // "This secondary push back is treated exactly like a normal push back" --
    // so when the chain target has no empty square either, it chains on. The
    // single-hop version parked two players on the same square.
    GameState gs;
    gs.phase = GamePhase::PLAY;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME, 6, 5, 3, 8);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    placePlayer(gs, 2, {12, 7}, TeamSide::HOME);
    placePlayer(gs, 3, {12, 6}, TeamSide::HOME);
    placePlayer(gs, 4, {12, 8}, TeamSide::HOME);
    placePlayer(gs, 5, {13, 7}, TeamSide::HOME);
    placePlayer(gs, 6, {13, 6}, TeamSide::HOME);
    placePlayer(gs, 7, {13, 8}, TeamSide::HOME);
    gs.ball.isHeld = false;
    gs.ball.position = {0, 0};

    FixedDiceRoller dice({3, 3, 3, 3, 3, 3});
    BlockParams params{1, 12, false, false};
    resolveBlock(gs, params, dice, nullptr);

    EXPECT_EQ(gs.getPlayer(12).position, (Position{12, 7}));
    EXPECT_EQ(gs.getPlayer(2).position, (Position{13, 7}));
    EXPECT_EQ(gs.getPlayer(5).position, (Position{14, 7}));   // chained on again

    for (int i = 1; i <= GameState::PLAYERS_TOTAL; i++) {
        for (int j = i + 1; j <= GameState::PLAYERS_TOTAL; j++) {
            const Player& a = gs.getPlayer(i);
            const Player& b = gs.getPlayer(j);
            if (a.isOnPitch() && b.isOnPitch()) {
                EXPECT_NE(a.position, b.position) << "players " << i << " and " << j;
            }
        }
    }
}

TEST(BlockHandler, StandFirmDefenderIsNotFollowedUpOnto) {
    // The defender holds his square, so there is nothing to follow up into.
    // The follow-up used to fire regardless and put the attacker on top of him.
    GameState gs;
    gs.phase = GamePhase::PLAY;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME, 6, 4, 3, 8);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    gs.getPlayer(12).skills.add(SkillName::StandFirm);
    gs.ball.isHeld = false;
    gs.ball.position = {0, 0};

    FixedDiceRoller dice({3, 3, 3, 3, 3});
    BlockParams params{1, 12, false, false};
    resolveBlock(gs, params, dice, nullptr);

    EXPECT_EQ(gs.getPlayer(12).position, (Position{11, 7}));
    EXPECT_EQ(gs.getPlayer(1).position, (Position{10, 7}));
}

TEST(BlockHandler, ChainIntoStandFirmMovesNobody) {
    // CRP Stand Firm: "If a player is pushed back into a player using Stand
    // Firm then neither player moves." That reaches down the chain too.
    GameState gs;
    gs.phase = GamePhase::PLAY;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME, 6, 5, 3, 8);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    placePlayer(gs, 13, {12, 7}, TeamSide::AWAY);
    placePlayer(gs, 14, {12, 6}, TeamSide::AWAY);
    placePlayer(gs, 15, {12, 8}, TeamSide::AWAY);
    gs.getPlayer(13).skills.add(SkillName::StandFirm);
    gs.getPlayer(14).skills.add(SkillName::StandFirm);
    gs.getPlayer(15).skills.add(SkillName::StandFirm);
    gs.ball.isHeld = false;
    gs.ball.position = {0, 0};

    FixedDiceRoller dice({3, 3, 3, 3, 3, 3});
    BlockParams params{1, 12, false, false};
    resolveBlock(gs, params, dice, nullptr);

    EXPECT_EQ(gs.getPlayer(12).position, (Position{11, 7}));
    EXPECT_EQ(gs.getPlayer(13).position, (Position{12, 7}));
    EXPECT_EQ(gs.getPlayer(1).position, (Position{10, 7}));
}

TEST(BlockHandler, DauntlessIgnoresAssists) {
    // CRP: "The strength of both players is calculated before any defensive or
    // offensive assists are added but after all other modifiers", and the skill
    // "only works when the player attempts to block an opponent who is stronger
    // than himself". Equal base strength plus enemy assists is not stronger --
    // this used to fire Dauntless, which then could not fail.
    GameState gs;
    gs.phase = GamePhase::PLAY;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);            // ST3, Dauntless
    gs.getPlayer(1).skills.add(SkillName::Dauntless);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);           // ST3
    placePlayer(gs, 13, {10, 6}, TeamSide::AWAY);           // defensive assists
    placePlayer(gs, 14, {10, 8}, TeamSide::AWAY);
    gs.ball.isHeld = false;
    gs.ball.position = {0, 0};

    std::vector<GameEvent> events;
    FixedDiceRoller dice({3, 3, 3, 3, 3, 3});
    BlockParams params{1, 12, false, false};
    resolveBlock(gs, params, dice, &events);

    for (const auto& e : events) {
        EXPECT_FALSE(e.type == GameEvent::Type::SKILL_USED &&
                     e.roll == static_cast<int>(SkillName::Dauntless))
            << "Dauntless fired against an equally strong opponent";
    }
}

TEST(BlockHandler, DauntlessEqualisesBeforeOurOwnAssistsAreAdded) {
    // Psyched up against ST5 the ST3 blocker counts as ST5, and his own
    // offensive assist then puts him ahead -- he keeps his assists rather than
    // inheriting the defender's total.
    GameState gs;
    gs.phase = GamePhase::PLAY;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);            // ST3, Dauntless
    gs.getPlayer(1).skills.add(SkillName::Dauntless);
    gs.getPlayer(1).skills.add(SkillName::Block);
    placePlayer(gs, 2, {11, 6}, TeamSide::HOME);            // offensive assist
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY, 6, 5, 3, 9);  // ST5
    gs.ball.isHeld = false;
    gs.ball.position = {0, 0};

    std::vector<GameEvent> events;
    // Dauntless D6=4 -> 4+3 > 5 psyched; then 2 dice (4+1=5... vs 5) attacker
    FixedDiceRoller dice({4, 6, 6, 3, 3});
    BlockParams params{1, 12, false, false};
    resolveBlock(gs, params, dice, &events);

    bool psyched = false;
    for (const auto& e : events) {
        if (e.type == GameEvent::Type::SKILL_USED &&
            e.roll == static_cast<int>(SkillName::Dauntless)) psyched = e.success;
    }
    EXPECT_TRUE(psyched);
    EXPECT_EQ(gs.getPlayer(12).state, PlayerState::PRONE);
}

// ---------------------------------------------------------------------------
// P9 / P9c (2026-08-18): the push DESTINATION is chosen, not just taken.
//
// Motivation and corpus numbers live in bb/block_handler.h. These pin the two
// defects that were actually measured, plus the two guarantees that keep the
// arm honest: OFF reproduces "straight back first" bit for bit, and the counter
// only ticks when the arm really redirected.
namespace {
struct PushArmOn {
    explicit PushArmOn(TeamSide s) : side(s) { setPushGeometryArm(side, true); }
    ~PushArmOn() { setPushGeometryArm(side, false); takePushGeometryEvalsInSearch(); }
    TeamSide side;
};
}  // namespace

TEST(PushGeometry, ArmOffKeepsStraightBackExactly) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    // Our carrier sits right behind the push line, so with the arm ON the
    // destination would matter. OFF it must not.
    placePlayer(gs, 2, {12, 8}, TeamSide::HOME);
    gs.ball = BallState::carried({12, 8}, 2);
    FixedDiceRoller dice({3, 3, 3, 3, 3, 3});    // PUSHED (+ zásoba na následné hody)
    BlockParams params{1, 12, false, false};
    resolveBlock(gs, params, dice, nullptr);
    EXPECT_EQ(gs.getPlayer(12).position, (Position{12, 7}))
        << "arm OFF must reproduce straight-back first";
    EXPECT_EQ(takePushGeometryEvalsInSearch(), 0);
}

TEST(PushGeometry, DoesNotShoveHimNextToOurOwnCarrier) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    placePlayer(gs, 2, {12, 8}, TeamSide::HOME);
    gs.ball = BallState::carried({12, 8}, 2);   // our carrier one step away
    PushArmOn arm(TeamSide::HOME);
    FixedDiceRoller dice({3, 3, 3, 3, 3, 3});    // PUSHED (+ zásoba na následné hody)
    BlockParams params{1, 12, false, false};
    resolveBlock(gs, params, dice, nullptr);
    // Candidates pushing east off (10,7) are (12,7), (12,6) and (12,8);
    // (12,8) is occupied by our carrier, and straight back (12,7) is adjacent
    // to him. (12,6) is the only square that does not hand him our carrier.
    EXPECT_EQ(gs.getPlayer(12).position, (Position{12, 6}));
    EXPECT_EQ(takePushGeometryEvalsInSearch(), 1)
        << "the counter must tick exactly when the square was redirected";
}

TEST(PushGeometry, ClearsHimOffACornerOfOurCage) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    // Our carrier further away, with a STANDING corner at (13,6): straight
    // back (12,7) keeps the pushed man adjacent to that corner -- the dirty
    // corner the whole doctrine is about -- while (12,8) clears him.
    placePlayer(gs, 2, {14, 7}, TeamSide::HOME);
    gs.ball = BallState::carried({14, 7}, 2);
    placePlayer(gs, 3, {13, 6}, TeamSide::HOME);
    PushArmOn arm(TeamSide::HOME);
    FixedDiceRoller dice({3, 3, 3, 3, 3, 3});    // PUSHED (+ zásoba na následné hody)
    BlockParams params{1, 12, false, false};
    resolveBlock(gs, params, dice, nullptr);
    const Position dest = gs.getPlayer(12).position;
    EXPECT_NE(dest, (Position{12, 7})) << "straight back leaves the corner dirty";
    EXPECT_GT(std::max(std::abs(dest.x - 13), std::abs(dest.y - 6)), 1)
        << "the pushed man must stop being adjacent to our standing corner";
}

TEST(PushGeometry, ArmIsPerSideSoAnABCanRunItOnOneTeamOnly) {
    setPushGeometryArm(TeamSide::HOME, true);
    EXPECT_TRUE(pushGeometryArm(TeamSide::HOME));
    EXPECT_FALSE(pushGeometryArm(TeamSide::AWAY));
    setPushGeometryArm(TeamSide::HOME, false);
    EXPECT_FALSE(pushGeometryArm(TeamSide::HOME));
}

// ============================================================================
// Block + Wrestle na JEDNOM hráči -- l. 8672-8676 "even if one or both have the
// Block skill". Do 24.08.2026 to bylo zadratovane: utocnik s Blockem Wrestle
// nepouzil nikdy. Kvuli teto kombinaci se o Wrestle u Longbearda uvazuje.
// ============================================================================

TEST(BlockHandler, AttackerWithBlockAndWrestleUsesWrestleAgainstABlockOpponent) {
    // oba maji Block => Both Down by neudelal NIC. Wrestle je jediny zpusob,
    // jak souperě polozit -- a to je ta "aktivni" varianta v utoku.
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    gs.getPlayer(1).skills.add(SkillName::Block);
    gs.getPlayer(1).skills.add(SkillName::Wrestle);
    gs.getPlayer(12).skills.add(SkillName::Block);
    FixedDiceRoller dice({2});   // BOTH DOWN
    BlockParams params{1, 12, false, false};
    auto result = resolveBlock(gs, params, dice, nullptr);
    EXPECT_FALSE(result.turnover);                       // nedrzi mic
    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::PRONE);
    EXPECT_EQ(gs.getPlayer(12).state, PlayerState::PRONE);
}

TEST(BlockHandler, AttackerWithBlockKeepsBlockAgainstANonBlockOpponent) {
    // druha strana volby: souper Block nema, takze Both Down ho slozi I S HODEM
    // NA ZBROJ a utocnik zustane stat. Wrestle by byl horsi -- nesmi se pouzit.
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    gs.getPlayer(1).skills.add(SkillName::Block);
    gs.getPlayer(1).skills.add(SkillName::Wrestle);
    FixedDiceRoller dice({2, 3, 3, 4, 4});   // BD + hod na zbroj souperě
    BlockParams params{1, 12, false, false};
    auto result = resolveBlock(gs, params, dice, nullptr);
    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::STANDING);
    EXPECT_EQ(gs.getPlayer(12).state, PlayerState::PRONE);
}

TEST(BlockHandler, AttackerWithBlockAndWrestleTakesDownTheCarrier) {
    // souper Block nema, ale NESE MIC => slozit ho bez hodu na zbroj se vyplati
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    gs.getPlayer(1).skills.add(SkillName::Block);
    gs.getPlayer(1).skills.add(SkillName::Wrestle);
    gs.ball.isHeld = true; gs.ball.carrierId = 12; gs.ball.position = {11, 7};
    FixedDiceRoller dice({2, 3, 4, 5, 6, 1, 2, 3});
    BlockParams params{1, 12, false, false};
    auto result = resolveBlock(gs, params, dice, nullptr);
    EXPECT_FALSE(result.turnover);                       // mic nesl SOUPER
    EXPECT_EQ(gs.getPlayer(12).state, PlayerState::PRONE);
    EXPECT_FALSE(gs.ball.isHeld);                        // mic se uvolnil
}

TEST(BlockHandler, AttackerWithTheBallDoesNotWrestleItAway) {
    // l. 8677-8678 + F11: polozeni nosice aktivniho tymu JE turnover, takze
    // utocnik s Blockem, ktery nese mic, Wrestle volit nesmi.
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    gs.getPlayer(1).skills.add(SkillName::Block);
    gs.getPlayer(1).skills.add(SkillName::Wrestle);
    gs.getPlayer(12).skills.add(SkillName::Block);
    gs.ball.isHeld = true; gs.ball.carrierId = 1; gs.ball.position = {10, 7};
    FixedDiceRoller dice({2});
    BlockParams params{1, 12, false, false};
    auto result = resolveBlock(gs, params, dice, nullptr);
    EXPECT_FALSE(result.turnover);
    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::STANDING);  // Block, ne Wrestle
}

TEST(BlockHandler, DefenderWithBlockKeepsBlockUnlessTheCarrierIsBlocking) {
    // obrance s Blockem: Both Down slozi utocnika a on zustane stat -- Wrestle
    // by byl horsi. Do 24.08. ho pouzival VZDY.
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    gs.getPlayer(12).skills.add(SkillName::Block);
    gs.getPlayer(12).skills.add(SkillName::Wrestle);
    FixedDiceRoller dice({2, 3, 3, 4, 4});
    BlockParams params{1, 12, false, false};
    auto result = resolveBlock(gs, params, dice, nullptr);
    EXPECT_EQ(gs.getPlayer(12).state, PlayerState::STANDING);
    EXPECT_TRUE(result.turnover);            // utocnik slozen = turnover
}

TEST(BlockHandler, WrestleMakesBothDownWorthMoreThanAPushAgainstABlockWall) {
    // PROLOMENI ZDI: obrance ma Block, takze odsun ho jen posune a Both Down by
    // bez Wrestle neudelal nic. S Wrestle je Both Down JEDINA cesta, jak ho
    // slozit -- vybirac to musi ocenit vys nez PUSHED.
    Player att; att.skills.add(SkillName::Block); att.skills.add(SkillName::Wrestle);
    Player def; def.skills.add(SkillName::Block);
    BlockDiceFace faces[2] = {BlockDiceFace::PUSHED, BlockDiceFace::BOTH_DOWN};
    EXPECT_EQ(autoChooseBlockDie(faces, 2, true, att, def), BlockDiceFace::BOTH_DOWN);
}

TEST(BlockHandler, WithoutWrestleAPushStillBeatsBothDownAgainstABlockWall) {
    // druha strana: bez Wrestle je Both Down proti Blocku mrtva kostka
    Player att; att.skills.add(SkillName::Block);
    Player def; def.skills.add(SkillName::Block);
    BlockDiceFace faces[2] = {BlockDiceFace::PUSHED, BlockDiceFace::BOTH_DOWN};
    EXPECT_EQ(autoChooseBlockDie(faces, 2, true, att, def), BlockDiceFace::PUSHED);
}

TEST(BlockHandler, DefenderDownStillBeatsAWrestledBothDown) {
    // Wrestle nesmi prebit cistou vyhru -- utocnik pri nem lezi taky
    Player att; att.skills.add(SkillName::Block); att.skills.add(SkillName::Wrestle);
    Player def; def.skills.add(SkillName::Block);
    BlockDiceFace faces[2] = {BlockDiceFace::DEFENDER_DOWN, BlockDiceFace::BOTH_DOWN};
    EXPECT_EQ(autoChooseBlockDie(faces, 2, true, att, def), BlockDiceFace::DEFENDER_DOWN);
}

TEST(BlockHandler, BothDownKnocksBothPlayersDownInPlaceWithNoPushOrFollowUp) {
    // ⛔⛔ N8 (24.08.2026), nalezl Fable audit pohybu. BB2016 l. 514-519:
    // "BOTH DOWN: **Both players are Knocked Down**, unless one or both of the
    // players involved has the Block skill." Zadny odsun, zadny follow-up.
    // Srovnej l. 530-533 (DEFENDER DOWN): "pushed back and then Knocked Down
    // in the square they are moved to. The attacking player may follow up."
    // Nas kod delal na Both Down to druhe -- posouval DVA hráče o pole.
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::Block);   // utocnik neplada
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);   // obrance bez Blocku

    FixedDiceRoller dice({2, 3, 3, 1, 2});   // BD; zbroj 6 <= AV8; (zraneni)
    BlockParams params{1, 12, false, false};
    auto result = resolveBlock(gs, params, dice, nullptr);

    EXPECT_EQ(gs.getPlayer(12).state, PlayerState::PRONE);
    EXPECT_EQ(gs.getPlayer(12).position, (Position{11, 7}))
        << "obrance pada NA MISTE, neodsouva se";
    EXPECT_EQ(gs.getPlayer(1).position, (Position{10, 7}))
        << "a utocnik ho nenasleduje";
    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::STANDING);
    EXPECT_FALSE(result.turnover);
}
