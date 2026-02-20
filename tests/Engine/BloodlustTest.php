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

final class BloodlustTest extends TestCase
{
    /**
     * Bloodlust roll 2+ passes: action proceeds normally.
     */
    public function testBloodlustPassesOn2Plus(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1, skills: [SkillName::Bloodlust])
            ->addPlayer(TeamSide::HOME, 6, 7, id: 2) // Thrall teammate
            ->withBallOnGround(7, 7) // something to move toward
            ->build();

        // Bloodlust roll: 2 (passes)
        // Move — no dodges/GFIs needed
        $dice = new FixedDiceRoller([2]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::MOVE, [
            'playerId' => 1,
            'x' => 4,
            'y' => 7,
        ]);

        $this->assertTrue($result->isSuccess());
        $newState = $result->getNewState();
        $pos = $newState->getPlayer(1)->getPosition();
        $this->assertNotNull($pos);
        $this->assertEquals(4, $pos->getX());
        $this->assertEquals(7, $pos->getY());
    }

    /**
     * Bloodlust fail: bite adjacent Thrall, action still proceeds.
     */
    public function testBloodlustBiteThrall(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1, skills: [SkillName::Bloodlust])
            ->addPlayer(TeamSide::HOME, 6, 7, id: 2) // Thrall teammate (no Bloodlust)
            ->withBallOnGround(3, 7)
            ->build();

        // Bloodlust roll: 1 (fail) → bite Thrall
        // Then move action proceeds
        $dice = new FixedDiceRoller([1]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::MOVE, [
            'playerId' => 1,
            'x' => 4,
            'y' => 7,
        ]);

        $this->assertTrue($result->isSuccess());
        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('bloodlust_bite', $types);

        $newState = $result->getNewState();
        // Thrall sent to KO
        $thrall = $newState->getPlayer(2);
        $this->assertEquals(PlayerState::KO, $thrall->getState());
        $this->assertNull($thrall->getPosition());

        // Vampire still moved
        $vampirePos = $newState->getPlayer(1)->getPosition();
        $this->assertNotNull($vampirePos);
        $this->assertEquals(4, $vampirePos->getX());
    }

    /**
     * Bloodlust fail with no Thrall: vampire loses action, moved to reserves.
     */
    public function testBloodlustFailNoThrall(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1, skills: [SkillName::Bloodlust])
            // No adjacent Thralls
            ->withBallOffPitch()
            ->build();

        // Bloodlust roll: 1 (fail), no Thrall → off pitch
        $dice = new FixedDiceRoller([1]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::MOVE, [
            'playerId' => 1,
            'x' => 4,
            'y' => 7,
        ]);

        $this->assertTrue($result->isSuccess());
        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('bloodlust_fail', $types);

        $newState = $result->getNewState();
        $vampire = $newState->getPlayer(1);
        $this->assertEquals(PlayerState::OFF_PITCH, $vampire->getState());
        $this->assertNull($vampire->getPosition());
    }

    /**
     * Another Vampire adjacent doesn't count as Thrall (has Bloodlust).
     */
    public function testBloodlustVampireNotThrall(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1, skills: [SkillName::Bloodlust])
            ->addPlayer(TeamSide::HOME, 6, 7, id: 2, skills: [SkillName::Bloodlust]) // Another Vampire
            ->withBallOffPitch()
            ->build();

        // Bloodlust roll: 1 (fail), adjacent player is Vampire (has Bloodlust) → no Thrall → reserves
        $dice = new FixedDiceRoller([1]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::MOVE, [
            'playerId' => 1,
            'x' => 4,
            'y' => 7,
        ]);

        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('bloodlust_fail', $types);
        $this->assertNotContains('bloodlust_bite', $types);
    }

    /**
     * Bloodlust applies to block action too.
     */
    public function testBloodlustOnBlock(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1, skills: [SkillName::Bloodlust])
            ->addPlayer(TeamSide::HOME, 5, 6, id: 2) // Thrall
            ->addPlayer(TeamSide::AWAY, 6, 7, id: 3) // block target
            ->withBallOffPitch()
            ->build();

        // Bloodlust: 1 (fail) → bite Thrall → block proceeds
        // Block: 1 die, roll 6 → DEFENDER_DOWN
        // Armor: 4+4=8 vs AV8, not broken
        $dice = new FixedDiceRoller([1, 6, 4, 4]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::BLOCK, [
            'playerId' => 1,
            'targetId' => 3,
        ]);

        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('bloodlust_bite', $types);
        $this->assertContains('block', $types);

        // Thrall KO'd
        $this->assertEquals(PlayerState::KO, $result->getNewState()->getPlayer(2)->getState());
    }
}
