<?php

declare(strict_types=1);

namespace App\Tests\Engine;

use App\DTO\ActionResult;
use App\Engine\ActionResolver;
use App\Engine\FixedDiceRoller;
use App\Enum\ActionType;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use App\ValueObject\Position;
use PHPUnit\Framework\TestCase;

/**
 * P198 (14.10.2026), převod C++ P194 (6) + review P208: follow-up se rozhoduje PŘED jakýmkoli
 * dalším hodem (ř. 609-610 "must decide whether to follow up before any other dice rolls are
 * made"). Odraz vyraženého míče (Strip Ball) i vhazování a zranění v davu přicházejí AŽ PO
 * follow-upu; míč, který pak dopadne na útočníkovo nové pole, útočník chytá.
 * Fend (ř. 8113-8118): "may not follow-up ... even if the Fend player is Knocked Down".
 */
final class FollowUpPredHodyTest extends TestCase
{
    /**
     * @param list<int> $kostky
     * @param list<SkillName> $obrance
     */
    private function blok(array $kostky, array $obrance = [], bool $nosic = true, int $ax = 5, int $dx = 6): ActionResult
    {
        $b = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, $ax, 7, skills: [SkillName::StripBall], id: 1)
            ->addPlayer(TeamSide::AWAY, $dx, 7, skills: $obrance, id: 2);
        if ($dx < 25) {   // pole za obráncem po stranách obsazená ⇒ odtlačení rovně
            $b = $b->addPlayer(TeamSide::AWAY, $dx + 1, 6, id: 3)->addPlayer(TeamSide::AWAY, $dx + 1, 8, id: 4);
        }
        if ($nosic) {
            $b = $b->withBallCarried(2);
        }

        return (new ActionResolver(new FixedDiceRoller($kostky)))
            ->resolve($b->build(), ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2, 'followUp' => true]);
    }

    public function testStripBall_OdrazPoFollowUpu_UtocnikChyta(): void
    {
        // Pushed (3) · odtlačen na (7,7), míč vyražen · útočník následuje na (6,7) · odraz D8 = 7 (Z)
        // z (7,7) na (6,7) = na útočníka ⇒ chytá 6. Dřív se odráželo před follow-upem: míč ležel na
        // prázdném (6,7) a útočník na něj jen vstoupil.
        $r = $this->blok([3, 7, 6]);
        $this->assertSame(1, $r->getNewState()->getBall()->getCarrierId());
    }

    public function testFendZakazujeFollowUpISrazenemu(): void
    {
        // POW (6): obránce s Fend sražen a odtlačen; útočník následovat nesmí; brnění 1+1
        $r = $this->blok([6, 1, 1], [SkillName::Fend], nosic: false);
        $this->assertEquals(new Position(5, 7), $r->getNewState()->requirePlayer(1)->getPosition());
    }

    public function testDav_VhazovaniAzPoFollowUpu(): void
    {
        // Nosič u pravé lajny vytlačen do davu, útočník následuje na (25,7). Vhazování: šablona 3
        // (z pravého okraje na západ), 2D6 = 1+1 ⇒ jedno pole = (24,7) -- útočníkovo PŮVODNÍ pole,
        // po follow-upu prázdné ⇒ odraz D8 = 5 (J) na (24,8). Zranění v davu 1+1.
        // Dřív se vhazovalo před follow-upem: na (24,7) ještě stál útočník a míč chytal.
        $r = $this->blok([3, 3, 1, 1, 5, 1, 1], nosic: true, ax: 24, dx: 25);
        $po = $r->getNewState();
        $this->assertEquals(new Position(25, 7), $po->requirePlayer(1)->getPosition());
        $this->assertNull($po->getBall()->getCarrierId());
        $this->assertEquals(new Position(24, 8), $po->getBall()->getPosition());
    }
}
