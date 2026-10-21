#include <gtest/gtest.h>
#include "bb/block_handler.h"
#include "bb/helpers.h"
#include "bb/macro_actions.h"   // M1/N10 arm
#include "bb/rules_engine.h"
#include "bb/action_resolver.h"
#include "bb/coach_choice.h"   // P225: kouč dál dostává otázku Stand Firm

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

// P194 (6), 13.10.2026 -- oba testy dřív stavěly stav, který ve hře nenastane
// (STOJÍCÍ obránce na poli s volným míčem), a hlídaly na něm ZVEDNUTÍ s turnoverem
// při neúspěchu. Skutečná cesta, kterou míč na uvolněné pole doputuje dřív, než
// tam útočník dojde: odtlačený vstoupí na pole s míčem a míč se odrazí zpět
// (ř. 605-607). O follow-upu se ale rozhoduje před tím hodem (ř. 609-610), útočník
// tam tedy už stojí a míč CHYTÁ (ř. 856-857); nechycený odražený míč není turnover.
TEST(BlockHandler, FollowUpOntoTheSquareTheBallBouncedBackToIsACatch) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    gs.ball = BallState::onGround({12, 7});
    // Pushed (3): obránce na (12,7), kde leží míč => odraz D8 = 7 (západ) -> (11,7).
    // Útočník následuje a chytá: AG3 4+, -1 za zónu obránce = 5+; hod 5 stačí.
    FixedDiceRoller dice({3, 7, 5});
    BlockParams params{1, 12, false, false};
    std::vector<GameEvent> evs;
    auto result = resolveBlock(gs, params, dice, &evs);
    EXPECT_EQ(gs.getPlayer(1).position, (Position{11, 7}));
    EXPECT_TRUE(gs.ball.isHeld);
    EXPECT_EQ(gs.ball.carrierId, 1);
    EXPECT_FALSE(result.turnover);
    for (const auto& e : evs) EXPECT_NE(e.type, GameEvent::Type::PICKUP);
}

TEST(BlockHandler, FollowUpOntoTheSquareTheBallBouncedBackToFailedCatchIsNoTurnover) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    gs.ball = BallState::onGround({12, 7});
    // Totéž, hod 4 na chycení (5+) nestačí -- zvednutí (3+1 = 4+) by prošlo.
    // Míč se odráží dál: D8 = 1 (sever) -> (11,6).
    FixedDiceRoller dice({3, 7, 4, 1});
    BlockParams params{1, 12, false, false};
    auto result = resolveBlock(gs, params, dice, nullptr);
    EXPECT_EQ(gs.getPlayer(1).position, (Position{11, 7}));
    EXPECT_FALSE(gs.ball.isHeld);
    EXPECT_EQ(gs.ball.position, (Position{11, 6}));
    EXPECT_FALSE(result.turnover);
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

    // P100: dřív se tvrzení o brnění přeskočilo, když událost nevznikla.
    int armourBreaks = 0;
    for (auto& e : events) {
        if (e.type == GameEvent::Type::ARMOR_BREAK) {
            ++armourBreaks;
            EXPECT_EQ(e.roll, 9) << "unspent Mighty Blow must not inflate the armour roll";
        }
    }
    EXPECT_EQ(armourBreaks, 1);
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

    // P100: dřív všechna tvrzení jen uvnitř smyčky — bez událostí test neověřil nic.
    int armourBreaks = 0, injuries = 0;
    for (auto& e : events) {
        if (e.type == GameEvent::Type::ARMOR_BREAK) {
            ++armourBreaks;
            EXPECT_EQ(e.roll, 9) << "the +1 is what took 8 past AV8";
            EXPECT_TRUE(e.success);
        }
        if (e.type == GameEvent::Type::INJURY) {
            ++injuries;
            EXPECT_EQ(e.roll, 8) << "Mighty Blow was already spent on the armour";
        }
    }
    EXPECT_EQ(armourBreaks, 1);
    EXPECT_EQ(injuries, 1);
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

// P199 (16.10.2026), ř. 8496-8498: „If the score is less than or equal to the victim's Armour value
// then the attack has no effect.“ Neprobodnutý nosič stojí dál a míč drží. Dřív se míč pustil vždy.
TEST(BlockHandler, P199StabWithNoEffectLeavesTheBallWithTheCarrier) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::Stab);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    gs.ball = BallState::carried({11, 7}, 12);
    FixedDiceRoller dice({2, 3, 4, 4, 4, 4, 4, 4});   // zbroj 5 ≤ AV8 ⇒ bez účinku
    BlockParams params{1, 12, false, false};
    resolveBlock(gs, params, dice, nullptr);
    EXPECT_EQ(gs.getPlayer(12).state, PlayerState::STANDING);
    EXPECT_TRUE(gs.ball.isHeld) << "bodnutí bez účinku míč nevyrazí";
    EXPECT_EQ(gs.ball.carrierId, 12);
}

// Tentýž hod, ale probodnutý (Stunned): míč se odrazí až po hodu na zranění, z jeho pole.
TEST(BlockHandler, P199StabbedCarrierDropsTheBall) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::Stab);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    gs.ball = BallState::carried({11, 7}, 12);
    FixedDiceRoller dice({5, 4, 3, 3, 4, 4, 4, 4});   // zbroj 9 > 8, zranění 6 ⇒ Stunned, pak odraz
    BlockParams params{1, 12, false, false};
    resolveBlock(gs, params, dice, nullptr);
    EXPECT_EQ(gs.getPlayer(12).state, PlayerState::STUNNED);
    EXPECT_FALSE(gs.ball.isHeld && gs.ball.carrierId == 12) << "omráčený míč neudrží";
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

// Audit parity 08.10. bod (b), uživatel 16.10.2026 (ověřeno proti textu): Dauntless v Multiple Block
// porovnává s obráncem VČETNĚ +2 (ř. 8299-8301 „each defender's strength is increased by 2“ je modifikátor),
// ale PŘED asistencemi (ř. 8033-8035). ST3 proti ST3+2 = 5: hod 3 ⇒ 3 + 3 = 6 > 5 ⇒ útočník má sílu 5.
// Kdyby se +2 nepočítalo, Dauntless by se nehodil vůbec (3 není víc než 3); kdyby se počítala asistence
// druhého obránce (5 + 1 = 6), hod 3 by neprošel.
TEST(BlockHandler, DauntlessInMultipleBlockCountsThePlusTwoButNotAssists) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);                 // ST3
    gs.getPlayer(1).skills.add(SkillName::MultipleBlock);
    gs.getPlayer(1).skills.add(SkillName::Dauntless);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);                // ST3 (+2), druhý obránce mu asistuje
    placePlayer(gs, 13, {11, 8}, TeamSide::AWAY);
    FixedDiceRoller dice({3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1});
    std::vector<GameEvent> events;
    resolveMultipleBlock(gs, 1, 12, 13, dice, &events);
    const GameEvent* d = nullptr;
    for (const auto& e : events)
        if (e.type == GameEvent::Type::SKILL_USED && e.roll == static_cast<int>(SkillName::Dauntless)) { d = &e; break; }
    ASSERT_NE(d, nullptr) << "proti ST 3 + 2 se Dauntless hází";
    EXPECT_TRUE(d->success) << "3 + 3 > 5 — asistence se do porovnání nepočítá";
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
// ⭐ M1/N10 NASAZENO 07.09.2026 -- rameno `setBlitzContinuationArm` odebrano,
// vsechny tri pulky jsou PRODUKCE. Noc 27.->28.08. (+0,0177 +- 0,0069, >2,5
// sigma, 6/6 predpovedi). Duvod odebrani je tyz jako u P35 (01.09.) a M13
// (02.09.): default-OFF vypinac u dolozene prospesne pravidlove opravy uz
// nema co hlidat.
//
// S ramenem odesla i RAII pomucka `BlitzContinuationArmOn` a test
// WithTheArmOffTheBlitzBehavesExactlyAsBefore -- ten hlidal VYPNUTY vypinac
// (aktivace zavrena, follow-up vzdy) a se zrusenim vypinace ztratil predmet;
// stara vetev uz neexistuje, takze nemuze zestarnout. Testy nize hlidaji SAMA
// PRAVIDLA a plati beze zmeny.

// M1c/T5.29 (25.08.2026): l. 608-611 make the follow-up the coach's decision,
// and we always took it. For a blitzer whose activation is still open that is
// not a free square, it is a shove deeper into contact -- he lands next to the
// very player he just pushed, and only then may he withdraw.
TEST(BlockHandler, BlitzerWithMovementLeftDeclinesAFollowUpIntoMoreTacklezones) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).movementRemaining = 4;
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);

    FixedDiceRoller dice({3});   // PUSHED: defender to (12,7), square (11,7) freed
    BlockParams params{1, 12, true, false};
    resolveBlock(gs, params, dice, nullptr);

    EXPECT_EQ(gs.getPlayer(12).position, (Position{12, 7}));
    EXPECT_EQ(gs.getPlayer(1).position, (Position{10, 7}))
        << "following up would put him back in the pushed defender's tacklezone";
    EXPECT_FALSE(gs.getPlayer(1).hasActed);
}

// The mirror: a Block Action IS the whole activation, so there is nothing to
// save the movement for and the free square is simply taken.
TEST(BlockHandler, BlockActionStillTakesTheFreeFollowUp) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).movementRemaining = 4;
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);

    FixedDiceRoller dice({3});
    BlockParams params{1, 12, false, false};   // Block Action, not a Blitz
    resolveBlock(gs, params, dice, nullptr);

    EXPECT_EQ(gs.getPlayer(1).position, (Position{11, 7}));
}

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
// P9 / P9c (2026-08-18, nasazeno 08.09.2026): the push DESTINATION is chosen,
// not just taken. Unconditional now -- see bb/block_handler.h for motivation
// and corpus numbers. These pin the two defects that were actually measured,
// plus the guarantee that the counter only ticks when the rule really
// redirected the pick from straight-back.

TEST(PushGeometry, DoesNotShoveHimNextToOurOwnCarrier) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    placePlayer(gs, 2, {12, 8}, TeamSide::HOME);
    gs.ball = BallState::carried({12, 8}, 2);   // our carrier one step away
    takePushGeometryEvalsInSearch();            // reset counter before this test
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
    FixedDiceRoller dice({3, 3, 3, 3, 3, 3});    // PUSHED (+ zásoba na následné hody)
    BlockParams params{1, 12, false, false};
    resolveBlock(gs, params, dice, nullptr);
    const Position dest = gs.getPlayer(12).position;
    EXPECT_NE(dest, (Position{12, 7})) << "straight back leaves the corner dirty";
    EXPECT_GT(std::max(std::abs(dest.x - 13), std::abs(dest.y - 6)), 1)
        << "the pushed man must stop being adjacent to our standing corner";
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

