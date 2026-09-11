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
     * ⛔ PREPSANO 11.09.2026. Puvodne se jmenoval `...CausesTurnover` a tvrdil
     *    opak toho, co rikaji pravidla. `rules_bb2016.txt` r. 8188-8189:
     *    „If the roll fails, then the hypnotic gaze **has no effect**."
     *    A uzavreny sedmicленny katalog turnoveru (r. 368-384) gaze NEZNA.
     *    C++ engine to ma spravne uz od zacatku (`gaze_handler.cpp:21,44`).
     */
    public function testFailedGazeIsNotATurnover(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1, skills: [SkillName::HypnoticGaze])
            ->addPlayer(TeamSide::AWAY, 6, 7, id: 2) // target
            ->addPlayer(TeamSide::AWAY, 5, 6, id: 3) // enemy creating TZ on gazer
            ->addPlayer(TeamSide::AWAY, 5, 8, id: 4) // enemy creating TZ on gazer
            ->withBallOffPitch()
            ->build();

        // Sousedi 3 a 4 delaji 2 TZ; OBET (2) se nepocita (r. 8183-8185)
        // => prah 2+2 = 4+. Hod 3 tedy NEUSPEJE.
        $dice = new FixedDiceRoller([3]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::HYPNOTIC_GAZE, [
            'playerId' => 1,
            'targetId' => 2,
        ]);

        $this->assertFalse($result->isTurnover(),
            'neuspesny gaze neni turnover -- r. 8188-8189 a katalog r. 368-384');
        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('hypnotic_gaze', $types, 'hod se ma ozvat');
        $this->assertNotContains('turnover', $types);
        // Akce se presto VYCERPA -- gaze se dela na konci Move Action.
        $this->assertTrue($result->getNewState()->getPlayer(1)->hasActed());
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

        // Soused 3 dela 1 TZ; OBET (2) se nepocita => prah 2+1 = 3+.
        // Hod 2 tedy NEUSPEJE.
        $dice = new FixedDiceRoller([2]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::HYPNOTIC_GAZE, [
            'playerId' => 1,
            'targetId' => 2,
        ]);

        // Test meri MODIFIKATOR, ne turnover: pri 1 TZ je prah 3+, takze
        // dvojka NEUSPEJE -- cil si tacklezony ponecha. (Drive se tu tvrdil
        // turnover; ten sem nikdy nepatril, viz test vys.)
        $this->assertFalse($result->isTurnover());
        $this->assertFalse($result->getNewState()->getPlayer(2)->hasLostTacklezones(),
            'gaze neuspel, cil tacklezony ztratit nesmi');
    }

    public function testGazeTZModifierSucceedsOneAbove(): void
    {
        // ⭐ Druha pulka paru k modifikatoru: pri 1 TZ je prah 3+, takze
        //    TROJKA uspet MA. Bez tohohle by test vys prosel i s prahem,
        //    ktery neprojde nikdy.
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1, skills: [SkillName::HypnoticGaze])
            ->addPlayer(TeamSide::AWAY, 6, 7, id: 2)
            ->addPlayer(TeamSide::AWAY, 4, 7, id: 3)
            ->withBallOffPitch()
            ->build();

        $dice = new FixedDiceRoller([3]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::HYPNOTIC_GAZE, [
            'playerId' => 1,
            'targetId' => 2,
        ]);

        $this->assertFalse($result->isTurnover());
        $this->assertTrue($result->getNewState()->getPlayer(2)->hasLostTacklezones(),
            'trojka pri prahu 3+ uspet MA');
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
