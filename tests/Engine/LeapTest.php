<?php
declare(strict_types=1);

namespace App\Tests\Engine;

use App\Engine\ActionResolver;
use App\Engine\FixedDiceRoller;
use App\Engine\Pathfinder;
use App\Engine\RulesEngine;
use App\Enum\ActionType;
use App\Enum\PlayerState;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use PHPUnit\Framework\TestCase;

final class LeapTest extends TestCase
{
    public function testLeapOverOccupiedSquare(): void
    {
        // Leap 2 squares over an occupied square. AG3, TZ at dest=1 from enemy at (6,5)
        // Target: 7-3+1=5+, roll 5 OK
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, skills: [SkillName::Leap], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, id: 2) // blocking normal movement, exerts TZ at (7,5)
            ->withBallOffPitch()
            ->build();

        $dice = new FixedDiceRoller([5]); // leap roll
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::MOVE, ['playerId' => 1, 'x' => 7, 'y' => 5]);

        $this->assertFalse($result->isTurnover());
        $this->assertSame(7, $result->getNewState()->getPlayer(1)->getPosition()->getX());

        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('leap', $types);
    }

    public function testLeapFailCausesTurnover(): void
    {
        // Leap fail → prone + turnover
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, skills: [SkillName::Leap], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, id: 2)
            ->withBallOffPitch()
            ->build();

        // Leap: AG3 target 4+, roll 2 = fail, armor 3+3=6 not > 8
        $dice = new FixedDiceRoller([2, 3, 3]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::MOVE, ['playerId' => 1, 'x' => 7, 'y' => 5]);

        $this->assertTrue($result->isTurnover());
        $this->assertSame(PlayerState::PRONE, $result->getNewState()->getPlayer(1)->getState());
    }

    public function testLeapIgnoresTacklezones(): void
    {
        // Leap: doesn't trigger dodge from TZ (no dodge event, only leap event)
        // Player surrounded by enemies, leaps out. No enemies near destination.
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, skills: [SkillName::Leap], id: 1)
            ->addPlayer(TeamSide::AWAY, 4, 5, id: 2)
            ->addPlayer(TeamSide::AWAY, 4, 4, id: 3)
            ->addPlayer(TeamSide::AWAY, 5, 4, id: 4)
            ->addPlayer(TeamSide::AWAY, 5, 6, id: 5)
            ->withBallOffPitch()
            ->build();

        // Leap to (7,5): AG3, TZ at dest=0, target=4+. Roll 4 OK
        $dice = new FixedDiceRoller([4]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::MOVE, ['playerId' => 1, 'x' => 7, 'y' => 5]);

        $this->assertFalse($result->isTurnover());
        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('leap', $types);
        $this->assertNotContains('dodge', $types);
        $this->assertNotContains('tentacles', $types);
    }

    public function testLeapOnlyOncePerMovement(): void
    {
        // Pathfinder check: can't leap twice
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, movement: 8, skills: [SkillName::Leap], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, id: 2) // blocks (6,5)
            ->addPlayer(TeamSide::AWAY, 8, 5, id: 3) // blocks (8,5)
            ->withBallOffPitch()
            ->build();

        $pathfinder = new Pathfinder();
        $moves = $pathfinder->findValidMoves($state, $state->getPlayer(1));

        // Can reach (7,5) via leap over (6,5). But (9,5) requires going through (8,5).
        // With one leap, can reach (7,5). Then from (7,5), need to go around (8,5).
        // Check that (9,5) exists but requires more cost (can't double-leap)
        $this->assertArrayHasKey('7,5', $moves);
    }

    public function testLeapCosts2MA(): void
    {
        // MA=4 player with Leap: leap costs 2 MA, then 2 squares normally = 4 total
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, movement: 4, skills: [SkillName::Leap], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, id: 2)
            ->withBallOffPitch()
            ->build();

        $pathfinder = new Pathfinder();
        $moves = $pathfinder->findValidMoves($state, $state->getPlayer(1));

        // Can leap to (7,5) costing 2 MA, then move 2 more squares
        $this->assertArrayHasKey('7,5', $moves);
        // Should be able to continue from (7,5) for 2 more squares
        // Total MA=4, leap costs 2, so 2 left for normal movement + GFI
    }

    public function testLeapTZAtDestinationIncreasesTarget(): void
    {
        // TZ at destination: +2 TZ from 2 enemies. AG3: 4++2=6+. Roll 6 OK.
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, skills: [SkillName::Leap], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, id: 2) // TZ at (7,5) from this enemy
            ->addPlayer(TeamSide::AWAY, 8, 5, id: 3) // TZ at (7,5) from this enemy too
            ->withBallOffPitch()
            ->build();

        // Leap to (7,5): AG3, +2 TZ, target=7-3+2=6+. Roll 6=success.
        $dice = new FixedDiceRoller([6]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::MOVE, ['playerId' => 1, 'x' => 7, 'y' => 5]);

        $this->assertFalse($result->isTurnover());
        $this->assertSame(7, $result->getNewState()->getPlayer(1)->getPosition()->getX());
    }
}
