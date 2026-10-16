<?php

declare(strict_types=1);

namespace App\Tests\Engine;

use App\DTO\BallState;
use App\DTO\GameState;
use App\Engine\ActionResolver;
use App\Engine\FixedDiceRoller;
use App\Enum\ActionType;
use App\Enum\PlayerState;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use App\ValueObject\Position;
use PHPUnit\Framework\TestCase;

/**
 * Audit parity 08.10. bod (a), ROZHODL UŽIVATEL 16.10.2026: „pokud nosič block nemá a obránce má — wrestle
 * nevolí — pokud má nosič block — obránce wrestle volí“; „pokud má obránce wrestle a útočník block — tak
 * obránce použije“. `rules_bb2016.txt` ř. 8670-8678. Táž podmínka v C++ (`block_handler.cpp`).
 */
final class WrestleDefenderChoiceTest extends TestCase
{
    /** @param list<SkillName> $attackerSkills */
    private function state(array $attackerSkills, bool $attackerCarries): GameState
    {
        $s = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, skills: $attackerSkills, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 7, skills: [SkillName::Block, SkillName::Wrestle], id: 2)
            ->withBallOffPitch()
            ->build();
        return $attackerCarries ? $s->withBall(BallState::carried(new Position(5, 7), 1)) : $s;
    }

    public function testAgainstACarrierWithoutBlockTheDefenderKeepsBlockAndStands(): void
    {
        // 2 = Both Down; útočník bez Blocku padá (brnění 1+1), míč odskočí (4 …)
        $r = (new ActionResolver(new FixedDiceRoller([2, 1, 1, 4, 4, 4, 4, 4, 4])))
            ->resolve($this->state([], true), ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);
        $this->assertSame(PlayerState::STANDING, $r->getNewState()->requirePlayer(2)->getState(), 'Wrestle nevolí');
        $this->assertSame(PlayerState::PRONE, $r->getNewState()->requirePlayer(1)->getState());
    }

    public function testAgainstAnAttackerWithBlockTheDefenderWrestles(): void
    {
        // útočník s Blockem, míč nenese: bez Wrestle by se nestalo nic ⇒ obránce Wrestle použije
        $r = (new ActionResolver(new FixedDiceRoller([2])))
            ->resolve($this->state([SkillName::Block], false), ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);
        $this->assertSame(PlayerState::PRONE, $r->getNewState()->requirePlayer(1)->getState(), 'Wrestle položí útočníka');
        $this->assertSame(PlayerState::PRONE, $r->getNewState()->requirePlayer(2)->getState());
        $this->assertFalse($r->isTurnover(), 'míč útočník nedržel (ř. 8677-8678)');
    }
}
