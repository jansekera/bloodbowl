<?php

declare(strict_types=1);

namespace App\Tests\Engine;

use App\Engine\ActionResolver;
use App\Engine\FixedDiceRoller;
use App\Engine\RulesEngine;
use App\Enum\ActionType;
use App\Enum\PlayerState;
use App\Enum\TeamSide;
use PHPUnit\Framework\TestCase;

final class StandUpTest extends TestCase
{
    public function testPronePlayerCanStandUpWithEnoughMA(): void
    {
        // Prone player with MA=6, stands up (costs 3 MA), targets own position
        $state = (new GameStateBuilder())
            ->addPronePlayer(TeamSide::HOME, 5, 7, movement: 6, id: 1)
            ->addPlayer(TeamSide::AWAY, 20, 7, id: 2)
            ->build();

        $dice = new FixedDiceRoller([]); // no dice needed for normal stand-up
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::MOVE, [
            'playerId' => 1, 'x' => 5, 'y' => 7,
        ]);

        $this->assertTrue($result->isSuccess());
        $player = $result->getNewState()->getPlayer(1);
        $this->assertNotNull($player);
        $this->assertEquals(PlayerState::STANDING, $player->getState());
        $this->assertTrue($player->hasMoved());
        $this->assertEquals(3, $player->getMovementRemaining()); // 6 - 3 = 3

