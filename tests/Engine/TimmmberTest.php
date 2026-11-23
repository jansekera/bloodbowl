<?php

declare(strict_types=1);

namespace App\Tests\Engine;

use App\Engine\ActionResolver;
use App\Engine\FixedDiceRoller;
use App\Engine\Pathfinder;
use App\Enum\ActionType;
use App\Enum\PlayerState;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use PHPUnit\Framework\TestCase;

/**
 * P321 — Timmm-ber! (rules_bb2016.txt ř. 8625-8636): při vstávání (pod 3 MA hod 4+, ř. 690-692) přidá každý
 * sousední spoluhráč, který stojí a není v soupeřově tackle zóně, +1; „a 1 is always a failure“.
 */
final class TimmmberTest extends TestCase
{
    /** @param list<int> $kostky */
    private function vstan(GameStateBuilder $b, array $kostky): PlayerState
    {
        $state = $b->withBallOffPitch()->build();
        $result = (new ActionResolver(new FixedDiceRoller($kostky)))
            ->resolve($state, ActionType::MOVE, ['playerId' => 1, 'x' => 10, 'y' => 7]);

        return $result->getNewState()->requirePlayer(1)->getState();
    }

    private function strom(bool $timmmber = true): GameStateBuilder
    {
        return (new GameStateBuilder())
            ->addPronePlayer(TeamSide::HOME, 10, 7, movement: 2, skills: $timmmber ? [SkillName::Timmmber] : [], id: 1)
            ->addPlayer(TeamSide::HOME, 11, 7, id: 2)
            ->addPlayer(TeamSide::HOME, 9, 7, id: 3);
    }

    public function testDvaVolniPomocniciSnizujiCilNaDva(): void
    {
        $this->assertSame(PlayerState::STANDING, $this->vstan($this->strom(), [2]));
    }

    public function testJednickaSelzeVzdy(): void
    {
        // 1, týmový přehoz 1 ⇒ leží dál i se dvěma pomocníky
        $this->assertSame(PlayerState::PRONE, $this->vstan($this->strom(), [1, 1]));
    }

    public function testBezSchopnostiPomocniciNepomahaji(): void
    {
        // pozitivní kontrola: tíž sousedé, bez Timmm-ber! je cíl 4+ ⇒ 2 a přehoz 3 nestačí
        $this->assertSame(PlayerState::PRONE, $this->vstan($this->strom(false), [2, 3]));
    }

    public function testPomocnikVSouperoveZoneSeNepocita(): void
    {
        // soupeř u pomocníka č. 2 ⇒ pomáhá jen č. 3 ⇒ cíl 3+
        $b = $this->strom()->addPlayer(TeamSide::AWAY, 12, 7, id: 4);
        $this->assertSame(PlayerState::PRONE, $this->vstan($b, [2, 2]));
        $b = $this->strom()->addPlayer(TeamSide::AWAY, 12, 7, id: 4);
        $this->assertSame(PlayerState::STANDING, $this->vstan($b, [3]));
    }

    public function testLeziciSpoluhracNepomaha(): void
    {
        $b = (new GameStateBuilder())
            ->addPronePlayer(TeamSide::HOME, 10, 7, movement: 2, skills: [SkillName::Timmmber], id: 1)
            ->addPronePlayer(TeamSide::HOME, 11, 7, id: 2);
        $this->assertSame(PlayerState::PRONE, $this->vstan($b, [3, 3]));
    }

    public function testOdhadSanceZnaPomocniky(): void
    {
        $state = $this->strom()->withBallOffPitch()->build();
        $strom = $state->requirePlayer(1);
        $this->assertEqualsWithDelta(5 / 6, Pathfinder::sanceVstani($strom, $state), 1e-9);
        $this->assertEqualsWithDelta(3 / 6, Pathfinder::sanceVstani($strom), 1e-9);
    }
}