// --- FOLLOW_UP event (28.08.2026) -------------------------------------------
//
// Follow-up used to set `att.position = defOldPos` and emit nothing, so a
// corpus could not tell "nobody follows up" from "following up is not logged".
// The pre-registration of the 27.->28.08. night asked how often the follow-up
// is REFUSED and the question was unanswerable for that reason alone -- the
// same class as STAND_UP before 21.08.
//
// ⭐ The event fires on the OPPORTUNITY, not on the choice: `success` carries
// whether the follow-up was taken. Without that the log would hold a numerator
// with no denominator, and "refused" would be indistinguishable from "there was
// nothing to refuse" -- which is exactly how the first reading on 28.08. went
// wrong (it divided by every block, though a defender vacates in only 80 % of
// blitzes).
static int countFollowUps(const std::vector<GameEvent>& ev, bool taken) {
    int n = 0;
    for (const auto& e : ev) {
        if (e.type == GameEvent::Type::FOLLOW_UP && e.success == taken) ++n;
    }
    return n;
}

TEST(BlockHandler, FollowUpTakenIsLogged) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).movementRemaining = 4;
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);

    std::vector<GameEvent> ev;
    FixedDiceRoller dice({3});                 // PUSHED
    // ⭐ 07.09.: byl to BLITZ „s vypnutym ramenem => nasleduje vzdy". Rameno
    // padlo (M1/N10 nasazeno) a blitzujici s pohybem by tuhle nabidku ted
    // ODMITL -- coz je predmet testu o dva niz. Povinny follow-up se tedy
    // bere tam, kde ho pravidla POVINNY nechavaji: blok jako CELA aktivace
    // (l. 608-611 nedava co setrit). Predmet testu -- ZAPIS udalosti
    // FOLLOW_UP vcetne from/to -- je tyz.
    BlockParams params{1, 12, false, false};   // Block Action => always follows
    resolveBlock(gs, params, dice, &ev);

    ASSERT_EQ(countFollowUps(ev, true), 1);
    ASSERT_EQ(countFollowUps(ev, false), 0);
    for (const auto& e : ev) {
        if (e.type != GameEvent::Type::FOLLOW_UP) continue;
        EXPECT_EQ(e.playerId, 1);
        EXPECT_EQ(e.targetId, 12);
        EXPECT_EQ(e.from, (Position{10, 7})) << "from = where the attacker stood";
        EXPECT_EQ(e.to, (Position{11, 7})) << "to = the square the defender left";
    }
    EXPECT_EQ(gs.getPlayer(1).position, (Position{11, 7}));
}

TEST(BlockHandler, FollowUpDeclinedIsLoggedToo) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).movementRemaining = 4;
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);

    std::vector<GameEvent> ev;
    FixedDiceRoller dice({3});                 // PUSHED: (11,7) is freed
    BlockParams params{1, 12, true, false};
    resolveBlock(gs, params, dice, &ev);

    // The blitzer stays put -- and the log still records that he COULD have.
    EXPECT_EQ(gs.getPlayer(1).position, (Position{10, 7}));
    EXPECT_EQ(countFollowUps(ev, false), 1) << "the refusal must be countable";
    EXPECT_EQ(countFollowUps(ev, true), 0);
}

TEST(BlockHandler, NoFollowUpEventWhenTheDefenderNeverVacated) {
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    gs.getPlayer(12).skills.add(SkillName::StandFirm);

    std::vector<GameEvent> ev;
    FixedDiceRoller dice({3});                 // PUSHED, but Stand Firm holds
    BlockParams params{1, 12, false, false};
    resolveBlock(gs, params, dice, &ev);

    EXPECT_EQ(gs.getPlayer(12).position, (Position{11, 7}));
    EXPECT_EQ(countFollowUps(ev, true) + countFollowUps(ev, false), 0)
        << "no square was freed, so there was no opportunity to log";
}

// ============================================================================
// L6 (29.08.2026) -- FRENZY + MULTIPLE BLOCK: PRAVIDLO ZAKAZUJE KOMBINACI,
// KOD BRAL VOLBU.
// r. 8302-8305: "The player cannot follow up either block when using this
// skill, so Multiple Block can be used INSTEAD OF Frenzy, but both skills
// cannot be used TOGETHER."
// ⇒ MIT se smi obojí, POUZIT v jednom okamziku jen jedno. Duvod je mechanicky:
// Multiple Block zakazuje follow-up, Frenzy ho vyzaduje (r. 8138).
//
// `rules_engine.cpp:222` mel `&& !p.hasSkill(Frenzy)`, takze hrac s obojim
// nedostal MULTIPLE_BLOCK NIKDY -- to je odebrana volba, ne zakazana
// kombinace. Nositel obojiho v `roster.cpp` existuje (r. 7450), jen ne mezi
// peti korpusovymi sestavami.
// ============================================================================

TEST(BlockHandler, L6MultipleBlockIsOfferedToAPlayerWhoAlsoHasFrenzy) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    gs.activeTeam = TeamSide::HOME;
    gs.homeTeam.side = TeamSide::HOME;
    gs.awayTeam.side = TeamSide::AWAY;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME, 6, 5, 3, 9);
    gs.getPlayer(1).skills.add(SkillName::MultipleBlock);
    gs.getPlayer(1).skills.add(SkillName::Frenzy);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    placePlayer(gs, 13, {11, 8}, TeamSide::AWAY);

    std::vector<Action> actions;
    getAvailableActions(gs, actions);

    bool offered = false;
    for (const auto& a : actions) {
        if (a.type == ActionType::MULTIPLE_BLOCK && a.playerId == 1) offered = true;
    }
    EXPECT_TRUE(offered);
}

// HRANICE: hrac BEZ Multiple Block ji nedostane, at ma Frenzy nebo ne.
TEST(BlockHandler, L6MultipleBlockIsStillNotOfferedWithoutTheSkill) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    gs.activeTeam = TeamSide::HOME;
    gs.homeTeam.side = TeamSide::HOME;
    gs.awayTeam.side = TeamSide::AWAY;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME, 6, 5, 3, 9);
    gs.getPlayer(1).skills.add(SkillName::Frenzy);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    placePlayer(gs, 13, {11, 8}, TeamSide::AWAY);

    std::vector<Action> actions;
    getAvailableActions(gs, actions);
    for (const auto& a : actions) {
        EXPECT_NE(a.type, ActionType::MULTIPLE_BLOCK);
    }
}

// A druha pulka pravidla: kdyz uz Multiple Block bezi, Frenzy se NESMI zapnout.
// Obranci maji Stand Firm, takze po PUSHED nikdo nikam nejde a oba zustavaji
// stat a sousedit -- presne stav, ve kterem Frenzy povinny druhy blok hazi.
// Musi vyjit PRESNE DVA bloky, ne tri nebo ctyri.
TEST(BlockHandler, L6FrenzyDoesNotAddBlocksInsideAMultipleBlock) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME, 6, 8, 3, 9);   // ST8, at ma 2 kostky
    gs.getPlayer(1).skills.add(SkillName::MultipleBlock);
    gs.getPlayer(1).skills.add(SkillName::Frenzy);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    placePlayer(gs, 13, {11, 8}, TeamSide::AWAY);
    gs.getPlayer(12).skills.add(SkillName::StandFirm);        // po PUSHED nikam
    gs.getPlayer(13).skills.add(SkillName::StandFirm);

    // 3 = PUSHED (dice.h:21). ST8 proti ST3+2 => 2 kostky pro utocnika, oba
    // padnou na PUSHED, Stand Firm odsun zrusi => oba stoji a sousedi, tedy
    // presne spoustec povinneho druheho bloku Frenzy. Dost kostek i pro rany,
    // ktere se hazet NEMAJI.
    FixedDiceRoller dice({3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3});
    std::vector<GameEvent> events;
    resolveMultipleBlock(gs, 1, 12, 13, dice, &events);

    int blocks = 0;
    for (const auto& e : events) if (e.type == GameEvent::Type::BLOCK) ++blocks;
    EXPECT_EQ(blocks, 2);
}

// ⭐⭐ 21.09.2026: `rules_bb2016.txt` r. 8385-8387 (Pro):
//   "On a roll of 1, 2 or 3 the original result stands and **may not be
//   re-rolled with a skill or team re-roll**; however you can re-roll the
//   Pro roll with a Team re-roll."
// ⇒ Kdyz hod Pro nevyjde, tymovy prehoz smi prehodit HOD PRO, ne kostky bloku.
//   Do 21.09. C++ po neuspesnem Pro prehodil rovnou kostky bloku.
TEST(BlockHandler, TymovyPrehozPoNeuspesnemProJdeNaHODPRO) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::Pro);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    gs.getTeamState(TeamSide::HOME).rerolls = 2;

    // 1 = ATTACKER_DOWN (spatny vysledek), hod Pro 2 = nesmi,
    // tymovy prehoz HODU PRO: 3 = zase nesmi => plati puvodni ATTACKER_DOWN.
    // Pak hod na brneni utocnika 3+3.
    // S vadou by se trojkou prehodila KOSTKA BLOKU (3 = PUSHED) a utocnik by stal.
    FixedDiceRoller dice({1, 2, 3, 3, 3});
    BlockParams params{1, 12, false, false};
    resolveBlock(gs, params, dice, nullptr);

    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::PRONE)
        << "r. 8386: po neuspesnem Pro plati puvodni vysledek";
    EXPECT_EQ(gs.getTeamState(TeamSide::HOME).rerolls, 1)
        << "tymovy prehoz se spotreboval na prehozeni HODU PRO";
}

TEST(BlockHandler, TymovyPrehozHODUPROmuzeUspet) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::Pro);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    gs.getTeamState(TeamSide::HOME).rerolls = 2;

    // 1 = ATTACKER_DOWN, hod Pro 2 = nesmi, tymovy prehoz hodu Pro: 5 = smi,
    // nova kostka bloku 6 = DEFENDER_DOWN, brneni obrance 3+3.
    FixedDiceRoller dice({1, 2, 5, 6, 3, 3});
    BlockParams params{1, 12, false, false};
    resolveBlock(gs, params, dice, nullptr);

    EXPECT_EQ(gs.getPlayer(12).state, PlayerState::PRONE) << "prehozeny blok srazil obrance";
    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::STANDING);
    EXPECT_EQ(gs.getTeamState(TeamSide::HOME).rerolls, 1);
}

// ============================================================================
// P68 (30.09.2026) -- port PHP 3cec72e2 (E19, E20b).
// E19, r. 8574-8576: zakorenení konci, "when he is Knocked Down or Placed
//   Prone" -- HNED, ne az na zacatku jeho dalsiho kola. Do te doby drzel
//   srazeny Treeman v souperove kole pole i v retezovem odtlaceni.
// E20b, r. 1824-1825: "Only Extraordinary skills work when a player is Prone
//   or Stunned" -- lezici Stand Firm retez nezastavi.
// ============================================================================

