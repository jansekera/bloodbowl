<?php

declare(strict_types=1);

namespace App\Tests\Engine;

use App\Engine\BallResolver;
use App\Engine\FixedDiceRoller;
use App\Engine\ScatterCalculator;
use App\Engine\TacklezoneCalculator;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use PHPUnit\Framework\TestCase;

final class BallResolverTest extends TestCase
{
    private TacklezoneCalculator $tzCalc;
    private ScatterCalculator $scatterCalc;

    protected function setUp(): void
    {
        $this->tzCalc = new TacklezoneCalculator();
        $this->scatterCalc = new ScatterCalculator();
    }

    private function resolver(FixedDiceRoller $dice): BallResolver
    {
        return new BallResolver($dice, $this->tzCalc, $this->scatterCalc);
    }

    public function testPickupSuccessNoTackleZones(): void
    {
        // AG3 player, no TZ: target = 7 - 3 - 1 = 3+
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 10, 7, agility: 3, id: 1)
            ->withBallOnGround(10, 7)
            ->build();

        $player = $state->getPlayer(1);
        $this->assertNotNull($player);

        // Roll 3 = success
        $dice = new FixedDiceRoller([3]);
        $result = $this->resolver($dice)->resolvePickup($state, $player);

        $this->assertTrue($result['success']);
        $this->assertTrue($result['state']->getBall()->isHeld());
        $this->assertEquals(1, $result['state']->getBall()->getCarrierId());
    }

    public function testPickupFailureBouncesball(): void
    {
        // AG3, no TZ: target = 3+
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 10, 7, agility: 3, id: 1)
            ->withBallOnGround(10, 7)
            ->build();

        $player = $state->getPlayer(1);
        $this->assertNotNull($player);

        // Roll 2 = fail, then D8=3 (East) for bounce
        $dice = new FixedDiceRoller([2, 3]);
        $result = $this->resolver($dice)->resolvePickup($state, $player);

        $this->assertFalse($result['success']);
        $this->assertFalse($result['state']->getBall()->isHeld());
        // Ball bounced East to (11, 7)
        $ballPos = $result['state']->getBall()->getPosition();
        $this->assertNotNull($ballPos);
        $this->assertEquals(11, $ballPos->getX());
        $this->assertEquals(7, $ballPos->getY());
    }

    public function testPickupWithSureHandsReroll(): void
    {
        // AG3, Sure Hands, no TZ: target = 3+
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 10, 7, agility: 3, skills: [SkillName::SureHands], id: 1)
            ->withBallOnGround(10, 7)
            ->build();

        $player = $state->getPlayer(1);
        $this->assertNotNull($player);

        // First roll 2 = fail, Sure Hands reroll 4 = success
        $dice = new FixedDiceRoller([2, 4]);
        $result = $this->resolver($dice)->resolvePickup($state, $player);

        $this->assertTrue($result['success']);
        $this->assertTrue($result['state']->getBall()->isHeld());
    }

    public function testPickupInTackleZoneHarder(): void
    {
        // AG3, 1 TZ: target = 7 - 3 - 1 + 1 = 4+
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 10, 7, agility: 3, id: 1)
            ->addPlayer(TeamSide::AWAY, 11, 7, id: 10)
            ->withBallOnGround(10, 7)
            ->build();

        $player = $state->getPlayer(1);
        $this->assertNotNull($player);

        // Roll 3 = fail (need 4+), then D8=1 bounce N
        $dice = new FixedDiceRoller([3, 1]);
        $result = $this->resolver($dice)->resolvePickup($state, $player);

        $this->assertFalse($result['success']);
    }

    public function testCatchSuccessWithModifier(): void
    {
        // AG3, no TZ, +1 modifier: target = 7 - 3 + 0 - 1 = 3+
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 10, 7, agility: 3, id: 1)
            ->withBallOnGround(10, 7)
            ->build();

        $player = $state->getPlayer(1);
        $this->assertNotNull($player);

        $dice = new FixedDiceRoller([3]);
        $result = $this->resolver($dice)->resolveCatch($state, $player, modifier: 1);

        $this->assertTrue($result['success']);
        $this->assertTrue($result['state']->getBall()->isHeld());
    }

    public function testCatchFailureBounces(): void
    {
        // AG3, no TZ, no modifier: target = 7 - 3 = 4+
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 10, 7, agility: 3, id: 1)
            ->withBallOnGround(10, 7)
            ->build();

        $player = $state->getPlayer(1);
        $this->assertNotNull($player);

        // Roll 3 = fail, D8=5 bounce S
        $dice = new FixedDiceRoller([3, 5]);
        $result = $this->resolver($dice)->resolveCatch($state, $player);

        $this->assertFalse($result['success']);
        $ballPos = $result['state']->getBall()->getPosition();
        $this->assertNotNull($ballPos);
        $this->assertEquals(10, $ballPos->getX());
        $this->assertEquals(8, $ballPos->getY());
    }

    public function testCatchWithCatchSkillReroll(): void
    {
        // AG3, Catch skill, no TZ: target = 4+
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 10, 7, agility: 3, skills: [SkillName::Catch], id: 1)
            ->withBallOnGround(10, 7)
            ->build();

        $player = $state->getPlayer(1);
        $this->assertNotNull($player);

        // Roll 3 = fail, Catch reroll 5 = success
        $dice = new FixedDiceRoller([3, 5]);
        $result = $this->resolver($dice)->resolveCatch($state, $player);

        $this->assertTrue($result['success']);
    }

    public function testBounceOntoPlayerWhoCatches(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 10, 7, agility: 3, id: 1)
            ->addPlayer(TeamSide::HOME, 11, 7, agility: 3, id: 2)
            ->withBallOnGround(10, 7)
            ->build();

        // D8=3 (East), ball lands on player 2 at (11,7), catch roll 5 = success (need 4+)
        $dice = new FixedDiceRoller([3, 5]);
        $ballPos = $state->getBall()->getPosition();
        $this->assertNotNull($ballPos);
        $result = $this->resolver($dice)->resolveBounce($state, $ballPos);

        $this->assertTrue($result['state']->getBall()->isHeld());
        $this->assertEquals(2, $result['state']->getBall()->getCarrierId());
    }

    public function testBounceOntoPlayerWhoFailsCatchBounceAgain(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 10, 7, agility: 3, id: 1)
            ->addPlayer(TeamSide::HOME, 11, 7, agility: 3, id: 2)
            ->withBallOnGround(10, 7)
            ->build();

        // D8=3 (East) -> lands on player 2 at (11,7)
        // catch roll 2 = fail (need 4+)
        // bounce again: D8=3 (East) -> lands at (12,7), empty square
        $dice = new FixedDiceRoller([3, 2, 3]);
        $ballPos = $state->getBall()->getPosition();
        $this->assertNotNull($ballPos);
        $result = $this->resolver($dice)->resolveBounce($state, $ballPos);

        $this->assertFalse($result['state']->getBall()->isHeld());
        $ballPos = $result['state']->getBall()->getPosition();
        $this->assertNotNull($ballPos);
        $this->assertEquals(12, $ballPos->getX());
        $this->assertEquals(7, $ballPos->getY());
    }

    public function testBounceToEmptySquare(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 10, 7, agility: 3, id: 1)
            ->withBallOnGround(10, 7)
            ->build();

        // D8=1 (North) -> ball lands at (10,6), empty
        $dice = new FixedDiceRoller([1]);
        $ballPos = $state->getBall()->getPosition();
        $this->assertNotNull($ballPos);
        $result = $this->resolver($dice)->resolveBounce($state, $ballPos);

        $this->assertFalse($result['state']->getBall()->isHeld());
        $ballPos = $result['state']->getBall()->getPosition();
        $this->assertNotNull($ballPos);
        $this->assertEquals(10, $ballPos->getX());
        $this->assertEquals(6, $ballPos->getY());
    }

    public function testBounceOffPitchTriggersThrowIn(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 0, 7, agility: 3, id: 1)
            ->withBallOnGround(0, 7)
            ->build();

        // D8=7 (West) -> off pitch
        // Throw-in: D8=3 (East), D6=3 -> lands at (3, 7) on pitch
        $dice = new FixedDiceRoller([7, 3, 3]);
        $ballPos = $state->getBall()->getPosition();
        $this->assertNotNull($ballPos);
        $result = $this->resolver($dice)->resolveBounce($state, $ballPos);

        $ballPos = $result['state']->getBall()->getPosition();
        $this->assertNotNull($ballPos);
        $this->assertTrue($ballPos->isOnPitch());
        $this->assertEquals(3, $ballPos->getX());
    }

    public function testThrowInLandsOnPitch(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1)
            ->withBallOnGround(0, 7)
            ->build();

        // D8=3 (East), D6=4 -> lands at (4, 7)
        $dice = new FixedDiceRoller([3, 4]);
        $ballPos = $state->getBall()->getPosition();
        $this->assertNotNull($ballPos);
        $result = $this->resolver($dice)->resolveThrowIn($state, $ballPos);

        $ballPos = $result['state']->getBall()->getPosition();
        $this->assertNotNull($ballPos);
        $this->assertEquals(4, $ballPos->getX());
        $this->assertEquals(7, $ballPos->getY());
    }

    public function testPickupTargetCalculation(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 10, 7, agility: 3, id: 1)
            ->build();

        $player = $state->getPlayer(1);
        $this->assertNotNull($player);

        $dice = new FixedDiceRoller([]);
        $resolver = $this->resolver($dice);

        // AG3, no TZ: 7 - 3 - 1 = 3
        $this->assertEquals(3, $resolver->getPickupTarget($state, $player));
    }

    public function testCatchTargetCalculation(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 10, 7, agility: 3, id: 1)
            ->addPlayer(TeamSide::AWAY, 11, 7, id: 10)
            ->build();

        $player = $state->getPlayer(1);
        $this->assertNotNull($player);

        $dice = new FixedDiceRoller([]);
        $resolver = $this->resolver($dice);

        // AG3, 1 TZ, no modifier: 7 - 3 + 1 = 5
        $this->assertEquals(5, $resolver->getCatchTarget($state, $player));

        // With +1 modifier: 7 - 3 + 1 - 1 = 4
        $this->assertEquals(4, $resolver->getCatchTarget($state, $player, 1));
    }
}
