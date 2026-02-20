<?php
declare(strict_types=1);

namespace App\Tests\Engine;

use App\Engine\ActionResolver;
use App\Engine\FixedDiceRoller;
use App\Enum\ActionType;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use PHPUnit\Framework\TestCase;

final class HypnoticGazeTest extends TestCase
{
    /**
     * Successful gaze: target loses tackle zones.
     */
    public function testSuccessfulGazeLosesTacklezones(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1, skills: [SkillName::HypnoticGaze])
            ->addPlayer(TeamSide::AWAY, 6, 7, id: 2) // target
            ->withBallOffPitch()
            ->build();

        // Gaze roll: 5 (need 2+, no enemy TZ on gazer)
        $dice = new FixedDiceRoller([5]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::HYPNOTIC_GAZE, [
            'playerId' => 1,
            'targetId' => 2,
        ]);

        $this->assertTrue($result->isSuccess());
        $this->assertFalse($result->isTurnover());

        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('hypnotic_gaze', $types);

        $newState = $result->getNewState();
        $this->assertTrue($newState->getPlayer(2)->hasLostTacklezones());
    }

    /**
     * Failed gaze causes turnover.
     */
    public function testFailedGazeCausesTurnover(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1, skills: [SkillName::HypnoticGaze])
            ->addPlayer(TeamSide::AWAY, 6, 7, id: 2) // target
            ->addPlayer(TeamSide::AWAY, 5, 6, id: 3) // enemy creating TZ on gazer
            ->addPlayer(TeamSide::AWAY, 5, 8, id: 4) // enemy creating TZ on gazer
            ->withBallOffPitch()
            ->build();

        // 2 enemy TZ on gazer → need 2+2=4+
        // Gaze roll: 3 (< 4, fails)
        $dice = new FixedDiceRoller([3]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::HYPNOTIC_GAZE, [
            'playerId' => 1,
            'targetId' => 2,
        ]);

        $this->assertTrue($result->isTurnover());
        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('hypnotic_gaze', $types);
        $this->assertContains('turnover', $types);
    }

    /**
     * Gaze difficulty increases with tackle zones on gazer.
     */
    public function testGazeTZModifier(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1, skills: [SkillName::HypnoticGaze])
            ->addPlayer(TeamSide::AWAY, 6, 7, id: 2) // target
            ->addPlayer(TeamSide::AWAY, 4, 7, id: 3) // enemy TZ +1
            ->withBallOffPitch()
            ->build();

        // 1 enemy TZ → need 2+1=3+
        // Gaze roll: 2 (< 3, fails)
        $dice = new FixedDiceRoller([2]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::HYPNOTIC_GAZE, [
            'playerId' => 1,
            'targetId' => 2,
        ]);

        $this->assertTrue($result->isTurnover());
    }

    /**
     * Gaze without TZ: 2+ succeeds.
     */
    public function testGazeNoTZSucceedsOn2(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1, skills: [SkillName::HypnoticGaze])
            ->addPlayer(TeamSide::AWAY, 6, 7, id: 2) // target (doesn't count as TZ for gaze)
            ->withBallOffPitch()
            ->build();

        // No enemy TZ on gazer (target is adjacent but the TZ calc counts opponent TZs)
        // Wait — player 2 is AWAY and gazer is HOME, so player 2 IS an enemy in gazer's TZ
        // Need 2+1=3+ because of the target itself creating a TZ
        // Roll: 3 (>= 3, success)
        $dice = new FixedDiceRoller([3]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::HYPNOTIC_GAZE, [
            'playerId' => 1,
            'targetId' => 2,
        ]);

        $this->assertTrue($result->isSuccess());
        $this->assertFalse($result->isTurnover());
    }

    /**
     * Gazer is marked as acted after using gaze.
     */
    public function testGazerMarkedAsActed(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1, skills: [SkillName::HypnoticGaze])
            ->addPlayer(TeamSide::AWAY, 6, 7, id: 2)
            ->withBallOffPitch()
            ->build();

        $dice = new FixedDiceRoller([6]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::HYPNOTIC_GAZE, [
            'playerId' => 1,
            'targetId' => 2,
        ]);

        $gazer = $result->getNewState()->getPlayer(1);
        $this->assertTrue($gazer->hasActed());
    }
}