TEST(BlockHandler, P68KnockedDownRootedPlayerStopsBeingRootedAtOnce) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME, 6, 5, 3, 8);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY, 2, 3, 1, 10);
    gs.getPlayer(12).skills.add(SkillName::TakeRoot);
    gs.getPlayer(12).rooted = true;
    gs.ball.isHeld = false;
    gs.ball.position = {0, 0};

    // DEFENDER DOWN na vsech kostkach; brneni 1+1 neprorazi
    FixedDiceRoller dice({6, 6, 6, 1, 1, 1, 1});
    BlockParams params{1, 12, false, false};
    resolveBlock(gs, params, dice, nullptr);

    ASSERT_EQ(gs.getPlayer(12).state, PlayerState::PRONE);
    EXPECT_FALSE(gs.getPlayer(12).rooted) << "srazeny zakorenený drzi koreny dal";
}

TEST(BlockHandler, P68ProneStandFirmDoesNotJamTheChain) {
    // Tyz rozestav jako ChainIntoStandFirmMovesNobody, jen ti tri se Stand
    // Firm LEZI. Retez je ma odtlacit, takze obrance odejde ze svého pole.
    GameState gs;
    gs.phase = GamePhase::PLAY;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME, 6, 5, 3, 8);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    for (int id : {13, 14, 15}) {
        placePlayer(gs, id, {12, 7 + (id - 14)}, TeamSide::AWAY);
        gs.getPlayer(id).skills.add(SkillName::StandFirm);
        gs.getPlayer(id).state = PlayerState::PRONE;
    }
    gs.ball.isHeld = false;
    gs.ball.position = {0, 0};

    FixedDiceRoller dice({3, 3, 3, 3, 3, 3, 3, 3});
    BlockParams params{1, 12, false, false};
    resolveBlock(gs, params, dice, nullptr);

    EXPECT_NE(gs.getPlayer(12).position, (Position{11, 7}))
        << "lezici Stand Firm retez zastavil";
}

TEST(BlockHandler, P68ProneRootedPlayerStillHoldsGround) {
    // Pozitivni kontrola opacnym smerem: zakorenení NENI Stand Firm. Kdo se
    // zakorenil vleze, drzi pole i vleze ("for any reason", r. 8578-8579).
    GameState gs;
    gs.phase = GamePhase::PLAY;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME, 6, 5, 3, 8);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    for (int id : {13, 14, 15}) {
        placePlayer(gs, id, {12, 7 + (id - 14)}, TeamSide::AWAY);
        gs.getPlayer(id).state = PlayerState::PRONE;
        gs.getPlayer(id).rooted = true;
    }
    gs.ball.isHeld = false;
    gs.ball.position = {0, 0};

    FixedDiceRoller dice({3, 3, 3, 3, 3, 3, 3, 3});
    BlockParams params{1, 12, false, false};
    resolveBlock(gs, params, dice, nullptr);

    EXPECT_EQ(gs.getPlayer(12).position, (Position{11, 7}));
}

// ============================================================================
// P75 (30.09.2026) -- port PHP fa2422ad, 78bf7601, 9e02a834. Chainsaw,
// r. 7996-8018: "if a player holding a chainsaw is Knocked Down FOR ANY
// REASON, the opposing coach is allowed to add 3 to his Armour roll" a "if an
// opponent knocks himself over when blocking the chainsaw player then add 3
// to his Armour roll". +3 se nesmi secist dvakrat (pila do nositele pily).
// ============================================================================

TEST(BlockHandler, P75KnockedDownChainsawHolderGivesPlusThreeArmour) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY, 6, 3, 3, 8);
    gs.getPlayer(12).skills.add(SkillName::Chainsaw);
    gs.ball.isHeld = false;
    gs.ball.position = {0, 0};

    // POW; brneni 3+3 = 6 (+3 = 9 > AV8); zraneni 3+3 = omracen
    FixedDiceRoller dice({6, 3, 3, 3, 3, 3, 3, 3, 3});
    BlockParams params{1, 12, false, false};
    resolveBlock(gs, params, dice, nullptr);

    EXPECT_EQ(gs.getPlayer(12).state, PlayerState::STUNNED) << "+3 za pilu chybi";
}

TEST(BlockHandler, P75AttackerWhoFallsOnTheChainsawGetsPlusThreeToo) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME, 6, 3, 3, 8);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    gs.getPlayer(12).skills.add(SkillName::Chainsaw);
    gs.homeTeam.rerolls = 0;
    gs.ball.isHeld = false;
    gs.ball.position = {0, 0};

    FixedDiceRoller dice({1, 3, 3, 3, 3, 3, 3, 3, 3});      // ATTACKER DOWN
    BlockParams params{1, 12, false, false};
    resolveBlock(gs, params, dice, nullptr);

    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::STUNNED);
}

TEST(BlockHandler, P75StabStaysUnmodifiedAgainstAChainsawHolder) {
    // Hlidaci test: Stab hazi brneni "unmodified", +3 za pilu se nepridava.
    GameState gs;
    gs.phase = GamePhase::PLAY;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::Stab);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY, 6, 3, 3, 8);
    gs.getPlayer(12).skills.add(SkillName::Chainsaw);
    gs.ball.isHeld = false;
    gs.ball.position = {0, 0};

    FixedDiceRoller dice({3, 3, 3, 3, 3, 3});
    BlockParams params{1, 12, false, false};
    resolveBlock(gs, params, dice, nullptr);

    EXPECT_EQ(gs.getPlayer(12).state, PlayerState::STANDING);
}

TEST(BlockHandler, P75ChainsawIntoAChainsawHolderIsPlusThreeNotSix) {
    // Hlidaci test: pila do nositele pily = +3 jednou.
    GameState gs;
    gs.phase = GamePhase::PLAY;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::Chainsaw);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY, 6, 3, 3, 9);
    gs.getPlayer(12).skills.add(SkillName::Chainsaw);
    gs.ball.isHeld = false;
    gs.ball.position = {0, 0};

    FixedDiceRoller dice({4, 3, 3, 3, 3, 3, 3});            // zasah; 6+3 = 9, ne > AV9
    BlockParams params{1, 12, false, false};
    resolveBlock(gs, params, dice, nullptr);

    EXPECT_EQ(gs.getPlayer(12).state, PlayerState::STANDING);
}

// Živá partie 02.10.2026: pole odtlačení a follow-up volí útočící kouč
// (BB2016 l. 608-611). Diagonální blok z (12,8) na (13,7), pole (14,7) obsazené:
// kandidáti (14,6) rovně a (13,6) bokem.
static GameState manualPushBoard() {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    placePlayer(gs, 1, {12, 8}, TeamSide::HOME);
    placePlayer(gs, 12, {13, 7}, TeamSide::AWAY);
    placePlayer(gs, 13, {14, 7}, TeamSide::AWAY);
    return gs;
}

// P100 (c): volby kouče jsou `thread_local` globály. Dřív je čistil jen konec těla testu
// (a jen u některých testů) — test, který skončil dřív (ASSERT, výjimka), nechal zvolené
// pole odtlačení viset a rozbil další test. Teď se čistí před každým testem i po něm.
class BlockHandlerManual : public ::testing::Test {
protected:
    void SetUp() override { clearManualBlockChoices(); }
    void TearDown() override { clearManualBlockChoices(); }
};

TEST_F(BlockHandlerManual, WithoutAChoiceTheEnginePushesStraightBack) {   // pozitivní kontrola
    GameState gs = manualPushBoard();
    clearManualBlockChoices();
    FixedDiceRoller dice({3});   // Pushed
    resolveBlock(gs, BlockParams{1, 12, false, false}, dice, nullptr);
    EXPECT_EQ(gs.getPlayer(12).position, (Position{14, 6}));
}

TEST_F(BlockHandlerManual, CoachPicksThePushSquareAndDeclinesTheFollowUp) {
    GameState gs = manualPushBoard();
    setManualPushChoice({13, 6});
    setManualFollowUp(false);
    FixedDiceRoller dice({3});
    resolveBlock(gs, BlockParams{1, 12, false, false}, dice, nullptr);
    EXPECT_EQ(gs.getPlayer(12).position, (Position{13, 6}));
    EXPECT_EQ(gs.getPlayer(1).position, (Position{12, 8})) << "bez follow-upu";
}

TEST_F(BlockHandlerManual, AChoiceTheRulesForbidIsIgnored) {
    GameState gs = manualPushBoard();
    setManualPushChoice({14, 7});   // obsazené, zatímco jiná pole jsou volná
    FixedDiceRoller dice({3});
    resolveBlock(gs, BlockParams{1, 12, false, false}, dice, nullptr);
    EXPECT_EQ(gs.getPlayer(12).position, (Position{14, 6}));
    EXPECT_EQ(gs.getPlayer(13).position, (Position{14, 7}));
    clearManualBlockChoices();
}

// Blitz: kouč volí i pole, ODKUD se blokuje. DBG9 (11,10) na WW21 (13,10),
// WL14 na (14,10); z (12,11) je odtlačení na (14,9) rovně nebo (13,9) bokem.
static GameState manualBlitzBoard() {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    gs.activeTeam = TeamSide::HOME;
    placePlayer(gs, 9, {11, 10}, TeamSide::HOME, 5);
    placePlayer(gs, 21, {13, 10}, TeamSide::AWAY);
    placePlayer(gs, 14, {14, 10}, TeamSide::AWAY);
    return gs;
}

TEST_F(BlockHandlerManual, CoachPicksTheBlitzSquareAndThePush) {
    GameState gs = manualBlitzBoard();
    setManualBlitzSquare({12, 11});
    setManualPushChoice({13, 9});
    setManualFollowUp(true);
    FixedDiceRoller dice(std::vector<int>(20, 3));   // Pushed, žádné jiné hody
    executeAction(gs, Action{ActionType::BLITZ, 9, 21, {13, 10}}, dice, nullptr);
    EXPECT_EQ(gs.getPlayer(21).position, (Position{13, 9}));
    EXPECT_EQ(gs.getPlayer(9).position, (Position{13, 10})) << "z (12,11) follow-up na uvolněné pole";
    clearManualBlockChoices();
}

TEST_F(BlockHandlerManual, WithoutABlitzSquareTheEngineChoosesItsOwn) {   // pozitivní kontrola
    GameState gs = manualBlitzBoard();
    clearManualBlockChoices();
    setManualFollowUp(false);
    FixedDiceRoller dice(std::vector<int>(20, 3));
    executeAction(gs, Action{ActionType::BLITZ, 9, 21, {13, 10}}, dice, nullptr);
    EXPECT_NE(gs.getPlayer(9).position, (Position{12, 11})) << "engine by z (12,11) neblokoval";
    clearManualBlockChoices();
}

