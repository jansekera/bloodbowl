<?php
declare(strict_types=1);

namespace App\Tests\Engine;

use App\Engine\ActionResolver;
use App\Engine\FixedDiceRoller;
use App\Enum\ActionType;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use PHPUnit\Framework\TestCase;

final class PassingSkillsTest extends TestCase
{
    // ========== STEP 1: Accurate, Strong Arm, Safe Throw ==========

    public function testAccurateReducesTargetByOne(): void
    {
        // AG3 short pass: base target = 7-3-0 = 4+, with Accurate = 3+
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, agility: 3, skills: [SkillName::Accurate], id: 1)
            ->addPlayer(TeamSide::HOME, 10, 5, agility: 3, id: 2)
            ->withBallCarried(1)
            ->build();

        // Accuracy roll 3 succeeds (target 3+ with Accurate), catch roll 4
        $dice = new FixedDiceRoller([3, 4]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::PASS, [
            'playerId' => 1,
            'targetX' => 10,
            'targetY' => 5,
        ]);

        $this->assertTrue($result->isSuccess());
        $this->assertEquals(2, $result->getNewState()->getBall()->getCarrierId());
    }

    public function testStrongArmReducesRange(): void
    {
        // Distance 8 = Long Pass normally (modifier -1), with Strong Arm = Short Pass (modifier 0)
        // AG3: target = 7-3-0 = 4+ (instead of 7-3+1 = 5+ for long pass)
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, agility: 3, skills: [SkillName::StrongArm], id: 1)
            ->addPlayer(TeamSide::HOME, 13, 5, agility: 3, id: 2)
            ->withBallCarried(1)
            ->build();

        // Accuracy roll 4 succeeds (target 4+), catch roll 4
        $dice = new FixedDiceRoller([4, 4]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::PASS, [
            'playerId' => 1,
            'targetX' => 13,
            'targetY' => 5,
        ]);

        $this->assertTrue($result->isSuccess());
        $this->assertEquals(2, $result->getNewState()->getBall()->getCarrierId());
    }

    public function testStrongArmQuickPassNoEffect(): void
    {
        // Distance 2 = Quick Pass, with Strong Arm still Quick Pass (can't go below)
        // AG3: target = 7-3-1 = 3+ (same with or without Strong Arm)
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, agility: 3, skills: [SkillName::StrongArm], id: 1)
            ->addPlayer(TeamSide::HOME, 7, 5, agility: 3, id: 2)
            ->withBallCarried(1)
            ->build();

        // Accuracy roll 3 succeeds (target 3+), catch roll 4
        $dice = new FixedDiceRoller([3, 4]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::PASS, [
            'playerId' => 1,
            'targetX' => 7,
            'targetY' => 5,
        ]);

        $this->assertTrue($result->isSuccess());
    }

    public function testSafeThrowNullifiesInterception(): void
    {
        // Thrower with SafeThrow, enemy in pass path intercepts but SafeThrow forces reroll
        // Thrower at (5,5), target at (10,5), enemy at (7,5)
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, agility: 3, skills: [SkillName::SafeThrow], id: 1)
            ->addPlayer(TeamSide::HOME, 10, 5, agility: 3, id: 2)
            ->addPlayer(TeamSide::AWAY, 7, 5, agility: 4, id: 3) // AG4: intercept target 7-4+2=5+
            ->withBallCarried(1)
            ->build();

        // Interception roll 5 (succeeds for AG4 target 5+)
        // Safe Throw reroll 3 (fails to intercept, target 5+) → interception nullified
        // Accuracy roll 4, catch roll 4
        $dice = new FixedDiceRoller([5, 3, 4, 4]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::PASS, [
            'playerId' => 1,
            'targetX' => 10,
            'targetY' => 5,
        ]);

        $this->assertTrue($result->isSuccess());
        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('safe_throw', $types);
        $this->assertContains('interception', $types);
    }

    public function testSafeThrowFailsStillIntercepted(): void
    {
        // SafeThrow reroll also succeeds → interception stands
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, agility: 3, skills: [SkillName::SafeThrow], id: 1)
            ->addPlayer(TeamSide::HOME, 10, 5, agility: 3, id: 2)
            ->addPlayer(TeamSide::AWAY, 7, 5, agility: 4, id: 3)
            ->withBallCarried(1)
            ->build();

        // Interception roll 6 (succeeds), SafeThrow reroll 6 (also succeeds → still intercepted)
        $dice = new FixedDiceRoller([6, 6]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::PASS, [
            'playerId' => 1,
            'targetX' => 10,
            'targetY' => 5,
        ]);

        $this->assertTrue($result->isTurnover());
        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('safe_throw', $types);
        // Ball should be with the interceptor
        $this->assertEquals(3, $result->getNewState()->getBall()->getCarrierId());
    }

    // ========== STEP 2: Two Heads, Extra Arms, No Hands ==========

    public function testTwoHeadsReducesDodgeTarget(): void
    {
        // AG3 player with Two Heads dodging out of 1 TZ
        // Base: 7-3=4, +0 (1 TZ, first is free), TwoHeads: -1 → target 3+
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, agility: 3, skills: [SkillName::TwoHeads], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, id: 2) // creates 1 TZ at (5,5)
            ->withBallOffPitch()
            ->build();

        // Dodge roll 3 succeeds (target 3+)
        $dice = new FixedDiceRoller([3]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::MOVE, [
            'playerId' => 1, 'x' => 5, 'y' => 6,
        ]);

        $this->assertTrue($result->isSuccess());
    }

    public function testExtraArmsReducesCatchTarget(): void
    {
        // AG3 receiver with ExtraArms: catch target = 7-3-1(ExtraArms) = 3+ (with accurate +1: 2+)
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, agility: 3, id: 1)
            ->addPlayer(TeamSide::HOME, 7, 5, agility: 3, skills: [SkillName::ExtraArms], id: 2)
            ->withBallCarried(1)
            ->build();

        // Accuracy roll 4 (short pass target 3+), Catch roll 2 (target 2+ with ExtraArms + accurate modifier)
        $dice = new FixedDiceRoller([4, 2]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::PASS, [
            'playerId' => 1,
            'targetX' => 7,
            'targetY' => 5,
        ]);

        $this->assertTrue($result->isSuccess());
        $this->assertEquals(2, $result->getNewState()->getBall()->getCarrierId());
    }

    public function testExtraArmsReducesPickupTarget(): void
    {
        // AG3 with ExtraArms: pickup target = 7-3-1-1(ExtraArms) = 2+
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, agility: 3, skills: [SkillName::ExtraArms], id: 1)
            ->withBallOnGround(6, 5)
            ->build();

        // Pickup roll 2 succeeds (target 2+)
        $dice = new FixedDiceRoller([2]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::MOVE, [
            'playerId' => 1, 'x' => 6, 'y' => 5,
        ]);

        $this->assertTrue($result->isSuccess());
        $this->assertEquals(1, $result->getNewState()->getBall()->getCarrierId());
    }

    public function testNoHandsCannotPickup(): void
    {
        // NoHands player tries to pick up → auto-fail, ball bounces
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, agility: 3, skills: [SkillName::NoHands], id: 1)
            ->withBallOnGround(6, 5)
            ->build();

        // D8=3 for bounce direction
        $dice = new FixedDiceRoller([3]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::MOVE, [
            'playerId' => 1, 'x' => 6, 'y' => 5,
        ]);

        $this->assertTrue($result->isTurnover());
        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('no_hands', $types);
    }

    public function testNoHandsCannotCatch(): void
    {
        // Pass to a NoHands player: accurate pass, but catcher can't catch
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, agility: 3, id: 1)
            ->addPlayer(TeamSide::HOME, 7, 5, agility: 3, skills: [SkillName::NoHands], id: 2)
            ->withBallCarried(1)
            ->build();

        // Accuracy roll 4 (accurate), NoHands → can't catch, D8=3 for bounce
        $dice = new FixedDiceRoller([4, 3]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::PASS, [
            'playerId' => 1,
            'targetX' => 7,
            'targetY' => 5,
        ]);

        $this->assertTrue($result->isTurnover());
        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('no_hands', $types);
    }

    public function testTwoHeadsPlusDodgeStack(): void
    {
        // AG3, TwoHeads + Dodge in 1 TZ: 7-3-1(TwoHeads)-1(Dodge) = 2+
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, agility: 3, skills: [SkillName::TwoHeads, SkillName::Dodge], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, id: 2)
            ->withBallOffPitch()
            ->build();

        $dice = new FixedDiceRoller([2]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::MOVE, [
            'playerId' => 1, 'x' => 5, 'y' => 6,
        ]);

        $this->assertTrue($result->isSuccess());
    }

    // ========== STEP 7: Diving Catch ==========

    public function testDivingCatchBonusInEnemyTZ(): void
    {
        // AG3 catcher with DivingCatch in 1 enemy TZ:
        // Normal: 7-3+1=5+ (with accurate +1: 4+)
        // With DC: 7-3+1-1=4+ (with accurate +1: 3+)
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, agility: 3, id: 1) // thrower
            ->addPlayer(TeamSide::HOME, 7, 5, agility: 3, skills: [SkillName::DivingCatch], id: 2) // catcher
            ->addPlayer(TeamSide::AWAY, 8, 5, id: 3) // creating TZ on catcher
            ->withBallCarried(1)
            ->build();

        // Accuracy roll 4 (short pass target 4+ due to no Accurate), catch roll 3 (target 3+ w/ DC+accurate)
        $dice = new FixedDiceRoller([4, 3]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::PASS, [
            'playerId' => 1,
            'targetX' => 7,
            'targetY' => 5,
        ]);

        $this->assertTrue($result->isSuccess());
        $this->assertEquals(2, $result->getNewState()->getBall()->getCarrierId());
    }

    public function testDivingCatchOnInaccuratePass(): void
    {
        // Inaccurate pass lands on empty square, but DC player is adjacent → catches
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, agility: 3, id: 1) // thrower
            ->addPlayer(TeamSide::HOME, 10, 6, agility: 3, skills: [SkillName::DivingCatch], id: 2) // DC player
            ->withBallCarried(1)
            ->build();

        // Accuracy roll 2 (inaccurate, short pass target 4+)
        // Team reroll: roll 2 (still inaccurate)
        // Scatter: D8=5 (South), D6=1 (dist 1) → lands at (10,6)... player is there.
        // Actually let me have the ball land at (10,5) with DC player at (10,6)
        // Scatter: D8=1 (North from target at 10,5) → (10,4), DC player at (10,5) adjacent
        // Hmm, let me use D8=2 direction, dist=1 → landing at some adjacent empty square

        // Alternative approach: target (10,5), no player there. Inaccurate scatter to (10,6) where DC player is
        // No, DC player catches at EMPTY squares adjacent to them.

        // Let me set up: target at (10,5), DC player at (11,5). Scatter to (11,4) (empty, adjacent to DC player)
        // D8 for scatter direction, D6 for scatter distance
        // Target (10,5), scatter D8=3 (East), D6=1 → landing at (11,5) which has the DC player → regular catch
        // I need the landing to be EMPTY and adjacent to DC player.
        // Let's place DC player at (10,6), target at (10,5), scatter D8=5 (South), D6=1 → (10,6) has DC player
        // Actually I need to test the DC diving-to-empty-square mechanic.

        // OK: thrower at (5,5), target at (10,5), DC player at (11,6).
        // Inaccurate: scatter D8=4 (SE), D6=1 → (11,6) has player → regular catch (not DC)
        // Need: scatter → empty square adjacent to DC player.
        // DC player at (12,5), scatter D8=3 (East), D6=2 from (10,5) → (12,5) has DC player → regular catch
        // Need empty landing, DC player adjacent.
        // DC player at (10,7), scatter D8=5 (South), D6=1 from (10,5) → (10,6) empty, DC player at (10,7) adj.

        $state2 = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, agility: 3, id: 1)
            ->addPlayer(TeamSide::HOME, 10, 7, agility: 3, skills: [SkillName::DivingCatch], id: 2)
            ->withBallCarried(1)
            ->build();

        // Accuracy: 2 (inaccurate, target 4+), team reroll: 2 (still inaccurate)
        // Scatter: D8=5 (South), D6=1 → from (10,5): (10,6) empty
        // DC player at (10,7) is adjacent → moves to (10,6), catch attempt
        // Catch roll: 4 (AG3, target 4+)
        $dice = new FixedDiceRoller([2, 2, 5, 1, 4]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state2, ActionType::PASS, [
            'playerId' => 1,
            'targetX' => 10,
            'targetY' => 5,
        ]);

        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('diving_catch', $types);
        $this->assertEquals(2, $result->getNewState()->getBall()->getCarrierId());
    }

    public function testDivingCatchNoBonusWithoutTZ(): void
    {
        // DC without enemy TZ: no catch bonus (only -1 in TZ)
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, agility: 3, id: 1)
            ->addPlayer(TeamSide::HOME, 7, 5, agility: 3, skills: [SkillName::DivingCatch], id: 2)
            ->withBallCarried(1)
            ->build();

        // Accuracy roll 4, catch: AG3, no TZ, with accurate +1 → target 3+. Roll 3 succeeds.
        $dice = new FixedDiceRoller([4, 3]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::PASS, [
            'playerId' => 1,
            'targetX' => 7,
            'targetY' => 5,
        ]);

        $this->assertTrue($result->isSuccess());
    }

    public function testDivingCatchOutOfRange(): void
    {
        // DC player >1 from landing: can't dive
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, agility: 3, id: 1)
            ->addPlayer(TeamSide::HOME, 15, 5, agility: 3, skills: [SkillName::DivingCatch], id: 2)
            ->withBallCarried(1)
            ->build();

        // Accuracy: 2 (inaccurate), team reroll: 2, scatter D8=5, D6=1 → (10,6) empty
        // DC player at (15,5) is too far → no diving catch, bounce
        // Bounce D8=1
        $dice = new FixedDiceRoller([2, 2, 5, 1, 1]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::PASS, [
            'playerId' => 1,
            'targetX' => 10,
            'targetY' => 5,
        ]);

        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertNotContains('diving_catch', $types);
    }
}
