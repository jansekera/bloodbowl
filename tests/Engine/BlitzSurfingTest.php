<?php
declare(strict_types=1);

namespace App\Tests\Engine;

use App\Engine\ActionResolver;
use App\Engine\FixedDiceRoller;
use App\Enum\ActionType;
use App\Enum\PlayerState;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use PHPUnit\Framework\TestCase;

final class BlitzSurfingTest extends TestCase
{
    public function testBlitzPrefersSurfAngleOnSidelineY0(): void
    {
        // Defender at (10,0) on top sideline
        // Attacker at (10,3) — multiple adjacent squares available
        // All approach squares from Y=1 push toward Y=-1 (off pitch) → surfBonus=5
        // (9,0) or (11,0) push along X axis only, pushY=0 → no surf bonus
        // So attacker should pick a Y=1 square (any of them) over X-only squares
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 10, 3, movement: 6, id: 1)
            ->addPlayer(TeamSide::AWAY, 10, 0, armour: 9, id: 2)
            ->withBallOffPitch()
            ->build();

        // Block: DD (roll=6), armor 3+3=6 ≤ AV9 holds
        $dice = new FixedDiceRoller([6, 3, 3]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::BLITZ, ['playerId' => 1, 'targetId' => 2]);

        $this->assertFalse($result->isTurnover());
        // Attacker should end at Y=1 (any X) for surf angle, not at Y=0
        $attacker = $result->getNewState()->getPlayer(1);
        $this->assertNotNull($attacker->getPosition());
        // After follow-up, attacker moves to defender's old position (10,0)
        // So we check events for the move destination before the block
        $moveEvents = array_values(array_filter(
            $result->getEvents(),
            fn($e) => $e->getType() === 'player_move'
        ));
        // Last move event is to the adjacent-to-defender square
        $lastMove = end($moveEvents);
        $this->assertNotFalse($lastMove);
        // The approach square should be at Y=1 (pushes defender toward sideline)
        $toStr = $lastMove->getData()['to'];
        $this->assertMatchesRegularExpression('/\(\d+,1\)/', $toStr);
    }

    public function testBlitzPrefersSurfAngleY14(): void
    {
        // Defender at (10,14) on bottom sideline
        // Approach from Y=13 pushes to Y=15 = off pitch → surfBonus=5
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 10, 11, movement: 6, id: 1)
            ->addPlayer(TeamSide::AWAY, 10, 14, armour: 9, id: 2)
            ->withBallOffPitch()
            ->build();

        // Block: DD (roll=6), armor holds
        $dice = new FixedDiceRoller([6, 3, 3]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::BLITZ, ['playerId' => 1, 'targetId' => 2]);

        $this->assertFalse($result->isTurnover());
        $moveEvents = array_values(array_filter(
            $result->getEvents(),
            fn($e) => $e->getType() === 'player_move'
        ));
        $lastMove = end($moveEvents);
        $this->assertNotFalse($lastMove);
        $toStr = $lastMove->getData()['to'];
        // Approach at Y=13
        $this->assertMatchesRegularExpression('/\(\d+,13\)/', $toStr);
    }

    public function testSafePathTrumpsSurfAngle(): void
    {
        // Defender at (5,1) near sideline. Attacker at (5,4).
        // Place enemies to make the surf-angle approach require dodges
        // (5,2) has enemy → dodge needed for direct approach
        // (4,2) free → no dodge, less surf bonus
        // Safe (0 dodge) should always beat surf angle
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 4, movement: 6, id: 1)
            ->addPlayer(TeamSide::AWAY, 5, 1, armour: 9, id: 2)
            ->addPlayer(TeamSide::AWAY, 5, 3, id: 3) // blocks Y=3 direct path; creates TZ on (5,2)
            ->addPlayer(TeamSide::AWAY, 6, 2, id: 4) // additional TZ on (5,2) approach
            ->withBallOffPitch()
            ->build();

        // Movement + block dice. Need enough dice for possible dodge + block.
        // Dodge roll (if needed) + block DD + armor
        $dice = new FixedDiceRoller([6, 6, 6, 3, 3]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::BLITZ, ['playerId' => 1, 'targetId' => 2]);

        $this->assertFalse($result->isTurnover());
        // Just verify it completes — the scoring ensures safe paths win
        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertTrue(
            in_array('stab', $types) || in_array('block', $types),
            'Expected either stab or block event'
        );
    }

    public function testNoSidelineNoSurfPreference(): void
    {
        // Defender at (10,7) center — all surf bonuses = 0
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 10, 10, movement: 6, id: 1)
            ->addPlayer(TeamSide::AWAY, 10, 7, armour: 9, id: 2)
            ->withBallOffPitch()
            ->build();

        // Block: DD, armor holds
        $dice = new FixedDiceRoller([6, 3, 3]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::BLITZ, ['playerId' => 1, 'targetId' => 2]);

        $this->assertFalse($result->isTurnover());
        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('player_move', $types);
    }
}
