<?php

declare(strict_types=1);

namespace App\Tests\Engine;

use App\DTO\GameState;
use App\Engine\ActionResolver;
use App\Engine\FixedDiceRoller;
use App\Engine\RulesEngine;
use App\Engine\TacklezoneCalculator;
use App\Enum\ActionType;
use App\Enum\TeamSide;
use App\ValueObject\Position;
use PHPUnit\Framework\Attributes\DataProvider;
use PHPUnit\Framework\TestCase;

/**
 * ⭐⭐ P170 — převod C++ testů `RiskWeightedPath.*` (engine/tests/test_pathfinder.cpp, commit 03fdb535).
 *
 * Uživatel 07.10.2026: „elfové by měli umět dodge na volné pole — což je 2 plus — zkontroluj, že
 * umějí volit cestu tak, aby uskočili do volného pole, i když cesta není tím přímo vpřed — a toto
 * by měli umět všichni — dodge je pro všechny agilní hráče“; „zkontroluj zároveň, že se takto nově
 * správně cesta počítá pro volbu i stejně provede pohyb“.
 *
 * Pravidla (úhyb): hází se při ODCHODU ze zóny, cíl 7 − AG, +1 za úhyb, −1 za každou zónu na
 * CÍLOVÉM poli ⇒ AG 4 do volného pole 2+, do pole v jedné zóně 3+; AG 3 do volného 3+, do zóny 4+.
 * Pozice: hráč (10,7), soupeř hned před ním (11,7), cíl (14,7). Přímo vpřed vede každý první krok
 * do soupeřovy zóny a z ní se uhýbá podruhé (AG 4: pád 44 %, AG 3: 67 %); ústup na sloupec x=9 je
 * úhyb do volného pole a zbytek cesty jde obloukem mimo zónu — 6 polí, jediný úhyb (17 % / 33 %).
 *
 * V PHP vybírá kouč cíl z `RulesEngine::getValidMoveTargets` (cesta + `successChance`) a pohyb
 * provede `MoveHandler` přes `Pathfinder::findPathTo` — obojí čte totéž hledání.
 */
final class RiskWeightedPathTest extends TestCase
{
    private const CIL_X = 14;
    private const CIL_Y = 7;

    private function stav(int $agility): GameState
    {
        return (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 10, 7, movement: 7, agility: $agility, id: 1)
            ->addPlayer(TeamSide::AWAY, 11, 7, agility: 3, id: 12)
            ->build();
    }

    /** @return array<string, array{int, int}> agility, šance cesty v % (jediný úhyb do volného pole) */
    public static function agility(): array
    {
        return ['AG 4' => [4, 83], 'AG 3' => [3, 67]];
    }

    /** @return array{x: int, y: int, dodges: int, gfis: int, successChance: int, path: list<array{x: int, y: int, dodge: bool, gfi: bool, dodgeTarget: int}>} */
    private function nabidnutaCesta(GameState $s): array
    {
        foreach ((new RulesEngine())->getValidMoveTargets($s, 1) as $t) {
            if ($t['x'] === self::CIL_X && $t['y'] === self::CIL_Y) {
                return $t;
            }
        }
        self::fail('cíl (14,7) není v nabídce pohybu');
    }

    #[DataProvider('agility')]
    public function testDodgesIntoAFreeSquareAndGoesAroundEvenWhenThatIsNotStraightAhead(int $agility, int $sance): void
    {
        $s = $this->stav($agility);
        $cesta = $this->nabidnutaCesta($s);

        $this->assertSame($sance, $cesta['successChance'], "AG {$agility}: jediný hod cesty je úhyb do volného pole");
        $this->assertSame(1, $cesta['dodges'], "AG {$agility}");

        $prvni = new Position($cesta['path'][0]['x'], $cesta['path'][0]['y']);
        $this->assertSame(
            0,
            (new TacklezoneCalculator())->countTacklezones($s, $prvni, TeamSide::HOME),
            "AG {$agility}: první krok {$prvni} vede do pole bez soupeřovy zóny, ne přímo vpřed",
        );
    }

    #[DataProvider('agility')]
    public function testTheWalkFollowsTheSameRouteThatWasPriced(int $agility, int $sance): void
    {
        $s = $this->stav($agility);
        $cesta = $this->nabidnutaCesta($s);
        $ocenena = array_map(static fn(array $k): string => "({$k['x']},{$k['y']})", $cesta['path']);

        // Kostky 6: každý hod vyjde; počet hodů = počet úhybů prošlé cesty.
        $r = (new ActionResolver(new FixedDiceRoller(array_fill(0, 10, 6))))
            ->resolve($s, ActionType::MOVE, ['playerId' => 1, 'x' => self::CIL_X, 'y' => self::CIL_Y]);
        $this->assertTrue($r->isSuccess(), "AG {$agility}");

        $prosla = [];
        $uhyby = [];
        foreach ($r->getEvents() as $e) {
            if ($e->getType() === 'player_move') {
                $prosla[] = $e->getData()['to'];
            }
            if ($e->getType() === 'dodge') {
                $uhyby[] = $e->getData()['target'];
            }
        }

        $this->assertSame($ocenena, $prosla, "AG {$agility}: prošlá cesta = oceněná cesta");
        $this->assertCount(6, $prosla, "AG {$agility}: oblouk o dvě pole delší než přímá cesta");
        $this->assertSame([6 - $agility], $uhyby, "AG {$agility}: z kontaktu jediný úhyb, a to do volného pole (7 − AG − 1)");
        $this->assertSame($sance, (int) round((7 - $uhyby[0]) / 6 * 100), "AG {$agility}: oceněná šance = šance prošlé cesty");
    }
}
