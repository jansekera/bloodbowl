<?php

declare(strict_types=1);

namespace App\Tests\Engine;

use App\Engine\ActionResolver;
use App\Engine\FixedDiceRoller;
use App\Enum\ActionType;
use App\Enum\TeamSide;
use PHPUnit\Framework\TestCase;

final class PassResolverTest extends TestCase
{
    public function testQuickPassAccurateCatch(): void
    {
        // Thrower AG3 at (5,5) passes to receiver at (7,5) - distance 2 = quick pass
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, agility: 3, id: 1)
            ->addPlayer(TeamSide::HOME, 7, 5, agility: 3, id: 2)
            ->withBallCarried(1)
            ->build();

        // Accuracy: 7-3-1(quick) = 3+, roll 4 = accurate
        // Catch: 7-3 = 4+, +1 accurate = 3+, roll 3 = success
        $dice = new FixedDiceRoller([4, 3]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::PASS, [
            'playerId' => 1,
            'targetX' => 7,
            'targetY' => 5,
        ]);

        $this->assertTrue($result->isSuccess());
        $this->assertFalse($result->isTurnover());
        $this->assertTrue($result->getNewState()->getBall()->isHeld());
        $this->assertEquals(2, $result->getNewState()->getBall()->getCarrierId());
    }

    public function testShortPassAccurateCatch(): void
    {
        // Distance 5 = short pass
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, agility: 3, id: 1)
            ->addPlayer(TeamSide::HOME, 10, 5, agility: 3, id: 2)
            ->withBallCarried(1)
            ->build();

        // Accuracy: 7-3-0(short) = 4+, roll 5 = accurate
        // Catch: 4+, +1 = 3+, roll 4 = success
        $dice = new FixedDiceRoller([5, 4]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::PASS, [
            'playerId' => 1,
            'targetX' => 10,
            'targetY' => 5,
        ]);

        $this->assertTrue($result->isSuccess());
        $this->assertEquals(2, $result->getNewState()->getBall()->getCarrierId());
    }

    public function testFumbleOnNaturalOne(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, agility: 3, id: 1)
            ->addPlayer(TeamSide::HOME, 7, 5, agility: 3, id: 2)
            ->withBallCarried(1)
            ->build();

        // Roll 1 = fumble, team reroll also 1 = fumble, D8=3 (East) for bounce
        $dice = new FixedDiceRoller([1, 1, 3]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::PASS, [
            'playerId' => 1,
            'targetX' => 7,
            'targetY' => 5,
        ]);

        $this->assertTrue($result->isTurnover());
        $this->assertFalse($result->getNewState()->getBall()->isHeld());
    }

    public function testInaccuratePassScatter(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, agility: 3, id: 1)
            ->addPlayer(TeamSide::HOME, 7, 5, agility: 3, id: 2)
            ->withBallCarried(1)
            ->build();

        // Accuracy: 3+, roll 2 = inaccurate, team reroll 2 = still inaccurate
        // Scatter: D8=3 (East), D6=1 (capped at 3) -> (8,5)
        // No player at (8,5) -> ball bounces: D8=5 (South) -> (8,6)
        $dice = new FixedDiceRoller([2, 2, 3, 1, 5]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::PASS, [
            'playerId' => 1,
            'targetX' => 7,
            'targetY' => 5,
        ]);

        // Ball not caught by own team = turnover
        $this->assertTrue($result->isTurnover());
    }

    public function testInaccuratePassCaughtByTeammate(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, agility: 3, id: 1)
            ->addPlayer(TeamSide::HOME, 7, 5, agility: 3, id: 2)
            ->addPlayer(TeamSide::HOME, 8, 5, agility: 3, id: 3) // player at scatter destination
            ->withBallCarried(1)
            ->build();

        // Accuracy: 3+, roll 2 = inaccurate, team reroll 2 = still inaccurate
        // Scatter: D8=3 (East), D6=1 -> (8,5) where player 3 is
        // Player 3 catch: 4+ (no modifier for inaccurate), roll 5 = success
        $dice = new FixedDiceRoller([2, 2, 3, 1, 5]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::PASS, [
            'playerId' => 1,
            'targetX' => 7,
            'targetY' => 5,
        ]);

        $this->assertTrue($result->isSuccess());
        $this->assertEquals(3, $result->getNewState()->getBall()->getCarrierId());
    }

    public function testPassMarksPassUsedThisTurn(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, agility: 3, id: 1)
            ->addPlayer(TeamSide::HOME, 7, 5, agility: 3, id: 2)
            ->withBallCarried(1)
            ->build();

        $dice = new FixedDiceRoller([4, 3]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::PASS, [
            'playerId' => 1,
            'targetX' => 7,
            'targetY' => 5,
        ]);

        $this->assertTrue($result->getNewState()->getHomeTeam()->isPassUsedThisTurn());
    }

    public function testInterceptionSuccess(): void
    {
        // Enemy in pass path can intercept
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, agility: 3, id: 1)
            ->addPlayer(TeamSide::HOME, 10, 5, agility: 3, id: 2) // receiver
            ->addPlayer(TeamSide::AWAY, 7, 5, agility: 4, id: 10) // interceptor in path
            ->withBallCarried(1)
            ->build();

        // Interception: AG4, target = 7-4+2 = 5+, roll 5 = success!
        $dice = new FixedDiceRoller([5]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::PASS, [
            'playerId' => 1,
            'targetX' => 10,
            'targetY' => 5,
        ]);

        $this->assertTrue($result->isTurnover());
        $this->assertTrue($result->getNewState()->getBall()->isHeld());
        $this->assertEquals(10, $result->getNewState()->getBall()->getCarrierId());
    }

    public function testInterceptionFailContinues(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, agility: 3, id: 1)
            ->addPlayer(TeamSide::HOME, 10, 5, agility: 3, id: 2)
            ->addPlayer(TeamSide::AWAY, 7, 5, agility: 3, id: 10)
            ->withBallCarried(1)
            ->build();

        // Interception: AG3, target = 7-3+2 = 6+, roll 4 = fail
        // Accuracy: 7-3-0 = 4+, roll 5 = accurate
        // Catch: 4+, +1=3+, roll 4 = success
        $dice = new FixedDiceRoller([4, 5, 4]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::PASS, [
            'playerId' => 1,
            'targetX' => 10,
            'targetY' => 5,
        ]);

        $this->assertTrue($result->isSuccess());
        $this->assertEquals(2, $result->getNewState()->getBall()->getCarrierId());
    }

    public function testPassPathBresenham(): void
    {
        $resolver = new \App\Engine\PassResolver(
            new FixedDiceRoller([]),
            new \App\Engine\TacklezoneCalculator(),
            new \App\Engine\ScatterCalculator(),
            new \App\Engine\BallResolver(
                new FixedDiceRoller([]),
                new \App\Engine\TacklezoneCalculator(),
                new \App\Engine\ScatterCalculator(),
            ),
        );

        $path = $resolver->getPassPath(
            new \App\ValueObject\Position(5, 5),
            new \App\ValueObject\Position(10, 5),
        );

        // Should return squares between (5,5) and (10,5), excluding endpoints
        $this->assertCount(4, $path); // (6,5), (7,5), (8,5), (9,5)
        $this->assertEquals(6, $path[0]->getX());
        $this->assertEquals(9, $path[3]->getX());
    }

    public function testAccuracyTargetCalculation(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, agility: 3, id: 1)
            ->withBallCarried(1)
            ->build();

        $player = $state->getPlayer(1);
        $this->assertNotNull($player);

        $resolver = new \App\Engine\PassResolver(
            new FixedDiceRoller([]),
            new \App\Engine\TacklezoneCalculator(),
            new \App\Engine\ScatterCalculator(),
            new \App\Engine\BallResolver(
                new FixedDiceRoller([]),
                new \App\Engine\TacklezoneCalculator(),
                new \App\Engine\ScatterCalculator(),
            ),
        );

        // AG3, no TZ, quick pass (+1): 7 - 3 - 1 = 3
        $this->assertEquals(3, $resolver->getAccuracyTarget($state, $player, \App\Enum\PassRange::QUICK_PASS));
        // AG3, no TZ, short pass (0): 7 - 3 = 4
        $this->assertEquals(4, $resolver->getAccuracyTarget($state, $player, \App\Enum\PassRange::SHORT_PASS));
        // AG3, no TZ, long pass (-1): 7 - 3 + 1 = 5
        $this->assertEquals(5, $resolver->getAccuracyTarget($state, $player, \App\Enum\PassRange::LONG_PASS));
        // AG3, no TZ, long bomb (-2): 7 - 3 + 2 = 6
        $this->assertEquals(6, $resolver->getAccuracyTarget($state, $player, \App\Enum\PassRange::LONG_BOMB));
    }
}
