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
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1, agility: 4, skills: [SkillName::HypnoticGaze])
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
        $this->assertTrue($newState->requirePlayer(2)->hasLostTacklezones());
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
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1, agility: 4, skills: [SkillName::HypnoticGaze])
            ->addPlayer(TeamSide::AWAY, 6, 7, id: 2) // target
            ->addPlayer(TeamSide::AWAY, 5, 6, id: 3) // enemy creating TZ on gazer
            ->addPlayer(TeamSide::AWAY, 5, 8, id: 4) // enemy creating TZ on gazer
            ->withBallOffPitch()
            ->build();

        // Sousedi 3 a 4 delaji 2 TZ; OBET (2) se nepocita (r. 8183-8185)
        // => Agility roll AG4: 7-4+2 = 5+. Hod 3 tedy NEUSPEJE.
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
        $this->assertTrue($result->getNewState()->requirePlayer(1)->hasActed());
    }

    /**
     * Gaze difficulty increases with tackle zones on gazer.
     */
    public function testGazeTZModifier(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1, agility: 4, skills: [SkillName::HypnoticGaze])
            ->addPlayer(TeamSide::AWAY, 6, 7, id: 2) // target
            ->addPlayer(TeamSide::AWAY, 4, 7, id: 3) // enemy TZ +1
            ->withBallOffPitch()
            ->build();

        // Soused 3 dela 1 TZ; OBET (2) se nepocita => AG4: 7-4+1 = 4+.
        // Hod 3 tedy NEUSPEJE.
        $dice = new FixedDiceRoller([3]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::HYPNOTIC_GAZE, [
            'playerId' => 1,
            'targetId' => 2,
        ]);

        // Test meri MODIFIKATOR, ne turnover: pri 1 TZ je prah 3+, takze
        // dvojka NEUSPEJE -- cil si tacklezony ponecha. (Drive se tu tvrdil
        // turnover; ten sem nikdy nepatril, viz test vys.)
        $this->assertFalse($result->isTurnover());
        $this->assertFalse($result->getNewState()->requirePlayer(2)->hasLostTacklezones(),
            'gaze neuspel, cil tacklezony ztratit nesmi');
    }

    public function testGazeTZModifierSucceedsOneAbove(): void
    {
        // ⭐ Druha pulka paru k modifikatoru: pri 1 TZ je prah 4+ (AG4), takze
        //    CTYRKA uspet MA. Bez tohohle by test vys prosel i s prahem,
        //    ktery neprojde nikdy.
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1, agility: 4, skills: [SkillName::HypnoticGaze])
            ->addPlayer(TeamSide::AWAY, 6, 7, id: 2)
            ->addPlayer(TeamSide::AWAY, 4, 7, id: 3)
            ->withBallOffPitch()
            ->build();

        $dice = new FixedDiceRoller([4]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::HYPNOTIC_GAZE, [
            'playerId' => 1,
            'targetId' => 2,
        ]);

        $this->assertFalse($result->isTurnover());
        $this->assertTrue($result->getNewState()->requirePlayer(2)->hasLostTacklezones(),
            'trojka pri prahu 3+ uspet MA');
    }

    /**
     * Gaze without TZ: 2+ succeeds.
     */
    public function testGazeNoTZSucceedsOn2(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1, agility: 4, skills: [SkillName::HypnoticGaze])
            ->addPlayer(TeamSide::AWAY, 6, 7, id: 2) // target (doesn't count as TZ for gaze)
            ->withBallOffPitch()
            ->build();

        // Obet se do modifikatoru nepocita (r. 8183-8185) => AG4 bez zon: 3+
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
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1, agility: 4, skills: [SkillName::HypnoticGaze])
            ->addPlayer(TeamSide::AWAY, 6, 7, id: 2)
            ->withBallOffPitch()
            ->build();

        $dice = new FixedDiceRoller([6]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::HYPNOTIC_GAZE, [
            'playerId' => 1,
            'targetId' => 2,
        ]);

        $gazer = $result->getNewState()->requirePlayer(1);
        $this->assertTrue($gazer->hasActed());
    }

    /** Je to Agility roll (r. 8182): AG2 bez zon potrebuje 5+ -- hod 4 neuspeje (drive fixni 2+). */
    public function testGazeTargetDependsOnAgility(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, agility: 2, skills: [SkillName::HypnoticGaze], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 7, id: 2)
            ->build();

        $result = (new ActionResolver(new FixedDiceRoller([4])))->resolve($state, ActionType::HYPNOTIC_GAZE, ['playerId' => 1, 'targetId' => 2]);

        $this->assertFalse($result->getNewState()->requirePlayer(2)->hasLostTacklezones());
    }

    /** Efekt plati "until the start of his next action" (r. 8187-8188): akci se zony vraci, neaktivace ne. */
    public function testGazeEffectEndsAtVictimsNextAction(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::AWAY, 10, 7, id: 1)
            ->addPlayer(TeamSide::HOME, 20, 7, id: 2)
            ->withActiveTeam(TeamSide::AWAY)
            ->withBallOffPitch()
            ->build();
        $state = $state->withPlayer($state->requirePlayer(1)->withLostTacklezones(true));
        $resolver = new ActionResolver(new FixedDiceRoller([]));

        $stoji = $resolver->resolve($state, ActionType::STAND_PAT, ['playerId' => 1]);
        $this->assertTrue($stoji->getNewState()->requirePlayer(1)->hasLostTacklezones(), 'neaktivace akci neni');

        $pohyb = $resolver->resolve($state, ActionType::MOVE, ['playerId' => 1, 'x' => 11, 'y' => 7]);
        $this->assertFalse($pohyb->getNewState()->requirePlayer(1)->hasLostTacklezones());
    }
}
