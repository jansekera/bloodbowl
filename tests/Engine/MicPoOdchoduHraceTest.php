<?php

declare(strict_types=1);

namespace App\Tests\Engine;

use App\Engine\ActionResolver;
use App\Engine\BallResolver;
use App\Engine\BigGuyCheckResolver;
use App\Engine\InjuryResolver;
use App\Engine\ScatterCalculator;
use App\Engine\TacklezoneCalculator;
use App\Enum\SkillName;
use App\Engine\FixedDiceRoller;
use App\Enum\ActionType;
use App\Enum\PlayerState;
use App\Enum\TeamSide;
use App\ValueObject\Position;
use PHPUnit\Framework\TestCase;

/**
 * P199 (14.10.2026), převod C++ P192 review (1): hráč je ze hřiště PRYČ dřív, než se jeho míč
 * odrazí — odraz přichází "after the player's armour and injury rolls are fully resolved"
 * (ř. 678-681). Vrátí-li se míč dvojím odrazem na pole odneseného nosiče (KO), zůstane ležet;
 * dřív se od ještě ležícího hráče odrazil znovu.
 */
final class MicPoOdchoduHraceTest extends TestCase
{
    public function testMicSeVratiNaPoleOdnesenehoNosiceAZustaneTam(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 7, id: 2)                 // nosič
            ->addPlayer(TeamSide::AWAY, 7, 6, id: 3)                 // pole za ním po stranách obsazená
            ->addPlayer(TeamSide::AWAY, 7, 8, id: 4)                 //   ⇒ odtlačení rovně na (7,7)
            ->addPronePlayer(TeamSide::AWAY, 8, 7, id: 5)            // ležící za ním
            ->withBallCarried(2)
            ->build();
        // Blok POW (6): odtlačen na (7,7) a sražen, bez follow-upu · brnění 6+6 prorazí · zranění
        // 4+5 = 9 KO · odraz D8 = 3 (V) na ležícího (8,7) · odraz D8 = 7 (Z) zpět na (7,7) =
        // prázdné pole po KO ⇒ míč leží. (Kdyby tam hráč 2 ještě byl, hodilo by se třetí D8.)
        $dice = new FixedDiceRoller([6, 6, 6, 4, 5, 3, 7]);
        $r = (new ActionResolver($dice))->resolve($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2, 'followUp' => false]);
        $po = $r->getNewState();
        $this->assertSame(PlayerState::KO, $po->requirePlayer(2)->getState());
        $this->assertNull($po->getBall()->getCarrierId());
        $this->assertEquals(new Position(7, 7), $po->getBall()->getPosition());
    }

    public function testVyloucenyNosic_MicSeOdraziAzPoJehoOdchodu(): void
    {
        // Nosič fauluje (ř. 1882-1884: dublet ⇒ vyloučen). Brnění 1+1 = dublet, neprorazí.
        // Odraz D8 = 3 (V) z (5,7) na faulovaného ležícího (6,7) · odraz D8 = 7 (Z) zpět na (5,7),
        // které je po vyloučení prázdné ⇒ míč leží. (Kdyby nosič na poli ještě stál, chytal by.)
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1)
            ->addPronePlayer(TeamSide::AWAY, 6, 7, id: 2)
            ->withBallCarried(1)
            ->build();
        $dice = new FixedDiceRoller([1, 1, 3, 7]);
        $r = (new ActionResolver($dice))->resolve($state, ActionType::FOUL, ['playerId' => 1, 'targetId' => 2]);
        $po = $r->getNewState();
        $this->assertNull($po->requirePlayer(1)->getPosition(), 'vyloučený je mimo hřiště');
        $this->assertNull($po->getBall()->getCarrierId());
        $this->assertEquals(new Position(5, 7), $po->getBall()->getPosition());
    }

    public function testUpirBezThralla_MicSeOdraziAzPoJehoOdchodu(): void
    {
        // Upír s míčem bez Thralla vedle sebe jde do rezerv (ř. 8027-8034). Odraz D8 = 3 (V) z (5,7)
        // na ležícího soupeře (6,7) · odraz D8 = 7 (Z) zpět na (5,7), už prázdné ⇒ míč leží.
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, skills: [SkillName::Bloodlust], id: 1)
            ->addPronePlayer(TeamSide::AWAY, 6, 7, id: 2)
            ->withBallCarried(1)
            ->build();
        $dice = new FixedDiceRoller([3, 7]);
        $resolver = new BigGuyCheckResolver(new InjuryResolver(), new BallResolver($dice, new TacklezoneCalculator(), new ScatterCalculator()));
        $r = $resolver->nakrmitUpira($state, $state->requirePlayer(1), $dice);
        $this->assertNull($r['state']->requirePlayer(1)->getPosition());
        $this->assertNull($r['state']->getBall()->getCarrierId());
        $this->assertEquals(new Position(5, 7), $r['state']->getBall()->getPosition());
    }
}