// P165 (07.10.2026) — Side Step, ř. 8474-8476: "the coach may choose to move the player to ANY
// adjacent square, not just the three squares shown on the Push Back diagram." Hráč se Side Step
// na kraji hřiště, tlačený kolmo ven: všechna tři pole z diagramu jsou mimo hřiště, ale vedle
// něj na krajní řadě je volno ⇒ do davu nejde.
TEST(BlockHandler, SideStepAtTheSidelineStepsAlongItInsteadOfIntoTheCrowd) {
    auto pushed = [](bool sideStep) {
        GameState gs;
        gs.phase = GamePhase::PLAY;
        placePlayer(gs, 1, {11, 1}, TeamSide::HOME, 6, 4, 3, 8);
        placePlayer(gs, 12, {11, 0}, TeamSide::AWAY);
        if (sideStep) gs.getPlayer(12).skills.add(SkillName::SideStep);
        gs.ball.isHeld = false;
        gs.ball.position = {0, 7};
        FixedDiceRoller dice({3, 3, 3, 3, 3, 3, 3, 3});   // 2 kostky, obě PUSHED; pak případné zranění v davu
        BlockParams params{1, 12, false, false};
        resolveBlock(gs, params, dice, nullptr);
        return gs.getPlayer(12);
    };
    {
        const Player p = pushed(true);
        EXPECT_TRUE(p.isOnPitch()) << "se Side Step zůstává na hřišti";
        EXPECT_EQ(p.position.y, 0);
        EXPECT_EQ(std::abs(p.position.x - 11), 1) << "ukročil po krajní řadě";
    }
    {   // pozitivní kontrola: bez Side Step táž rána znamená dav
        const Player p = pushed(false);
        EXPECT_FALSE(p.isOnPitch());
    }
}

// ---------------------------------------------------------------------------
// P165 (08.10.2026) — LONGBEARD S BLOCK I WRESTLE (jediný takový hráč v soupiskách TV1500;
// uživatel: „zaměř část testů speciálně na něj a na block jeho na někoho s block a bloky na něho“).
// Wrestle, ř. 8672-8678: smí se použít, když je vybráno Both Down, ať hráč blokuje nebo je
// blokován; oba jdou k zemi i přes Block, bez hodu na brnění; turnover jen když aktivní hráč
// držel míč. Je to VOLBA — tyto testy hlídají, kdy ji hráč s Block i Wrestle využije.
// Jeho rány (útok): AttackerWithBlockAndWrestle… výš. Tady rány NA něho.
// ---------------------------------------------------------------------------
namespace {
void makeLongbeardWithWrestle(GameState& gs, int id, Position pos, TeamSide side) {
    placePlayer(gs, id, pos, side, 4, 3, 2, 9);
    for (SkillName s : {SkillName::Block, SkillName::Tackle, SkillName::ThickSkull, SkillName::Wrestle}) {
        gs.getPlayer(id).skills.add(s);
    }
}
}  // namespace

TEST(BlockHandler, BlockWrestleDefenderKeepsBlockAgainstAnAttackerWithoutBlock) {
    // Both Down, útočník Block nemá: s Block obránce stojí a útočník padá (hod na brnění, turnover).
    // Wrestle by útočníka zachránil před brněním i turnoverem ⇒ nesmí se použít.
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    makeLongbeardWithWrestle(gs, 12, {11, 7}, TeamSide::AWAY);
    FixedDiceRoller dice({2, 1, 1, 1, 1});               // BOTH DOWN + hod na brnění útočníka
    BlockParams params{1, 12, false, false};
    auto result = resolveBlock(gs, params, dice, nullptr);
    EXPECT_TRUE(result.turnover);
    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::PRONE);
    EXPECT_EQ(gs.getPlayer(12).state, PlayerState::STANDING) << "Longbeard použil Block, ne Wrestle";
}

TEST(BlockHandler, BlockWrestleDefenderWrestlesDownAnAttackerWithBlock) {
    // PŘEPSÁNO 16.10.2026 — ROZHODL UŽIVATEL: „pokud má obránce wrestle a útočník block — tak obránce
    // použije“. Dřív tu stálo opačně (obránce zůstal stát, „bez Wrestle se nestane nic“). Both Down,
    // oba mají Block, míč nedrží nikdo: Wrestle položí oba, bez hodu na brnění, turnover to není
    // (ř. 8677-8678).
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::Block);
    makeLongbeardWithWrestle(gs, 12, {11, 7}, TeamSide::AWAY);
    FixedDiceRoller dice({2});
    BlockParams params{1, 12, false, false};
    auto result = resolveBlock(gs, params, dice, nullptr);
    EXPECT_FALSE(result.turnover);
    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::PRONE);
    EXPECT_EQ(gs.getPlayer(12).state, PlayerState::PRONE);
}

TEST(BlockHandler, BlockWrestleDefenderWrestlesDownAnAttackerWhoCarriesTheBall) {
    // Both Down, útočník s Block DRŽÍ MÍČ: Wrestle ho položí, míč je volný a je to turnover
    // (ř. 8677-8678 „unless the active player was holding the ball“).
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::Block);
    makeLongbeardWithWrestle(gs, 12, {11, 7}, TeamSide::AWAY);
    gs.ball = BallState::carried({10, 7}, 1);
    FixedDiceRoller dice({2, 4, 4, 4, 4, 4, 4});          // BOTH DOWN + odskok míče
    BlockParams params{1, 12, false, false};
    auto result = resolveBlock(gs, params, dice, nullptr);
    EXPECT_TRUE(result.turnover);
    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::PRONE);
    EXPECT_EQ(gs.getPlayer(12).state, PlayerState::PRONE);
    EXPECT_FALSE(gs.ball.isHeld && gs.ball.carrierId == 1) << "nosič míč pustil";
}

TEST(BlockHandler, BlockWrestleAttackerWrestlesDownABlockWrestleDefender) {
    // Longbeard proti Longbeardovi (oba Block + Wrestle), Both Down: útočník Wrestle použije
    // (soupeř má Block, jinak by se nestalo nic) ⇒ oba leží, bez brnění, bez turnoveru.
    GameState gs;
    makeLongbeardWithWrestle(gs, 1, {10, 7}, TeamSide::HOME);
    makeLongbeardWithWrestle(gs, 12, {11, 7}, TeamSide::AWAY);
    FixedDiceRoller dice({2});
    BlockParams params{1, 12, false, false};
    auto result = resolveBlock(gs, params, dice, nullptr);
    EXPECT_FALSE(result.turnover);
    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::PRONE);
    EXPECT_EQ(gs.getPlayer(12).state, PlayerState::PRONE);
}

TEST(BlockHandler, CarrierBlockingABlockWrestleDefenderNeverPicksBothDown) {
    // Uživatel 08.10.: „proč by nosič … vybral wrestle, aby ztratil míč?“ — nevybírá ho on.
    // Wrestle je volba OBRÁNCE (Longbearda s Block + Wrestle), a ta nastane jen tehdy, když
    // Both Down zůstane jako vybraná kostka. Útočník, který drží míč a kostku vybírá, ji proto
    // nikdy nevezme, má-li jinou: ze dvou kostek [Both Down, Pushed] bere Pushed.
    GameState gs;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME, 6, 4, 3, 8);      // ST4 ⇒ 2 kostky, vybírá útočník
    gs.getPlayer(1).skills.add(SkillName::Block);
    makeLongbeardWithWrestle(gs, 12, {11, 7}, TeamSide::AWAY);
    gs.ball = BallState::carried({10, 7}, 1);
    FixedDiceRoller dice({2, 3, 3, 3, 3});                         // Both Down, Pushed
    BlockParams params{1, 12, false, false};
    auto result = resolveBlock(gs, params, dice, nullptr);
    EXPECT_FALSE(result.turnover);
    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::STANDING);
    EXPECT_TRUE(gs.ball.isHeld && gs.ball.carrierId == 1) << "nosič míč drží dál";
    EXPECT_EQ(gs.getPlayer(12).state, PlayerState::STANDING) << "obránce byl jen odtlačen";
}

// ---------------------------------------------------------------------------
// NÁLEZ 3b (audit parity 08.10.2026) — DAV × ŘETĚZ. Pravidla ř. 639-651:
//   ř. 639     „The player must be pushed back into an empty square if possible.“
//   ř. 641-644 „If ALL SUCH SQUARES are occupied by other players, then the player is pushed into
//               an occupied square, and the player that originally occupied the square is pushed
//               back in turn.“
//   ř. 650-651 „Players must be pushed off the pitch if there are no eligible empty squares on
//               the pitch.“
// Pořadí je tedy: volné pole na hřišti > dav (některé ze tří polí je mimo hřiště a žádné volné)
// > řetěz (všechna tři pole jsou na hřišti a obsazená). Dřív šel hráč do davu jen tehdy, když
// byla mimo hřiště VŠECHNA tři pole; u lajny s obsazeným polem řetězil.
// Kostky: 3,3 = dvě Pushed (ST4 × ST3); zbytek jsou hody davu / vhazování.
// ---------------------------------------------------------------------------
TEST(BlockHandler, N3bPushAlongTheSidelineWithNoEmptySquareGoesIntoTheCrowd) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    placePlayer(gs, 1, {10, 0}, TeamSide::HOME, 6, 4, 3, 8);
    placePlayer(gs, 12, {11, 0}, TeamSide::AWAY);
    placePlayer(gs, 13, {12, 0}, TeamSide::AWAY);   // rovně za ním
    placePlayer(gs, 14, {12, 1}, TeamSide::AWAY);   // šikmo do hřiště; třetí pole (12,-1) je mimo
    gs.ball.isHeld = false;
    gs.ball.position = {0, 7};

    FixedDiceRoller dice({3, 3, 1, 1, 1, 1});       // zranění v davu 1+1 = omráčen ⇒ rezervy
    BlockParams params{1, 12, false, false};
    resolveBlock(gs, params, dice, nullptr);

    EXPECT_FALSE(gs.getPlayer(12).isOnPitch()) << "žádné volné pole a jedno mimo hřiště ⇒ dav";
    EXPECT_EQ(gs.getPlayer(13).position, (Position{12, 0})) << "řetěz se nekoná, soused stojí";
    EXPECT_EQ(gs.getPlayer(14).position, (Position{12, 1}));
}

TEST(BlockHandler, N3bDiagonalPushAtTheSidelineWithNoEmptySquareGoesIntoTheCrowd) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    placePlayer(gs, 1, {10, 1}, TeamSide::HOME, 6, 4, 3, 8);
    placePlayer(gs, 12, {11, 0}, TeamSide::AWAY);
    placePlayer(gs, 13, {12, 0}, TeamSide::AWAY);   // jediné ze tří polí na hřišti
    gs.ball.isHeld = false;
    gs.ball.position = {0, 7};

    FixedDiceRoller dice({3, 3, 1, 1, 1, 1});
    BlockParams params{1, 12, false, false};
    resolveBlock(gs, params, dice, nullptr);

    EXPECT_FALSE(gs.getPlayer(12).isOnPitch());
    EXPECT_EQ(gs.getPlayer(13).position, (Position{12, 0}));
}

