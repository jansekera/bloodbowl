<?php

declare(strict_types=1);

namespace App\Tests\Engine;

use App\Engine\RulesEngine;
use App\Enum\ActionType;
use App\Enum\TeamSide;
use App\Enum\Weather;
use PHPUnit\Framework\TestCase;

/**
 * ⭐ VANICE: JEN QUICK A SHORT PRIHRAVKY (15.09.2026).
 *
 * `rules_bb2016.txt` r. 1490-1494: "the snow means that only quick or short
 * passes can be attempted." ⛔ Do 15.09. engine vanici u prihravky jen
 * penalizoval (-1, coz byla vada, opraveno v 8cf8e316) a dlouhe hazet nechal.
 */
final class BlizzardPassRangeTest extends TestCase
{
    private function stav(Weather $pocasi)
    {
        return (new GameStateBuilder())
            ->withWeather($pocasi)
            ->addPlayer(TeamSide::HOME, 5, 5, id: 1)
            ->addPlayer(TeamSide::HOME, 9, 5, id: 2)
            ->addPlayer(TeamSide::HOME, 14, 5, id: 3)
            ->withBallCarried(1)
            ->build();
    }

    /** @return list<string> */
    private function dosahy(Weather $pocasi): array
    {
        $s = $this->stav($pocasi);
        $r = array_unique(array_column((new RulesEngine())->getPassTargets($s, $s->getPlayer(1)), 'range'));
        sort($r);
        return array_values($r);
    }

    public function testVeVaniciNabizeJenQuickAShort(): void
    {
        $this->assertSame(['quick_pass', 'short_pass'], $this->dosahy(Weather::BLIZZARD));
    }

    public function testPozitivniKontrolaZaHezkehoPoasiNabiziVsechnyCtyri(): void
    {
        $this->assertCount(4, $this->dosahy(Weather::NICE));
    }

    public function testVeVaniciJeDlouhaPrihravkaNeplatna(): void
    {
        $chyby = (new RulesEngine())->validate($this->stav(Weather::BLIZZARD), ActionType::PASS, [
            'playerId' => 1, 'targetX' => 14, 'targetY' => 5,   // 9 poli = long pass
        ]);
        $this->assertNotSame([], $chyby);
    }

    public function testVeVaniciJeKratkaPrihravkaPlatna(): void
    {
        $chyby = (new RulesEngine())->validate($this->stav(Weather::BLIZZARD), ActionType::PASS, [
            'playerId' => 1, 'targetX' => 9, 'targetY' => 5,    // 4 pole = short pass
        ]);
        $this->assertSame([], $chyby);
    }
}