        // Check stand_up event
        $standUpEvents = array_filter(
            $result->getEvents(),
            fn($e) => $e->getType() === 'stand_up',
        );
        $this->assertCount(1, $standUpEvents);
    }

    public function testPronePlayerStandsUpAndMoves(): void
    {
        // Prone player at (5,7) with MA=6, stands up (3 MA) then moves to (8,7) (3 MA)
        $state = (new GameStateBuilder())
            ->addPronePlayer(TeamSide::HOME, 5, 7, movement: 6, id: 1)
            ->addPlayer(TeamSide::AWAY, 20, 7, id: 2)
            ->build();

        $dice = new FixedDiceRoller([]); // no dodges/GFI needed
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::MOVE, [
            'playerId' => 1, 'x' => 8, 'y' => 7,
        ]);

        $this->assertTrue($result->isSuccess());
        $player = $result->getNewState()->getPlayer(1);
        $this->assertNotNull($player);
        $this->assertEquals(PlayerState::STANDING, $player->getState());
        $this->assertEquals(8, $player->getPosition()?->getX());
        $this->assertEquals(7, $player->getPosition()?->getY());
        $this->assertTrue($player->hasMoved());
    }

    public function testPronePlayerStandUpReducesMovement(): void
    {
        // Prone player with MA=6 can't reach (9,7) - that's 4 squares but only 3 MA left after stand-up
        $state = (new GameStateBuilder())
            ->addPronePlayer(TeamSide::HOME, 5, 7, movement: 6, id: 1)
            ->addPlayer(TeamSide::AWAY, 20, 7, id: 2)
            ->build();

        $rules = new RulesEngine();
        $targets = $rules->getValidMoveTargets($state, 1);

        // Current position should be in targets (stand in place)
        $hasOwnPos = false;
        $maxX = 0;
        foreach ($targets as $t) {
            if ($t['x'] === 5 && $t['y'] === 7) {
                $hasOwnPos = true;
            }
            if ($t['y'] === 7) {
                $maxX = max($maxX, $t['x']);
            }
        }

        $this->assertTrue($hasOwnPos, 'Prone player should be able to stand in place');
        // With 3 MA + 2 GFI, max east = 5+5 = 10
        $this->assertLessThanOrEqual(10, $maxX);
        // With only 3 MA (no GFI), can reach x=8
        // Verify x=9 requires GFI
        $gfiTargets = array_filter($targets, fn($t) => $t['x'] === 9 && $t['y'] === 7);
        if ($gfiTargets !== []) {
            $gfiTarget = array_values($gfiTargets)[0];
            $this->assertGreaterThan(0, $gfiTarget['gfis']);
        }
    }

    public function testPronePlayerLowMAStandUpRollSuccess(): void
    {
        // Prone player with MA=2 (< 3), needs 4+ roll to stand
        $state = (new GameStateBuilder())
            ->addPronePlayer(TeamSide::HOME, 5, 7, movement: 2, id: 1)
            ->addPlayer(TeamSide::AWAY, 20, 7, id: 2)
            ->build();

        $dice = new FixedDiceRoller([4]); // roll 4 = success (4+)
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::MOVE, [
            'playerId' => 1, 'x' => 5, 'y' => 7,
        ]);

        $this->assertTrue($result->isSuccess());
        $player = $result->getNewState()->getPlayer(1);
        $this->assertNotNull($player);
        $this->assertEquals(PlayerState::STANDING, $player->getState());
        $this->assertEquals(0, $player->getMovementRemaining());

        // Check event has roll info
        $events = $result->getEvents();
        $standUpEvent = $events[0];
        $this->assertEquals('stand_up', $standUpEvent->getType());
        $this->assertTrue($standUpEvent->getData()['success']);
        $this->assertEquals(4, $standUpEvent->getData()['roll']);
    }

    public function testPronePlayerLowMAStandUpRollFail(): void
    {
        // Prone player with MA=2, rolls 3 (needs 4+) = fail
        $state = (new GameStateBuilder())
            ->addPronePlayer(TeamSide::HOME, 5, 7, movement: 2, id: 1)
            ->addPlayer(TeamSide::AWAY, 20, 7, id: 2)
            ->build();

        $dice = new FixedDiceRoller([3]); // roll 3 = fail
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::MOVE, [
            'playerId' => 1, 'x' => 5, 'y' => 7,
        ]);

        // Failed stand-up is NOT a turnover
        $this->assertTrue($result->isSuccess());
        $this->assertFalse($result->isTurnover());

        $player = $result->getNewState()->getPlayer(1);
        $this->assertNotNull($player);
        $this->assertEquals(PlayerState::PRONE, $player->getState()); // Still prone
        $this->assertTrue($player->hasMoved()); // Action done
        $this->assertTrue($player->hasActed()); // Can't do anything else

        // Check event
        $events = $result->getEvents();
        $this->assertCount(1, $events);
        $this->assertEquals('stand_up', $events[0]->getType());
        $this->assertFalse($events[0]->getData()['success']);
    }

    public function testPronePlayerAppearsInAvailableActions(): void
    {
        $state = (new GameStateBuilder())
            ->addPronePlayer(TeamSide::HOME, 5, 7, movement: 6, id: 1)
            ->addPlayer(TeamSide::AWAY, 20, 7, id: 2)
            ->build();

        $rules = new RulesEngine();
        $actions = $rules->getAvailableActions($state);

        // MOVE should be available for prone player
        $moveActions = array_filter($actions, fn($a) => $a['type'] === 'move' && ($a['playerId'] ?? null) === 1);
        $this->assertNotEmpty($moveActions, 'Prone player should have MOVE available');
    }

    public function testPronePlayerLowMAOnlyStandInPlace(): void
    {
        // MA=2 player can only stand in place (pathfinder returns empty)
        $state = (new GameStateBuilder())
            ->addPronePlayer(TeamSide::HOME, 5, 7, movement: 2, id: 1)
            ->addPlayer(TeamSide::AWAY, 20, 7, id: 2)
            ->build();

        $rules = new RulesEngine();
        $targets = $rules->getValidMoveTargets($state, 1);

        // Should only have current position
        $this->assertCount(1, $targets);
        $this->assertEquals(5, $targets[0]['x']);
        $this->assertEquals(7, $targets[0]['y']);
    }

    public function testPronePlayerStandUpAndPickupBall(): void
    {
        // Prone player on a square with a loose ball, stands up and picks it up
        $state = (new GameStateBuilder())
            ->addPronePlayer(TeamSide::HOME, 5, 7, movement: 6, agility: 3, id: 1)
            ->addPlayer(TeamSide::AWAY, 20, 7, id: 2)
            ->withBallOnGround(7, 7)
            ->build();

        // Stand up (auto), move to (7,7), pickup roll: AG3 target=3+, roll 3 = success
        $dice = new FixedDiceRoller([3]); // pickup roll
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::MOVE, [
            'playerId' => 1, 'x' => 7, 'y' => 7,
        ]);

        $this->assertTrue($result->isSuccess());
        $ball = $result->getNewState()->getBall();
        $this->assertTrue($ball->isHeld());
        $this->assertEquals(1, $ball->getCarrierId());
    }

    public function testPronePlayerStandUpWithGFI(): void
    {
        // Prone player with MA=4, stands up (3 MA), 1 MA left, moves 3 squares (2 GFI)
        $state = (new GameStateBuilder())
            ->addPronePlayer(TeamSide::HOME, 5, 7, movement: 4, id: 1)
            ->addPlayer(TeamSide::AWAY, 20, 7, id: 2)
            ->build();

        // 1 normal MA + 2 GFI = 3 squares. Move from (5,7) to (8,7).
        // GFI rolls: 2 (success), 2 (success)
        $dice = new FixedDiceRoller([2, 2]); // two GFI rolls
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::MOVE, [
            'playerId' => 1, 'x' => 8, 'y' => 7,
        ]);

        $this->assertTrue($result->isSuccess());
        $player = $result->getNewState()->getPlayer(1);
        $this->assertNotNull($player);
        $this->assertEquals(8, $player->getPosition()?->getX());
    }

    public function testValidateMoveAllowsProneStandInPlace(): void
    {
        $state = (new GameStateBuilder())
            ->addPronePlayer(TeamSide::HOME, 5, 7, movement: 6, id: 1)
            ->addPlayer(TeamSide::AWAY, 20, 7, id: 2)
            ->build();

        $rules = new RulesEngine();
        $errors = $rules->validate($state, ActionType::MOVE, [
            'playerId' => 1, 'x' => 5, 'y' => 7,
        ]);

        $this->assertEmpty($errors, 'Prone player should be able to stand in place');
    }

    public function testStandingPlayerUnaffected(): void
    {
        // Standing player moves normally (no stand-up overhead)
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, movement: 6, id: 1)
            ->addPlayer(TeamSide::AWAY, 20, 7, id: 2)
            ->build();

        $dice = new FixedDiceRoller([]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::MOVE, [
            'playerId' => 1, 'x' => 11, 'y' => 7, // 6 squares = full MA
        ]);

        $this->assertTrue($result->isSuccess());
        $player = $result->getNewState()->getPlayer(1);
        $this->assertNotNull($player);
        $this->assertEquals(11, $player->getPosition()?->getX());

        // No stand_up events
        $standUpEvents = array_filter(
            $result->getEvents(),
            fn($e) => $e->getType() === 'stand_up',
        );
        $this->assertEmpty($standUpEvents);
    }

    public function testPronePlayerCannotBlock(): void
    {
        // Prone player adjacent to enemy cannot block (requires STANDING via canAct)
        $state = (new GameStateBuilder())
            ->addPronePlayer(TeamSide::HOME, 5, 7, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 7, id: 2)
            ->build();

        $rules = new RulesEngine();
        $actions = $rules->getAvailableActions($state);

        // No BLOCK action for prone player
        $blockActions = array_filter($actions, fn($a) => $a['type'] === 'block' && ($a['playerId'] ?? null) === 1);
        $this->assertEmpty($blockActions, 'Prone player should not be able to block');
    }
}
