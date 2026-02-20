<?php
declare(strict_types=1);

namespace App\Tests\Engine;

use App\Engine\ActionResolver;
use App\Engine\FixedDiceRoller;
use App\Enum\ActionType;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use PHPUnit\Framework\TestCase;

final class PassBlockTest extends TestCase
{
    public function testPassBlockPlayerMovesTowardReceiver(): void
    {
        // PB player at (8,3), within 3 of target (7,5)
        // Should move up to 3 squares toward target
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, agility: 3, id: 1)   // thrower
            ->addPlayer(TeamSide::HOME, 7, 5, agility: 3, id: 2)   // receiver
            ->addPlayer(TeamSide::AWAY, 8, 3, skills: [SkillName::PassBlock], id: 10) // PB player
            ->withBallCarried(1)
            ->build();

        // Accuracy: 7-3-1(quick) = 3+, roll 4 = accurate
        // Catch: 3+, roll 4 = success
        $dice = new FixedDiceRoller([4, 4]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::PASS, [
            'playerId' => 1,
            'targetX' => 7,
            'targetY' => 5,
        ]);

        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('pass_block', $types);

        // PB player should have moved closer to (7,5)
        $pbPlayer = $result->getNewState()->getPlayer(10);
        $this->assertNotNull($pbPlayer->getPosition());
        // Original pos (8,3), target (7,5), should move diag toward target
        $this->assertLessThan(
            abs(8 - 7) + abs(3 - 5), // original distance
            abs($pbPlayer->getPosition()->getX() - 7) + abs($pbPlayer->getPosition()->getY() - 5)
        );
    }

    public function testPassBlockPlayerTooFarNoMovement(): void
    {
        // PB player at (20,12) — far from both thrower (5,5) and target (7,5)
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, agility: 3, id: 1)   // thrower
            ->addPlayer(TeamSide::HOME, 7, 5, agility: 3, id: 2)   // receiver
            ->addPlayer(TeamSide::AWAY, 20, 12, skills: [SkillName::PassBlock], id: 10) // PB player far away
            ->withBallCarried(1)
            ->build();

        // Accuracy: 3+, roll 4; Catch: 3+, roll 4
        $dice = new FixedDiceRoller([4, 4]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::PASS, [
            'playerId' => 1,
            'targetX' => 7,
            'targetY' => 5,
        ]);

        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertNotContains('pass_block', $types);

        // PB player unchanged
        $pbPlayer = $result->getNewState()->getPlayer(10);
        $this->assertEquals(20, $pbPlayer->getPosition()->getX());
        $this->assertEquals(12, $pbPlayer->getPosition()->getY());
    }

    public function testPassBlockMovesUpToThreeSquares(): void
    {
        // PB player at (7,2), target at (7,10) — within 3 of thrower at (5,5)
        // Can only move 3 squares
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, agility: 3, id: 1)   // thrower
            ->addPlayer(TeamSide::HOME, 7, 10, agility: 3, id: 2)  // receiver
            ->addPlayer(TeamSide::AWAY, 5, 3, skills: [SkillName::PassBlock], id: 10) // within 3 of thrower
            ->withBallCarried(1)
            ->build();

        // Accuracy: 7-3+1(long) = 5+, roll 5 = accurate
        // Catch: 3+, roll 4
        $dice = new FixedDiceRoller([5, 4]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::PASS, [
            'playerId' => 1,
            'targetX' => 7,
            'targetY' => 10,
        ]);

        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('pass_block', $types);

        // PB moved at most 3 squares from (5,3) toward (7,10)
        $pbPlayer = $result->getNewState()->getPlayer(10);
        $originalDist = max(abs(5 - $pbPlayer->getPosition()->getX()), abs(3 - $pbPlayer->getPosition()->getY()));
        $this->assertLessThanOrEqual(3, $originalDist);
    }

    public function testPassBlockBlockedByOccupiedSquare(): void
    {
        // PB player at (8,5), target at (7,5), but (7,5) is occupied by receiver
        // PB player can't move into occupied square
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, agility: 3, id: 1)   // thrower
            ->addPlayer(TeamSide::HOME, 7, 5, agility: 3, id: 2)   // receiver at target
            ->addPlayer(TeamSide::AWAY, 8, 5, skills: [SkillName::PassBlock], id: 10) // PB player adjacent to target
            ->withBallCarried(1)
            ->build();

        // Accuracy: 3+, roll 4; Catch: 3+, roll 4
        $dice = new FixedDiceRoller([4, 4]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::PASS, [
            'playerId' => 1,
            'targetX' => 7,
            'targetY' => 5,
        ]);

        // PB player at (8,5) wants to move to (7,5) but it's occupied — no movement
        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertNotContains('pass_block', $types);

        $pbPlayer = $result->getNewState()->getPlayer(10);
        $this->assertEquals(8, $pbPlayer->getPosition()->getX());
        $this->assertEquals(5, $pbPlayer->getPosition()->getY());
    }

    public function testOnlyOnePassBlockPlayerMoves(): void
    {
        // Two PB players — only one (closest to target) moves
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, agility: 3, id: 1)   // thrower
            ->addPlayer(TeamSide::HOME, 9, 5, agility: 3, id: 2)   // receiver
            ->addPlayer(TeamSide::AWAY, 7, 3, skills: [SkillName::PassBlock], id: 10) // dist to (9,5)=3
            ->addPlayer(TeamSide::AWAY, 7, 8, skills: [SkillName::PassBlock], id: 11) // dist to (9,5)=4
            ->withBallCarried(1)
            ->build();

        // PB: player 10 is closer to target (dist 3 vs 4) — only 10 moves
        // Accuracy: 7-3-0(short)=4+, roll 5; Catch with TZ: 7-3+1-1=4+, roll 5
        $dice = new FixedDiceRoller([5, 5]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::PASS, [
            'playerId' => 1,
            'targetX' => 9,
            'targetY' => 5,
        ]);

        $passBlockEvents = array_filter(
            $result->getEvents(),
            fn($e) => $e->getType() === 'pass_block'
        );
        // Only 1 PB player moves per pass
        $this->assertCount(1, $passBlockEvents);

        // Player 10 moved, player 11 didn't
        $pb10 = $result->getNewState()->getPlayer(10);
        $pb11 = $result->getNewState()->getPlayer(11);
        $this->assertNotEquals(7, $pb10->getPosition()->getX()); // moved
        $this->assertEquals(7, $pb11->getPosition()->getX());    // unchanged
    }

    public function testPassBlockWithinThreeOfThrowerQualifies(): void
    {
        // PB player at (6,3) — within 3 of thrower at (5,5), but far from target (15,5)
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, agility: 3, id: 1)   // thrower
            ->addPlayer(TeamSide::HOME, 15, 5, agility: 3, id: 2)  // receiver far away
            ->addPlayer(TeamSide::AWAY, 6, 3, skills: [SkillName::PassBlock], id: 10) // within 3 of thrower
            ->withBallCarried(1)
            ->build();

        // Long pass: 7-3+2(long bomb)=6+, roll 6
        // Catch: 3+, roll 4
        $dice = new FixedDiceRoller([6, 4]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::PASS, [
            'playerId' => 1,
            'targetX' => 15,
            'targetY' => 5,
        ]);

        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('pass_block', $types);
    }
}
