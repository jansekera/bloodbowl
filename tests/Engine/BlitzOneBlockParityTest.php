<?php

declare(strict_types=1);

namespace App\Tests\Engine;

use App\DTO\GameState;
use App\Engine\ActionResolver;
use App\Engine\FixedDiceRoller;
use App\Engine\RulesEngine;
use App\Enum\ActionType;
use App\Enum\PlayerState;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use PHPUnit\Framework\TestCase;

/**
 * P210 (audit parity 16.10.2026), převod C++ P209 do PHP.
 *  (1)(2) V blitzu jen jedna rána (ř. 347-350 „one block during the move“) — po ní ani další Block.
 *  (3) Both Down, který útočník s Blockem ustál, aktivaci blitzu nezavírá (ř. 551-552 „carry on moving“).
 *  (4) Foul Appearance v blitzu: propadá RÁNA, ne akce (ř. 8131-8132 „too revolted to make the block
 *      and it is wasted“; srov. „the Action is wasted“ u Wild Animal ř. 8669) ⇒ smí dál běžet; pole za
 *      ránu je zaplacené (uživatel 15.10.).
 */
final class BlitzOneBlockParityTest extends TestCase
{
    /**
     * @param list<SkillName> $attackerSkills
     * @param list<SkillName> $defenderSkills
     */
    private function state(array $attackerSkills = [], array $defenderSkills = []): GameState
    {
        return (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, skills: $attackerSkills, id: 1)
            ->addPlayer(TeamSide::HOME, 4, 9, id: 3)          // drží hráče 4 v zóně ⇒ ten neasistuje
            ->addPlayer(TeamSide::AWAY, 6, 7, skills: $defenderSkills, id: 2)
            ->addPlayer(TeamSide::AWAY, 5, 8, id: 4)          // druhý soupeř vedle útočníka
            ->withBallOffPitch()
            ->build();
    }

    /** @return list<array<string, mixed>> */
    private function actionsOf(GameState $state, int $playerId, ActionType $type): array
    {
        return array_values(array_filter(
            (new RulesEngine())->getAvailableActions($state),
            fn($a) => $a['type'] === $type->value && ($a['playerId'] ?? null) === $playerId,
        ));
    }

    public function testAfterTheBlitzBlockNoSecondBlockIsOffered(): void
    {
        // POW na hráče 2 (6), brnění 1+1; hráč 4 v (5,8) stojí dál vedle útočníka
        $r = (new ActionResolver(new FixedDiceRoller([6, 1, 1, 1, 1])))
            ->resolve($this->state(), ActionType::BLITZ, ['playerId' => 1, 'targetId' => 2]);
        $after = $r->getNewState();
        $this->assertSame(PlayerState::PRONE, $after->requirePlayer(2)->getState(), 'fixtura: obránce leží');
        $this->assertEmpty($this->actionsOf($after, 1, ActionType::BLOCK), 'druhá rána v blitzu (ř. 347-350)');
        $this->assertEmpty($this->actionsOf($after, 1, ActionType::MULTIPLE_BLOCK));
        $this->assertEmpty($this->actionsOf($after, 1, ActionType::BLITZ));
    }

    public function testBothDownWithBlockKeepsTheBlitzOpen(): void
    {
        // jedna kostka (ST 3 × 3): 2 = Both Down; útočník má Block ⇒ stojí; obránce padá, brnění 1+1
        $r = (new ActionResolver(new FixedDiceRoller([2, 1, 1, 1, 1])))
            ->resolve($this->state([SkillName::Block]), ActionType::BLITZ, ['playerId' => 1, 'targetId' => 2]);
        $after = $r->getNewState();
        $this->assertSame(PlayerState::STANDING, $after->requirePlayer(1)->getState(), 'fixtura: útočník stojí');
        $this->assertNotEmpty($this->actionsOf($after, 1, ActionType::MOVE), 'po Both Down smí blitzující dál běžet');
    }

    public function testFoulAppearanceWastesOnlyTheBlockNotTheBlitz(): void
    {
        // Foul Appearance: 1 = útočník se zhnusí, rána propadne
        $r = (new ActionResolver(new FixedDiceRoller([1, 1, 1, 1])))
            ->resolve($this->state([], [SkillName::FoulAppearance]), ActionType::BLITZ, ['playerId' => 1, 'targetId' => 2]);
        $this->assertFalse($r->isTurnover());
        $after = $r->getNewState();
        $this->assertSame(PlayerState::STANDING, $after->requirePlayer(2)->getState(), 'fixtura: rána propadla');
        $this->assertNotEmpty($this->actionsOf($after, 1, ActionType::MOVE), 'propadá rána, ne akce (ř. 8131-8132)');
        $this->assertSame(5, $after->requirePlayer(1)->getMovementRemaining(), 'pole za ránu je zaplacené');
        $this->assertEmpty($this->actionsOf($after, 1, ActionType::BLOCK), 'další rána už ne');
    }
}