// Hlídka druhé strany pořadí (ř. 639): volné pole na hřišti má přednost před davem.
TEST(BlockHandler, N3bAnEmptySquareOnThePitchStillBeatsTheCrowd) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    placePlayer(gs, 1, {10, 0}, TeamSide::HOME, 6, 4, 3, 8);
    placePlayer(gs, 12, {11, 0}, TeamSide::AWAY);
    placePlayer(gs, 13, {12, 0}, TeamSide::AWAY);   // rovně obsazeno, (12,1) volné
    gs.ball.isHeld = false;
    gs.ball.position = {0, 7};

    FixedDiceRoller dice({3, 3, 1, 1, 1, 1});
    BlockParams params{1, 12, false, false};
    resolveBlock(gs, params, dice, nullptr);

    EXPECT_EQ(gs.getPlayer(12).position, (Position{12, 1}));
    EXPECT_EQ(gs.getPlayer(13).position, (Position{12, 0}));
}

// Hlídka třetího stupně: všechna tři pole na hřišti a obsazená ⇒ řetěz, ne dav (ř. 641-644).
TEST(BlockHandler, N3bAllThreeSquaresOnThePitchAndOccupiedStillChains) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    placePlayer(gs, 1, {10, 1}, TeamSide::HOME, 6, 4, 3, 8);
    placePlayer(gs, 12, {11, 1}, TeamSide::AWAY);
    placePlayer(gs, 13, {12, 0}, TeamSide::AWAY);
    placePlayer(gs, 14, {12, 1}, TeamSide::AWAY);
    placePlayer(gs, 15, {12, 2}, TeamSide::AWAY);
    gs.ball.isHeld = false;
    gs.ball.position = {0, 7};

    FixedDiceRoller dice({3, 3, 1, 1, 1, 1});
    BlockParams params{1, 12, false, false};
    resolveBlock(gs, params, dice, nullptr);

    EXPECT_EQ(gs.getPlayer(12).position, (Position{12, 1}));
    EXPECT_EQ(gs.getPlayer(14).position, (Position{13, 1}));
}

// Nosič vytlačený do davu PODÉL lajny. Pravidla ř. 659-663: „…he is beaten up by the fans, who are
// more than happy to throw the ball back into play! The Throw-in template is centred on the last
// square the player was in before he was pushed off the pitch“ — šablona tedy míří od HORNÍ lajny
// do hřiště, ne od pole „rovně za ním“, které tu leží na hřišti. Vhazování D6 = 3 ⇒ kolmo do
// hřiště, 2D6 = 1+1 ⇒ JEDNO pole (první pole pohybu je (11,0) samo, FAQ ř. 9307-9314) ⇒ (11,1);
// prázdné ⇒ odraz D8 = 5 (jih) ⇒ (11,2). Pak zranění v davu.
TEST(BlockHandler, N3bCarrierSurfedAlongTheSidelineIsThrownInFromThatSideline) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    placePlayer(gs, 1, {10, 0}, TeamSide::HOME, 6, 4, 3, 8);
    placePlayer(gs, 12, {11, 0}, TeamSide::AWAY);
    placePlayer(gs, 13, {12, 0}, TeamSide::AWAY);
    placePlayer(gs, 14, {12, 1}, TeamSide::AWAY);
    gs.ball = BallState::carried({11, 0}, 12);

    FixedDiceRoller dice({3, 3, 3, 1, 1, 5, 1, 1, 1, 1});
    BlockParams params{1, 12, false, false};
    resolveBlock(gs, params, dice, nullptr);

    ASSERT_FALSE(gs.getPlayer(12).isOnPitch());
    EXPECT_FALSE(gs.ball.isHeld);
    EXPECT_EQ(gs.ball.position, (Position{11, 2}));
}

// ---------------------------------------------------------------------------
// STAND FIRM / ZAKOŘENĚNÍ UVNITŘ ŘETĚZU (review P186, M3). Pravidla ř. 8514-8516: „If a player is
// pushed back into a player with using Stand Firm then neither player moves.“ a ř. 644-646: řetězové
// odtlačení „is treated exactly like a normal push back as if the second player had been blocked
// by the first“. Když se tedy druhý v řetězu nemá kam hnout (všechna jeho pole drží Stand Firm),
// nehne se ani on, ani ten, kdo do něj byl tlačen, a útočník nemá kam následovat.
// Dřív: zaseknutí se hlídalo jen o jeden stupeň — druhý v řetězu zůstal stát a první byl přesto
// posunut NA JEHO POLE (dva hráči na jednom poli) a útočník následoval.
// ---------------------------------------------------------------------------
TEST(BlockHandler, ChainThatJamsOnStandFirmFurtherDownMovesNobody) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME, 6, 5, 3, 8);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    placePlayer(gs, 13, {12, 7}, TeamSide::AWAY);           // bez Stand Firm, ale nemá kam
    placePlayer(gs, 14, {12, 6}, TeamSide::AWAY);
    placePlayer(gs, 15, {12, 8}, TeamSide::AWAY);
    placePlayer(gs, 16, {13, 6}, TeamSide::AWAY);
    placePlayer(gs, 17, {13, 7}, TeamSide::AWAY);
    placePlayer(gs, 18, {13, 8}, TeamSide::AWAY);
    for (int id : {14, 15, 16, 17, 18}) gs.getPlayer(id).skills.add(SkillName::StandFirm);
    gs.ball.isHeld = false;
    gs.ball.position = {0, 0};

    FixedDiceRoller dice({3, 3, 3, 3, 3, 3});               // dvě Pushed
    BlockParams params{1, 12, false, false};
    resolveBlock(gs, params, dice, nullptr);

    EXPECT_EQ(gs.getPlayer(12).position, (Position{11, 7})) << "obránce byl posunut na obsazené pole";
    EXPECT_EQ(gs.getPlayer(13).position, (Position{12, 7}));
    EXPECT_EQ(gs.getPlayer(1).position, (Position{10, 7})) << "útočník nemá kam následovat";
    for (int i = 1; i <= GameState::PLAYERS_TOTAL; i++) {
        for (int j = i + 1; j <= GameState::PLAYERS_TOTAL; j++) {
            const Player& a = gs.getPlayer(i);
            const Player& b = gs.getPlayer(j);
            if (a.isOnPitch() && b.isOnPitch()) {
                EXPECT_NE(a.position, b.position) << "hráči " << i << " a " << j << " na jednom poli";
            }
        }
    }
}

// Totéž se zakořeněnými (ř. 8578-8579 „may not … be pushed back for any reason“).
TEST(BlockHandler, ChainThatJamsOnRootedPlayersFurtherDownMovesNobody) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME, 6, 5, 3, 8);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    placePlayer(gs, 13, {12, 7}, TeamSide::AWAY);
    placePlayer(gs, 14, {12, 6}, TeamSide::AWAY);
    placePlayer(gs, 15, {12, 8}, TeamSide::AWAY);
    placePlayer(gs, 16, {13, 6}, TeamSide::AWAY);
    placePlayer(gs, 17, {13, 7}, TeamSide::AWAY);
    placePlayer(gs, 18, {13, 8}, TeamSide::AWAY);
    for (int id : {14, 15, 16, 17, 18}) gs.getPlayer(id).rooted = true;
    gs.ball.isHeld = false;
    gs.ball.position = {0, 0};

    FixedDiceRoller dice({3, 3, 3, 3, 3, 3});
    BlockParams params{1, 12, false, false};
    resolveBlock(gs, params, dice, nullptr);

    EXPECT_EQ(gs.getPlayer(12).position, (Position{11, 7}));
    EXPECT_EQ(gs.getPlayer(13).position, (Position{12, 7}));
    EXPECT_EQ(gs.getPlayer(1).position, (Position{10, 7}));
}

// U lajny rozhoduje pořadí z ř. 639-651 (nález 3b): žádné volné pole a jedno mimo hřiště ⇒ dav,
// i když zbylá pole drží Stand Firm — „neither player moves“ platí jen tam, kde se do hráče se
// Stand Firm skutečně tlačí, tj. když jsou všechna tři pole na hřišti (shodně s PHP d3f2c3ad).
TEST(BlockHandler, StandFirmBehindTheDefenderAtTheSidelineDoesNotSaveHimFromTheCrowd) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    placePlayer(gs, 1, {10, 0}, TeamSide::HOME, 6, 4, 3, 8);
    placePlayer(gs, 12, {11, 0}, TeamSide::AWAY);
    placePlayer(gs, 13, {12, 0}, TeamSide::AWAY);
    placePlayer(gs, 14, {12, 1}, TeamSide::AWAY);
    gs.getPlayer(13).skills.add(SkillName::StandFirm);
    gs.getPlayer(14).skills.add(SkillName::StandFirm);
    gs.ball.isHeld = false;
    gs.ball.position = {0, 7};

    FixedDiceRoller dice({3, 3, 1, 1, 1, 1});
    BlockParams params{1, 12, false, false};
    resolveBlock(gs, params, dice, nullptr);

    EXPECT_FALSE(gs.getPlayer(12).isOnPitch());
    EXPECT_EQ(gs.getPlayer(13).position, (Position{12, 0}));
    EXPECT_EQ(gs.getPlayer(14).position, (Position{12, 1}));
}

// ---------------------------------------------------------------------------
// NÁLEZ 17 (audit parity 08.10.2026) — SIDE STEP VLEŽE V ŘETĚZU. Pravidla ř. 1824-1825: „Only
// Extraordinary skills work when a player is Prone or Stunned.“ Side Step (Agility) tedy ležícímu
// neplatí a směr řetězového odtlačení volí trenér týmu na tahu (ř. 647-649). Dřív si pole vybíral
// i ležící hráč se Side Step (týž princip jako P68 u Stand Firm).
// Rozestavení: obránce 12 je tlačen do 13 na (12,7); tři pole za 13 jsou obsazená, volná jsou jen
// (11,6) a (11,8) směrem K útočníkovi — tam smí jen Side Step. Bez něj řetěz pokračuje přes (13,7).
// ---------------------------------------------------------------------------
TEST(BlockHandler, N17ProneSideStepperInAChainDoesNotChooseHisSquare) {
    auto chained = [](PlayerState stateOf13) {
        GameState gs;
        gs.phase = GamePhase::PLAY;
        placePlayer(gs, 1, {10, 7}, TeamSide::HOME, 6, 5, 3, 8);
        placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
        placePlayer(gs, 13, {12, 7}, TeamSide::AWAY);
        gs.getPlayer(13).skills.add(SkillName::SideStep);
        gs.getPlayer(13).state = stateOf13;
        placePlayer(gs, 2, {12, 6}, TeamSide::HOME);
        placePlayer(gs, 3, {12, 8}, TeamSide::HOME);
        placePlayer(gs, 14, {13, 6}, TeamSide::AWAY);
        placePlayer(gs, 15, {13, 7}, TeamSide::AWAY);
        placePlayer(gs, 16, {13, 8}, TeamSide::AWAY);
        gs.ball.isHeld = false;
        gs.ball.position = {0, 0};
        FixedDiceRoller dice({3, 3, 3, 3, 3, 3});           // Pushed na všech kostkách
        BlockParams params{1, 12, false, false};
        resolveBlock(gs, params, dice, nullptr);
        return gs;
    };
    {
        const GameState gs = chained(PlayerState::PRONE);
        EXPECT_EQ(gs.getPlayer(13).position, (Position{13, 7})) << "ležící si pole nevybírá";
        EXPECT_EQ(gs.getPlayer(15).position, (Position{14, 7})) << "řetěz pokračuje rovně";
    }
    {
        const GameState gs = chained(PlayerState::STUNNED);
        EXPECT_EQ(gs.getPlayer(13).position, (Position{13, 7})) << "omráčený si pole nevybírá";
    }
    {   // pozitivní kontrola: STOJÍCÍ se Side Step v řetězu ukročí na volné pole a řetěz skončí
        const GameState gs = chained(PlayerState::STANDING);
        EXPECT_EQ(gs.getPlayer(13).position.x, 11);
        EXPECT_EQ(gs.getPlayer(15).position, (Position{13, 7}));
    }
}

