<?php

declare(strict_types=1);

namespace App\Tests\Engine;

use App\DTO\GameState;
use App\Engine\ActionResolver;
use App\Engine\FixedDiceRoller;
use App\Enum\ActionType;
use App\Enum\PlayerState;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use PHPUnit\Framework\TestCase;

/**
 * Audit parity (kniha „ocenění Both Down PHP 90 × C++ 8“, 24.11.2026): výběr kostky AI v PHP podle pořadí C++
 * `scoreFace`. Kostky: 1 lebka · 2 Both Down · 3-4 Pushed · 5 Stumbles · 6 Defender Down. Útočník HOME (5,5) ST4
 * proti ST3 ⇒ dvě kostky, volí útočník.
 */
final class BothDownOceneniParitaTest extends TestCase
{
    /**
     * @param list<SkillName> $utocnik
     * @param list<SkillName> $obrance
     */
    private function stav(array $utocnik, array $obrance): GameState
    {
        return (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 4, skills: $utocnik, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, skills: $obrance, id: 2)
            ->withBallOffPitch()
            ->build();
    }

    /** @param list<int> $kostky */
    private function blok(GameState $s, array $kostky): GameState
    {
        return (new ActionResolver(new FixedDiceRoller($kostky)))->resolve($s, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2])->getNewState();
    }

    /** Block proti obránci bez Blocku: Stumbles (odtlačen A sražen) před Both Down (sražen na místě) — C++ 9 > 8. */
    public function testStumblesPredBothDownSBlockem(): void
    {
        $s = $this->blok($this->stav([SkillName::Block], []), [2, 5, 1, 1, 1, 1]);

        $this->assertSame(PlayerState::PRONE, $s->requirePlayer(2)->getState());
        $this->assertSame(7, $s->requirePlayer(2)->requirePosition()->getX(), 'Stumbles: odtlačen, pak sražen (dřív Both Down na místě)');
    }

    /** Block + Wrestle proti obránci s Blockem: Both Down (Wrestle složí obránce) před Pushed — C++ 7 > 5. */
    public function testWrestleProtiBlockuBothDownPredPushed(): void
    {
        $s = $this->blok($this->stav([SkillName::Block, SkillName::Wrestle], [SkillName::Block]), [2, 3, 1, 1]);

        $this->assertSame(PlayerState::PRONE, $s->requirePlayer(2)->getState(), 'dřív Pushed (20 < 30)');
        $this->assertSame(6, $s->requirePlayer(2)->requirePosition()->getX());
    }

    /** Pro: Both Down útočníka bez Blocku je špatná kostka i s Wrestle (C++ `shouldRerollBlock`) ⇒ zkusí Pro. */
    public function testProPrehodiBothDownBezBlockuIsWrestle(): void
    {
        $s = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, skills: [SkillName::Wrestle, SkillName::Pro], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, id: 2)
            ->withBallOffPitch()
            ->build();
        $po = $this->blok($s, [2, 4, 6, 1, 1]);   // Both Down; Pro 4+ ⇒ přehoz 6 = Defender Down; brnění

        $this->assertTrue($po->requirePlayer(1)->isProUsedThisTurn(), 'Pro se nezkusil (dřív Both Down s Wrestle = 25 ≥ 0)');
        $this->assertSame(PlayerState::STANDING, $po->requirePlayer(1)->getState());
    }
}
