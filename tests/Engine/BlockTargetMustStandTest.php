<?php
declare(strict_types=1);

namespace App\Tests\Engine;

use App\Engine\RulesEngine;
use App\Enum\ActionType;
use App\Enum\TeamSide;
use PHPUnit\Framework\TestCase;

/**
 * `rules_bb2016.txt` r. 540-541: blokovat lze jen STOJICIHO hrace -- lezici
 * ani omraceny se blokovat nesmi. Plati pro Block i pro blok v ramci Blitzu.
 * Nalezeno 25.09.2026 pri oprave PHPStanu: `GreedyAICoach` blitzoval lezici cil.
 */
final class BlockTargetMustStandTest extends TestCase
{
    public function testBlockOnProneTargetIsInvalid(): void
    {
        $s = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, id: 1)
            ->addPronePlayer(TeamSide::AWAY, 6, 5, id: 2)
            ->build();

        $this->assertNotSame([], (new RulesEngine())->validate($s, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]));
    }

    public function testBlitzOnProneTargetIsInvalid(): void
    {
        $s = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, id: 1)
            ->addPronePlayer(TeamSide::AWAY, 7, 5, id: 2)
            ->build();

        $this->assertNotSame([], (new RulesEngine())->validate($s, ActionType::BLITZ, ['playerId' => 1, 'targetId' => 2]));
    }

    /** Pozitivni kontrola: stojici cil projde, jinak by testy vyse merily neco jineho. */
    public function testBlockAndBlitzOnStandingTargetAreValid(): void
    {
        $s = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, id: 2)
            ->build();
        $rules = new RulesEngine();

        $this->assertSame([], $rules->validate($s, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]));
        $this->assertSame([], $rules->validate($s, ActionType::BLITZ, ['playerId' => 1, 'targetId' => 2]));
    }
}