// ---------------------------------------------------------------------------
// TÝŽ PRINCIP JAKO T5.26 — NOSIČ, KTERÝ PO SRAŽENÍ OPUSTÍ HŘIŠTĚ (KO / zranění). Pravidla
// ř. 678-681: „A player who is carrying the ball and who is knocked down or placed prone will drop
// the ball IN THE SQUARE WHERE THEY FALL. The dropped ball will bounce one square in a random
// direction after the player's armour and injury rolls (if any) are fully resolved.“ Dřív hod na
// zranění při KO / Casualty nastavil pozici na (−1,−1) a míč se pak pouštěl odtamtud.
// Kostky: Skull (1) ⇒ padá útočník-nosič na (10,7); brnění 5+4 = 9 > AV 8; zranění; D8 = 7 (západ)
// ⇒ míč na (9,7). Stará mechanika: z (−1,−1) na západ = mimo hřiště ⇒ vhazování.
// ---------------------------------------------------------------------------
TEST(BlockHandler, CarrierRemovedFromThePitchDropsTheBallWhereHeFell) {
    auto skull = [](int inj1, int inj2, std::vector<int> tail) {
        GameState gs;
        gs.phase = GamePhase::PLAY;
        placePlayer(gs, 1, {10, 7}, TeamSide::HOME, 6, 3, 3, 8);
        placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
        gs.ball = BallState::carried({10, 7}, 1);
        std::vector<int> rolls{1, 5, 4, inj1, inj2};
        rolls.insert(rolls.end(), tail.begin(), tail.end());
        FixedDiceRoller dice(rolls);
        BlockParams params{1, 12, false, false};
        resolveBlock(gs, params, dice, nullptr);
        EXPECT_EQ(dice.remaining(), 0u) << "jediný odraz, žádné vhazování";
        return gs;
    };
    {   // zranění 4+5 = 9 ⇒ KO
        const GameState gs = skull(4, 5, {7});
        ASSERT_EQ(gs.getPlayer(1).state, PlayerState::KO);
        EXPECT_FALSE(gs.ball.isHeld);
        EXPECT_EQ(gs.ball.position, (Position{9, 7}));
    }
    {   // zranění 6+5 = 11 ⇒ Casualty (tabulka zranění D6 = 3, D8 = 1; pak odraz)
        const GameState gs = skull(6, 5, {3, 1, 7});
        ASSERT_FALSE(gs.getPlayer(1).isOnPitch());
        EXPECT_FALSE(gs.ball.isHeld);
        EXPECT_EQ(gs.ball.position, (Position{9, 7}));
    }
    {   // pozitivní kontrola: zranění 3+4 = 7 ⇒ Stunned, zůstává na hřišti — míč stejně na (9,7)
        const GameState gs = skull(3, 4, {7});
        ASSERT_EQ(gs.getPlayer(1).state, PlayerState::STUNNED);
        EXPECT_EQ(gs.ball.position, (Position{9, 7}));
    }
}

// ---------------------------------------------------------------------------
// P194 (6), uživatel 10.10.2026: „ano chytá — nechytá pouze pokud nestojí nebo se
// pohnul ne voluntarily — při follow se hýbe voluntarily“. Pravidla ř. 609-610:
// „The player's coach must decide whether to follow up BEFORE ANY OTHER DICE ROLLS
// are made.“ Útočník tedy na uvolněném poli už STOJÍ, když se míč odráží, a míč,
// který tam dopadne, CHYTÁ (ř. 856-857) — nezvedá ho a neúspěch není turnover.
// Dřív: se Strip Ballem a u nosiče vytlačeného do davu se míč odrazil PŘED
// rozhodnutím o follow-upu, dopadl na prázdné pole a útočník ho pak ZVEDAL
// (jiný cíl hodu, neúspěch = turnover).
// ---------------------------------------------------------------------------
static int eventIndex(const std::vector<GameEvent>& evs, GameEvent::Type t) {
    for (size_t i = 0; i < evs.size(); ++i) if (evs[i].type == t) return static_cast<int>(i);
    return -1;
}

TEST(BlockHandler, P194StripBallBounceOntoTheFollowedUpAttackerIsACatchNotAPickup) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::StripBall);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    gs.ball = BallState::carried({11, 7}, 12);
    // Pushed (3): obránce na (12,7), útočník následuje na (11,7). Strip Ball: míč padá
    // na (12,7), odraz D8 = 7 (západ) -> (11,7), kde útočník STOJÍ => chytá: AG3 4+,
    // -1 za zónu obránce = 5+. Hod 4 NESTAČÍ (zvednutí by bylo 3+1 = 4+ a prošlo by)
    // => odraz D8 = 1 (sever) -> (11,6). Není to turnover.
    FixedDiceRoller dice({3, 7, 4, 1});
    BlockParams params{1, 12, false, false};
    std::vector<GameEvent> evs;
    auto result = resolveBlock(gs, params, dice, &evs);
    EXPECT_EQ(gs.getPlayer(1).position, (Position{11, 7}));
    EXPECT_FALSE(gs.ball.isHeld) << "hod 4 na chycení (5+) nestačí";
    EXPECT_EQ(gs.ball.position, (Position{11, 6}));
    EXPECT_FALSE(result.turnover) << "nechycený odražený míč není turnover";
    EXPECT_EQ(eventIndex(evs, GameEvent::Type::PICKUP), -1) << "míč se chytá, nezvedá";
    ASSERT_GE(eventIndex(evs, GameEvent::Type::FOLLOW_UP), 0);
    EXPECT_LT(eventIndex(evs, GameEvent::Type::FOLLOW_UP), eventIndex(evs, GameEvent::Type::BALL_BOUNCE))
        << "o follow-upu se rozhoduje před jakýmkoli dalším hodem (ř. 609-610)";
}

TEST(BlockHandler, P194StripBallBounceCaughtByTheFollowedUpAttacker) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::StripBall);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    gs.ball = BallState::carried({11, 7}, 12);
    FixedDiceRoller dice({3, 7, 5});   // totéž, chycení 5+ hodem 5
    BlockParams params{1, 12, false, false};
    auto result = resolveBlock(gs, params, dice, nullptr);
    EXPECT_TRUE(gs.ball.isHeld);
    EXPECT_EQ(gs.ball.carrierId, 1);
    EXPECT_FALSE(result.turnover);
}

// Nosič vytlačený do davu (sestava z N3b): i tady se o follow-upu rozhoduje před
// vhazováním. Vhazování 2D6 = 2 -> (11,1), odraz D8 = 1 (sever) -> (11,0), kde už
// stojí útočník => chytá: 4+ a -2 za zóny hráčů 13 a 14 = 6+. Hod 5 nestačí
// (zvednutí 3+2 = 5+ by prošlo) => odraz D8 = 5 (jih) -> (11,1).
TEST(BlockHandler, P194FollowUpIsDecidedBeforeTheCrowdThrowsTheBallBackIn) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    placePlayer(gs, 1, {10, 0}, TeamSide::HOME, 6, 4, 3, 8);
    placePlayer(gs, 12, {11, 0}, TeamSide::AWAY);
    placePlayer(gs, 13, {12, 0}, TeamSide::AWAY);
    placePlayer(gs, 14, {12, 1}, TeamSide::AWAY);
    gs.ball = BallState::carried({11, 0}, 12);

    FixedDiceRoller dice({3, 3, 3, 1, 1, 1, 5, 5, 1, 1, 1, 1});
    BlockParams params{1, 12, false, false};
    std::vector<GameEvent> evs;
    auto result = resolveBlock(gs, params, dice, &evs);

    ASSERT_FALSE(gs.getPlayer(12).isOnPitch());
    EXPECT_EQ(gs.getPlayer(1).position, (Position{11, 0}));
    EXPECT_FALSE(gs.ball.isHeld) << "hod 5 na chycení (6+) nestačí";
    EXPECT_EQ(gs.ball.position, (Position{11, 1}));
    EXPECT_FALSE(result.turnover);
    EXPECT_EQ(eventIndex(evs, GameEvent::Type::PICKUP), -1);
    ASSERT_GE(eventIndex(evs, GameEvent::Type::FOLLOW_UP), 0);
    EXPECT_LT(eventIndex(evs, GameEvent::Type::FOLLOW_UP), eventIndex(evs, GameEvent::Type::BALL_BOUNCE));
}

// ⭐ 15.10.2026 (nalezeno při P156): V BLITZU JEN JEDEN BLOK. BB2016 ř. 347-350: „He may make one
//   block during the move.“ Po bloku v blitzu zůstává aktivace otevřená (M1/N10 — smí dál běžet),
//   ale podmínka bloku pouštěla každého s `usedBlitz` ⇒ engine nabízel druhý BLOCK (4793× ve 199
//   výkopech lidé × orkové). Kostky se tu neřídí: hledá se semínko, po kterém útočník stojí a vedle
//   něj stojí druhý soupeř — pak se smí jen hýbat.
TEST(BlockHandler, BlitzAllowsOnlyOneBlock) {
    int checked = 0;
    for (uint32_t seed = 1; seed <= 60 && checked < 5; ++seed) {
        GameState state;
        state.phase = GamePhase::PLAY;
        state.activeTeam = TeamSide::HOME;
        state.homeTeam.turnNumber = 2;
        Player& att = state.getPlayer(1);
        att.id = 1; att.teamSide = TeamSide::HOME; att.state = PlayerState::STANDING;
        att.position = {10, 7}; att.stats = {6, 4, 3, 8}; att.movementRemaining = 6;
        att.skills.add(SkillName::Block);
        const Position defPos[3] = {{13, 7}, {13, 6}, {13, 8}};   // cíl + soused každého pole příchodu
        for (int k = 0; k < 3; ++k) {
            Player& d = state.getPlayer(12 + k);
            d.id = 12 + k; d.teamSide = TeamSide::AWAY; d.state = PlayerState::STANDING;
            d.position = defPos[k];
            d.stats = {6, 3, 3, 8}; d.movementRemaining = 6;
        }
        state.ball = BallState::onGround({2, 2});
        DiceRoller dice(seed);
        const ActionResult r = executeAction(state, Action{ActionType::BLITZ, 1, 12, {13, 7}}, dice, nullptr);
        if (r.turnover || state.getPlayer(1).state != PlayerState::STANDING) continue;
        bool nextToStanding = false;
        for (int k = 0; k < 3; ++k) {
            const Player& d = state.getPlayer(12 + k);
            if (d.state == PlayerState::STANDING && d.position.distanceTo(state.getPlayer(1).position) == 1)
                nextToStanding = true;
        }
        if (!nextToStanding) continue;
        ++checked;
        std::vector<Action> actions;
        getAvailableActions(state, actions);
        bool canStillMove = false;
        for (const Action& a : actions) {
            EXPECT_FALSE(a.type == ActionType::BLOCK && a.playerId == 1)
                << "semínko " << seed << ": po bloku v blitzu nabídnut druhý BLOCK na " << a.targetId;
            if (a.type == ActionType::MOVE && a.playerId == 1) canStillMove = true;
        }
        // Druhá polovina pravidla (ř. 347-350, M1/N10): po bloku smí v blitzu dál BĚŽET.
        if (state.getPlayer(1).movementRemaining > 0)
            EXPECT_TRUE(canStillMove) << "semínko " << seed << ": po bloku v blitzu už nesmí ani chodit";
    }
    ASSERT_GE(checked, 1) << "pozitivní kontrola: situace „po blitzu stojí vedle soupeře“ musí nastat";
}

