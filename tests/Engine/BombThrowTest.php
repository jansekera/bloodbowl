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

final class BombThrowTest extends TestCase
{
    /**
     * Accurate bomb knocks down target and adjacent players.
     * Bombardier at (5,7) AG3, target at (8,7) = short pass (distance 3).
     * Short range modifier = +1, so accuracy = 7-3+0-1 = 3+ (need 3+).
     * Enemy players at (8,7) and (9,7).
     */
    public function testAccurateBombKnocksDown(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1, skills: [SkillName::Bombardier])
            ->addPlayer(TeamSide::AWAY, 8, 7, id: 2) // at target
            ->addPlayer(TeamSide::AWAY, 9, 7, id: 3) // adjacent to target
            ->withBallOffPitch()
            ->build();

        // Accuracy roll: 5 (>= 3, accurate)
        // Explosion: player 2 armor: 4+4=8 vs AV8, not broken
        //            player 3 armor: 4+4=8 vs AV8, not broken
        $dice = new FixedDiceRoller([5, 4, 4, 4, 4]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::BOMB_THROW, [
            'playerId' => 1,
            'targetX' => 8,
            'targetY' => 7,
        ]);

        $this->assertFalse($result->isTurnover());
        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('bomb_throw', $types);
        $this->assertContains('bomb_landing', $types);
        $this->assertContains('bomb_explosion', $types);

        // Both players should be prone
        $newState = $result->getNewState();
        $this->assertEquals(PlayerState::PRONE, $newState->getPlayer(2)->getState());
        $this->assertEquals(PlayerState::PRONE, $newState->getPlayer(3)->getState());
    }

    /**
     * Inaccurate bomb scatters 3 times from target.
     */
    public function testInaccurateBombScatters(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1, agility: 2, skills: [SkillName::Bombardier])
            // AG2: accuracy = 7-2+0-1 = 4+ (need 4+)
            ->withBallOffPitch()
            ->build();

        // Accuracy roll: 3 (< 4, inaccurate but not fumble)
        // 3 scatter D8s: 3 (East), 3 (East), 3 (East)
        // Target (8,7) → (9,7) → (10,7) → (11,7)
        // No players at bomb landing — no explosion effects
        $dice = new FixedDiceRoller([3, 3, 3, 3]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::BOMB_THROW, [
            'playerId' => 1,
            'targetX' => 8,
            'targetY' => 7,
        ]);

        $this->assertFalse($result->isTurnover());
        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('bomb_throw', $types);
        $this->assertContains('bomb_landing', $types);
    }

    /**
     * Fumble scatters from thrower (1 scatter).
     */
    public function testFumbleScattersFromThrower(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1, skills: [SkillName::Bombardier])
            ->addPlayer(TeamSide::AWAY, 6, 7, id: 2) // adjacent, could be hit by fumbled bomb
            ->withBallOffPitch()
            ->build();

        // Accuracy roll: 1 (fumble)
        // Scatter D8: 3 (East) → bomb at (6,7) where player 2 is
        // Explosion: player 2 armor: 4+4=8 vs AV8, not broken
        $dice = new FixedDiceRoller([1, 3, 4, 4]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::BOMB_THROW, [
            'playerId' => 1,
            'targetX' => 8,
            'targetY' => 7,
        ]);

        // Bomb never causes turnover, even on fumble
        $this->assertFalse($result->isTurnover());
        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('bomb_throw', $types);

        // Player 2 should be knocked down
        $newState = $result->getNewState();
        $this->assertEquals(PlayerState::PRONE, $newState->getPlayer(2)->getState());
    }

    /**
     * Bomb doesn't cause turnover even on fumble.
     */
    public function testBombNeverCausesTurnover(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1, skills: [SkillName::Bombardier])
            ->withBallOffPitch()
            ->build();

        // Fumble: roll 1, scatter D8=1 (North), bomb at (5,6) — empty
        $dice = new FixedDiceRoller([1, 1]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::BOMB_THROW, [
            'playerId' => 1,
            'targetX' => 8,
            'targetY' => 7,
        ]);

        $this->assertFalse($result->isTurnover());
    }

    /**
     * Thrower not affected by own bomb.
     */
    public function testThrowerNotAffectedByOwnBomb(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1, skills: [SkillName::Bombardier])
            ->withBallOffPitch()
            ->build();

        // Fumble: roll 1, scatter D8=7 (West) → bomb at (4,7) — adjacent to thrower at (5,7)
        // Thrower is in 3x3 blast area but should not be affected
        $dice = new FixedDiceRoller([1, 7]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::BOMB_THROW, [
            'playerId' => 1,
            'targetX' => 8,
            'targetY' => 7,
        ]);

        $newState = $result->getNewState();
        $this->assertEquals(PlayerState::STANDING, $newState->getPlayer(1)->getState());
    }

    /**
     * Armor roll on knocked-down players — armor broken causes injury.
     */
    public function testBombArmorRollCausesInjury(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1, skills: [SkillName::Bombardier])
            ->addPlayer(TeamSide::AWAY, 8, 7, id: 2, armour: 6) // low armor
            ->withBallOffPitch()
            ->build();

        // Accuracy: 5 (accurate)
        // Player 2 armor: 6+6=12 vs AV6 → broken
        // Injury: 4+4=8 → KO
        $dice = new FixedDiceRoller([5, 6, 6, 4, 4]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::BOMB_THROW, [
            'playerId' => 1,
            'targetX' => 8,
            'targetY' => 7,
        ]);

        $newState = $result->getNewState();
        $this->assertEquals(PlayerState::KO, $newState->getPlayer(2)->getState());
    }
}