// 15.10.2026: Multiple Block jen „at the start of a Block Action“ (ř. 8299) — hráč, který se
// pohnul (nebo ohlásil blitz), ho už vyhlásit nesmí. Pozitivní kontrola: na začátku aktivace ano.
TEST(BlockHandler, MultipleBlockOnlyAtTheStartOfTheAction) {
    GameState state;
    state.phase = GamePhase::PLAY;
    state.activeTeam = TeamSide::HOME;
    Player& att = state.getPlayer(1);
    att.id = 1; att.teamSide = TeamSide::HOME; att.state = PlayerState::STANDING;
    att.position = {10, 7}; att.stats = {6, 4, 3, 8}; att.movementRemaining = 6;
    att.skills.add(SkillName::MultipleBlock);
    for (int k = 0; k < 2; ++k) {
        Player& d = state.getPlayer(12 + k);
        d.id = 12 + k; d.teamSide = TeamSide::AWAY; d.state = PlayerState::STANDING;
        d.position = {11, static_cast<int8_t>(6 + 2 * k)}; d.stats = {6, 3, 3, 8};
    }
    state.ball = BallState::onGround({2, 2});
    auto offered = [&] {
        std::vector<Action> actions;
        getAvailableActions(state, actions);
        for (const Action& a : actions) if (a.type == ActionType::MULTIPLE_BLOCK && a.playerId == 1) return true;
        return false;
    };
    ASSERT_TRUE(offered()) << "pozitivní kontrola: na začátku aktivace se nabídnout má";
    state.getPlayer(1).hasMoved = true;
    state.currentActivationId = 1;
    EXPECT_FALSE(offered()) << "po pohybu už to Block Action není";
}

// 15.10.2026 (uživatel přinesl čtení pravidel): Foul Appearance v BLITZU. Ř. 8131-8133: na 1 je
// „too revolted to make the block and IT is wasted“ — propadá blok, ne akce Blitz; podle ř. 347-350
// smí blitzující dál běžet (jen už neblokuje, blok blitzu byl spotřebován). Dřív FA aktivaci zavíral.
TEST(BlockHandler, FoulAppearanceInBlitzWastesOnlyTheBlock) {
    GameState state;
    state.phase = GamePhase::PLAY;
    state.activeTeam = TeamSide::HOME;
    state.homeTeam.turnNumber = 2;
    Player& att = state.getPlayer(1);
    att.id = 1; att.teamSide = TeamSide::HOME; att.state = PlayerState::STANDING;
    att.position = {10, 7}; att.stats = {6, 3, 3, 8}; att.movementRemaining = 6;
    Player& fa = state.getPlayer(12);
    fa.id = 12; fa.teamSide = TeamSide::AWAY; fa.state = PlayerState::STANDING;
    fa.position = {12, 7}; fa.stats = {6, 3, 3, 8};
    fa.skills.add(SkillName::FoulAppearance);
    for (int k = 0; k < 2; ++k) {   // druzí soupeři: každé pole příchodu (11,6..8) sousedí s jedním
        Player& other = state.getPlayer(13 + k);
        other.id = 13 + k; other.teamSide = TeamSide::AWAY; other.state = PlayerState::STANDING;
        other.position = {12, static_cast<int8_t>(6 + 2 * k)}; other.stats = {6, 3, 3, 8};
    }
    state.ball = BallState::onGround({2, 2});
    FixedDiceRoller dice({1, 1, 1, 1});    // první hod = Foul Appearance: 1
    executeAction(state, Action{ActionType::BLITZ, 1, 12, {12, 7}}, dice, nullptr);
    ASSERT_EQ(state.getPlayer(1).position.distanceTo(fa.position), 1) << "fixture: blitzující má dojít vedle cíle";
    ASSERT_EQ(fa.state, PlayerState::STANDING) << "fixture: blok měl propadnout";
    // Uživatel 15.10.: „nikde nevidím důvod nespotřebovat bod pohybu na deklarovaný block, i když jej
    // Foul Appearance znehodnotil“ (ř. 550: blok „costs one square of movement“) ⇒ 1 pole chůze + 1 za blok.
    EXPECT_EQ(state.getPlayer(1).movementRemaining, 6 - 1 - 1) << "propadlý blok stojí pole pohybu";
    std::vector<Action> actions;
    getAvailableActions(state, actions);
    bool move = false, block = false;
    for (const Action& a : actions) {
        if (a.playerId != 1) continue;
        if (a.type == ActionType::MOVE) move = true;
        if (a.type == ActionType::BLOCK) block = true;
    }
    EXPECT_TRUE(move) << "po propadlém bloku smí blitzující dál běžet";
    EXPECT_FALSE(block) << "blok blitzu je spotřebován — druhý ne";
}

// ---------------------------------------------------------------------------
// P192 review (3)(4)(5) — Grab × Side Step. Grab, ř. 8148-8154: „only while making a Block
// Action, if his block results in a push back he MAY choose any empty square adjacent to his
// opponent … When making a Block OR BLITZ Action, Grab and Side Step will cancel each other out
// and the standard pushback rules apply. Grab will not work if there are no empty adjacent
// squares.“ Side Step, ř. 8477-8478: „may not use this skill if there are no open squares on
// the pitch adjacent to this player.“
// ---------------------------------------------------------------------------
namespace {
// Útočník HOME (10,7) → obránce AWAY (11,7); tři pole odtlačení (12,6..8) drží HOME přihlížející,
// vedle obránce (11,6) a (11,8) je volno ⇒ Side Step uhne na hřiště, běžné odtlačení řetězí.
GameState grabVsSideStepFixture(bool attackerHasGrab) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    if (attackerHasGrab) gs.getPlayer(1).skills.add(SkillName::Grab);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    gs.getPlayer(12).skills.add(SkillName::SideStep);
    placePlayer(gs, 2, {12, 6}, TeamSide::HOME);
    placePlayer(gs, 3, {12, 7}, TeamSide::HOME);
    placePlayer(gs, 4, {12, 8}, TeamSide::HOME);
    gs.ball = BallState::onGround({2, 2});
    return gs;
}
}  // namespace

// (3) Grab v BLITZU sám nepůsobí („only while making a Block Action“), ale Side Step ruší dál
// („When making a Block or Blitz Action … cancel each other out“). Dřív `grab = … && !isBlitz`
// počítalo zrušení až z oslabeného `grab`, takže v blitzu si obránce pole vybral sám.
TEST(BlockHandler, P192GrabCancelsSideStepOnABlitzToo) {
    {
        GameState gs = grabVsSideStepFixture(true);
        FixedDiceRoller dice({3, 3, 3, 3, 3, 3, 3, 3});   // až 3 kostky (asistence), vše PUSHED
        resolveBlock(gs, BlockParams{1, 12, true, false}, dice, nullptr, false, /*noFollowUp=*/true);
        EXPECT_EQ(gs.getPlayer(12).position.x, 12)
            << "Grab × Side Step se ruší i v blitzu ⇒ běžné odtlačení (tři pole za ním, řetěz)";
    }
    {   // pozitivní kontrola: bez Grabu v témž blitzu Side Step platí a obránce uhne vedle
        GameState gs = grabVsSideStepFixture(false);
        FixedDiceRoller dice({3, 3, 3, 3, 3, 3, 3, 3});
        resolveBlock(gs, BlockParams{1, 12, true, false}, dice, nullptr, false, true);
        EXPECT_NE(gs.getPlayer(12).position.x, 12);
        EXPECT_EQ(gs.getPlayer(3).position, (Position{12, 7})) << "nikdo neřetězen";
    }
}

// (4) Grab je VOLBA (uživatel 10.10.: „většina skillů jsou nepovinné - může grab nevyužít pro
// vysurfování“; ř. 8149 „he MAY choose“). Grab umí jen PRÁZDNÁ pole na hřišti, do davu tedy
// jedině bez něj. Dřív se Grab použil vždy a obránce u lajny do davu nešel nikdy.
TEST(BlockHandler, P192AttackerDeclinesGrabWhenTheStandardPushGoesIntoTheCrowd) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    placePlayer(gs, 1, {11, 1}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::Grab);
    placePlayer(gs, 12, {11, 0}, TeamSide::AWAY);   // všechna tři pole odtlačení mimo hřiště
    gs.ball = BallState::onGround({2, 7});
    FixedDiceRoller dice({3, 3, 3, 3, 3, 3, 3, 3});  // PUSHED; pak zranění v davu
    resolveBlock(gs, BlockParams{1, 12, false, false}, dice, nullptr, false, true);
    EXPECT_FALSE(gs.getPlayer(12).isOnPitch()) << "útočník Grab nepoužije a vytlačí do davu";
}

// Pozitivní kontrola k (4): kde běžné odtlačení do davu nevede, Grab platí — obránce jde na
// volné sousední pole MIMO tři pole diagramu místo řetězu (ř. 8149-8150).
TEST(BlockHandler, P192GrabStillPicksAnyEmptyAdjacentSquareInsteadOfAChain) {
    GameState gs = grabVsSideStepFixture(true);
    gs.getPlayer(12).skills = SkillSet{};             // obránce bez Side Step
    FixedDiceRoller dice({3, 3, 3, 3, 3, 3, 3, 3});
    resolveBlock(gs, BlockParams{1, 12, false, false}, dice, nullptr, false, true);
    EXPECT_NE(gs.getPlayer(12).position.x, 12) << "Grab: volné pole vedle, ne řetěz";
    EXPECT_TRUE(gs.getPlayer(12).isOnPitch());
    EXPECT_EQ(gs.getPlayer(3).position, (Position{12, 7}));
}

// (5) Bez volného sousedního pole Side Step ani Grab neplatí (ř. 8153-8154, 8477-8478) —
// a pak NESMÍ ani vybírat, kam se řetězí: platí běžná pravidla, pole volí útočník běžnou
// volbou (pushDestScore: co nejdál od našeho nosiče). Dřív se kandidáti vrátili na tři pole,
// ale výběr mezi nimi dál řídil skill (Side Step: obránce; Grab: „k lajně“).
namespace {
GameState surroundedDefenderFixture() {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    const Position around[7] = {{10, 6}, {10, 8}, {11, 6}, {11, 8}, {12, 6}, {12, 7}, {12, 8}};
    for (int k = 0; k < 7; ++k) placePlayer(gs, 2 + k, around[k], TeamSide::HOME);
    placePlayer(gs, 9, {13, 9}, TeamSide::HOME);     // náš nosič: běžná volba tlačí od něj ⇒ (12,6)
    gs.ball = BallState::carried({13, 9}, 9);
    return gs;
}
}  // namespace

TEST(BlockHandler, P192SideStepWithNoOpenSquareDoesNotPickTheChainSquare) {
    GameState gs = surroundedDefenderFixture();
    gs.getPlayer(12).skills.add(SkillName::SideStep);
    FixedDiceRoller dice({3, 3, 3, 3, 3, 3, 3, 3});
    resolveBlock(gs, BlockParams{1, 12, false, false}, dice, nullptr, false, true);
    EXPECT_EQ(gs.getPlayer(12).position, (Position{12, 6}));
}

TEST(BlockHandler, P192GrabWithNoEmptySquareDoesNotPickTheChainSquare) {
    GameState gs = surroundedDefenderFixture();
    gs.getPlayer(1).skills.add(SkillName::Grab);
    FixedDiceRoller dice({3, 3, 3, 3, 3, 3, 3, 3});
    resolveBlock(gs, BlockParams{1, 12, false, false}, dice, nullptr, false, true);
    EXPECT_EQ(gs.getPlayer(12).position, (Position{12, 6}));
}

// Pozitivní kontrola k (5): táž rána bez skillů vede na (12,6) — fixture měří, co má.
TEST(BlockHandler, P192SurroundedDefenderWithoutSkillsIsChainedAwayFromOurCarrier) {
    GameState gs = surroundedDefenderFixture();
    FixedDiceRoller dice({3, 3, 3, 3, 3, 3, 3, 3});
    resolveBlock(gs, BlockParams{1, 12, false, false}, dice, nullptr, false, true);
    EXPECT_EQ(gs.getPlayer(12).position, (Position{12, 6}));
}

// ============================================================================
// P225 (uživatel 21.10.2026, „AI jej nepoužije, když má nachystanou frenzy trap“).
// Stand Firm je volba (ř. 8510-8516 „may choose to not be pushed back“; ř. 985, 1820
// „Skill use is not mandatory“). Frenzy po Pushed / Defender Stumbles MUSÍ následovat
// a hodit druhý blok, pokud oba stojí a sousedí (ř. 8134-8141).
// AI obránce Stand Firm NEPOUŽIJE, když zároveň: (a) druhý blok z nového pole (po
// odtlačení a follow-upu) je pro útočníka horší než ze stejného pole, (b) odtlačení
// nepošle obránce do davu, (c) obránce nenese míč. Jinak Stand Firm použije.
//
// Past: útočník (Frenzy) z (10,7) na obránce (11,7) se Stand Firm. Dva soupeřovi
// pomocníci na (12,6) a (12,8): od (10,7) jsou 2 pole, k (11,7) sousedí. Jediné volné
// pole odtlačení je (12,7) ⇒ útočník následuje na (11,7) a druhý blok hází proti dvěma
// asistencím obránce: ST3 vs ST5 = 2 kostky volí obránce (−2). Ze stejného pole 1 kostka (+1).
// Kostky: 1. blok Pushed (3); druhý blok samé Attacker Down (1) — útočník leží, nikdo
// dál nikam netlačí, takže poloha obránce čte jen rozhodnutí o Stand Firm.
// ============================================================================
namespace {
GameState frenzyTrapFixture() {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    gs.activeTeam = TeamSide::HOME;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::Frenzy);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    gs.getPlayer(12).skills.add(SkillName::StandFirm);
    placePlayer(gs, 13, {12, 6}, TeamSide::AWAY);
    placePlayer(gs, 14, {12, 8}, TeamSide::AWAY);
    return gs;
}
std::vector<int> trapDice() { return {3, 1, 1, 1, 1, 1, 1, 1, 1}; }
}  // namespace

TEST(BlockHandler, P225FrenzyTrapReady_AiDeclinesStandFirmAndIsPushed) {
    GameState gs = frenzyTrapFixture();
    FixedDiceRoller dice(trapDice());
    takeStandFirmDeclinedForFrenzy();
    resolveBlock(gs, BlockParams{1, 12, false, false}, dice, nullptr);
    EXPECT_EQ(gs.getPlayer(12).position, (Position{12, 7}));   // odtlačen
    EXPECT_EQ(gs.getPlayer(1).position, (Position{11, 7}));    // Frenzy následoval
    EXPECT_EQ(takeStandFirmDeclinedForFrenzy(), 1);
}

// Defender Stumbles s Dodge = jen odtlačení (obránce stojí) ⇒ táž past.
TEST(BlockHandler, P225FrenzyTrapOnStumblesWithDodge_AiDeclinesStandFirm) {
    GameState gs = frenzyTrapFixture();
    gs.getPlayer(12).skills.add(SkillName::Dodge);
    FixedDiceRoller dice({5, 1, 1, 1, 1, 1, 1, 1, 1});
    resolveBlock(gs, BlockParams{1, 12, false, false}, dice, nullptr);
    EXPECT_EQ(gs.getPlayer(12).position, (Position{12, 7}));
}

// Pomocníci na (12,6) a (12,8) jsou v zóně našeho hráče na (13,7) ⇒ neasistují;
// druhý blok z nového pole je zase 1 kostka (+1 = +1, ne horší) ⇒ Stand Firm.
TEST(BlockHandler, P225NoWorseDiceFromNewSquare_AiUsesStandFirm) {
    GameState gs = frenzyTrapFixture();
    placePlayer(gs, 2, {13, 7}, TeamSide::HOME);
    FixedDiceRoller dice(trapDice());
    resolveBlock(gs, BlockParams{1, 12, false, false}, dice, nullptr);
    EXPECT_EQ(gs.getPlayer(12).position, (Position{11, 7}));
}

// Odtlačení do davu: obránce u lajny (11,0), útočník šikmo z (10,1); jediné pole odtlačení
// na hřišti (12,0) obsazené ⇒ dav (ř. 650-651). Pomocník (12,1) sousedí s (11,0), ne s (10,1).
TEST(BlockHandler, P225PushIntoCrowd_AiUsesStandFirm) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    placePlayer(gs, 1, {10, 1}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::Frenzy);
    placePlayer(gs, 12, {11, 0}, TeamSide::AWAY);
    gs.getPlayer(12).skills.add(SkillName::StandFirm);
    placePlayer(gs, 13, {12, 0}, TeamSide::AWAY);
    placePlayer(gs, 14, {12, 1}, TeamSide::AWAY);
    FixedDiceRoller dice(trapDice());
    resolveBlock(gs, BlockParams{1, 12, false, false}, dice, nullptr);
    EXPECT_EQ(gs.getPlayer(12).position, (Position{11, 0}));
    EXPECT_TRUE(gs.getPlayer(12).isOnPitch());
}

TEST(BlockHandler, P225DefenderCarriesBall_AiUsesStandFirm) {
    GameState gs = frenzyTrapFixture();
    gs.ball = BallState::carried({11, 7}, 12);
    FixedDiceRoller dice(trapDice());
    resolveBlock(gs, BlockParams{1, 12, false, false}, dice, nullptr);
    EXPECT_EQ(gs.getPlayer(12).position, (Position{11, 7}));
}

TEST(BlockHandler, P225AttackerWithoutFrenzy_AiUsesStandFirm) {
    GameState gs = frenzyTrapFixture();
    gs.getPlayer(1).skills = SkillSet{};
    FixedDiceRoller dice(trapDice());
    resolveBlock(gs, BlockParams{1, 12, false, false}, dice, nullptr);
    EXPECT_EQ(gs.getPlayer(12).position, (Position{11, 7}));
}

// Defender Stumbles bez Dodge: obránce padá tak jako tak, druhý blok nebude ⇒ past není.
TEST(BlockHandler, P225StumblesWithoutDodge_NoSecondBlock_AiUsesStandFirm) {
    GameState gs = frenzyTrapFixture();
    FixedDiceRoller dice({5, 1, 1, 1, 1, 1, 1});
    resolveBlock(gs, BlockParams{1, 12, false, false}, dice, nullptr);
    EXPECT_EQ(gs.getPlayer(12).position, (Position{11, 7}));
    EXPECT_EQ(gs.getPlayer(12).state, PlayerState::PRONE);
}

// Blitz bez zbytku pohybu: druhý blok se nehází (ř. 8143-8145) ⇒ past není.
TEST(BlockHandler, P225BlitzWithoutMovementForSecondBlock_AiUsesStandFirm) {
    GameState gs = frenzyTrapFixture();
    gs.getPlayer(1).movementRemaining = -1;   // první GFI už padl na příchod
    FixedDiceRoller dice({6, 3});              // GFI za blok, Pushed
    resolveBlock(gs, BlockParams{1, 12, true, false}, dice, nullptr);
    EXPECT_EQ(gs.getPlayer(12).position, (Position{11, 7}));
}

TEST(BlockHandler, P225SwitchOff_AiUsesStandFirmAsBefore) {
    GameState gs = frenzyTrapFixture();
    setExtraFeaturesOff(kP225FrenzyTrapStandFirm);
    FixedDiceRoller dice(trapDice());
    resolveBlock(gs, BlockParams{1, 12, false, false}, dice, nullptr);
    setExtraFeaturesOff(0);
    EXPECT_EQ(gs.getPlayer(12).position, (Position{11, 7}));
}

// Kouč (P130) dostává otázku dál, i když je past nachystaná — volí on, ne AI.
TEST(BlockHandler, P225CoachDefenderIsStillAsked) {
    GameState gs = frenzyTrapFixture();
    FixedDiceRoller dice(trapDice());
    CoachChoiceScope scope(TeamSide::AWAY, {});
    try {
        resolveBlock(gs, BlockParams{1, 12, false, false}, dice, nullptr);
        FAIL() << "kouč obránce nedostal otázku";
    } catch (const CoachChoicePending& p) {
        EXPECT_EQ(p.question, CoachQuestion::USE_STAND_FIRM);
        EXPECT_EQ(p.playerId, 12);
    }
}
