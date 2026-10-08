<?php

declare(strict_types=1);

namespace Tests\Enum;

use App\Engine\RulesEngine;
use App\Enum\ActionType;
use App\Enum\PassRange;
use App\Enum\TeamSide;
use App\Tests\Engine\GameStateBuilder;
use PHPUnit\Framework\TestCase;

/**
 * DOSAH PŘIHRÁVKY SE MĚŘÍ PRAVÍTKEM (audit parity 08.10.2026, nález 12).
 *
 * `rules_bb2016.txt` ř. 765: "Next, the coach must measure the range using the **range
 * ruler**." ř. 774-776: "attempting to throw the ball four squares ... The range ruler shows
 * that this falls just on the boundary between a Quick and a Short Pass, so the longer of
 * the two ranges must be used."
 * Pravítko je tvarovaná šablona, ne poloměr; v textu pravidel je jen jako obrázek. Jeho
 * mřížka pásem pro posun (|dx|, |dy|) je přepsaná z tištěné tabulky v
 * `evidence/pass_range_grid_20260810.txt` (ověřeno uživatelem 10.08.2026, C++ ji má
 * v `engine/include/bb/enums.h`). Tenhle test čte mřížku z toho souboru -- nezávisle na
 * konstantě v `PassRange`.
 *
 * Stará mechanika: pásma podle Čebyševovy vzdálenosti max(|dx|,|dy|) s prahy 3/6/10/13 --
 * šikmé přihrávky byly lehčí a delší, než pravítko dovolí ((3,3) Quick místo Short,
 * (6,6) Short místo Long, (10,10) i (13,13) šly hodit).
 */
final class PassRangeTest extends TestCase
{
    private static function pasmo(int $dx, int $dy): ?PassRange
    {
        return PassRange::fromOffset($dx, $dy);
    }

    /**
     * Mřížka z evidence: [dy][dx] => 'Q' | 'S' | 'L' | 'B' | '.' ('T' = pole házejícího).
     *
     * @return array<int, list<string>>
     */
    private static function mrizkaZEvidence(): array
    {
        $radky = file(__DIR__ . '/../../evidence/pass_range_grid_20260810.txt');
        self::assertIsArray($radky);
        $mrizka = [];
        foreach ($radky as $radek) {
            if (preg_match('/^\s*(\d+) \|(.*)$/', $radek, $m) === 1) {
                $mrizka[(int) $m[1]] = preg_split('/\s+/', trim($m[2])) ?: [];
            }
        }

        return $mrizka;
    }

    public function testCelaMrizkaPravitkaSediSTistenouTabulkou(): void
    {
        $mrizka = self::mrizkaZEvidence();
        $this->assertCount(14, $mrizka, 'evidence: 14 řad (dy 0-13)');

        $ocekavane = ['Q' => PassRange::QUICK_PASS, 'S' => PassRange::SHORT_PASS, 'L' => PassRange::LONG_PASS, 'B' => PassRange::LONG_BOMB, '.' => null];
        $overeno = 0;
        for ($dy = 0; $dy <= 13; $dy++) {
            $this->assertCount(14, $mrizka[$dy], "evidence: řada dy={$dy} má 14 polí");
            for ($dx = 0; $dx <= 13; $dx++) {
                if ($dx === 0 && $dy === 0) {
                    continue; // pole házejícího
                }
                $this->assertSame($ocekavane[$mrizka[$dy][$dx]], self::pasmo($dx, $dy), "posun ({$dx},{$dy})");
                $overeno++;
            }
        }
        $this->assertSame(195, $overeno);
    }

    public function testPravitkoJeSoumerneVeVsechSmerech(): void
    {
        foreach ([[2, 3], [5, 9], [10, 3], [12, 4], [13, 2]] as [$dx, $dy]) {
            $pasmo = self::pasmo($dx, $dy);
            $this->assertSame($pasmo, self::pasmo($dy, $dx), "({$dx},{$dy}) × ({$dy},{$dx})");
            $this->assertSame($pasmo, self::pasmo(-$dx, $dy));
            $this->assertSame($pasmo, self::pasmo($dx, -$dy));
            $this->assertSame($pasmo, self::pasmo(-$dx, -$dy));
        }
    }

    public function testCtyriPoleRovneJeShortPodlePrikladuVPravidlech(): void
    {
        // ř. 774-776: čtyři pole = hranice Quick/Short ⇒ platí delší, Short.
        $this->assertSame(PassRange::QUICK_PASS, self::pasmo(3, 0));
        $this->assertSame(PassRange::SHORT_PASS, self::pasmo(4, 0));
    }

    public function testSikmePrihravkyJsouDelsiNezPodleCebyseva(): void
    {
        $this->assertSame(PassRange::SHORT_PASS, self::pasmo(3, 3), 'Čebyšev 3 dával Quick');
        $this->assertSame(PassRange::LONG_PASS, self::pasmo(6, 6), 'Čebyšev 6 dával Short');
        $this->assertSame(PassRange::LONG_BOMB, self::pasmo(10, 3), 'Čebyšev 10 dával Long');
        $this->assertNull(self::pasmo(10, 10), 'Čebyšev 10 dával Long');
        $this->assertNull(self::pasmo(13, 13), 'Čebyšev 13 dával Long Bomb');
        $this->assertNull(self::pasmo(5, 12), 'stejně daleko jako (13,0), a pravítko nedosáhne');
        $this->assertSame(PassRange::LONG_BOMB, self::pasmo(13, 0));
        $this->assertNull(self::pasmo(14, 0));
    }

    public function testPravidlaNedovoliPrihravkuKamPravitkoNedosahne(): void
    {
        // Totéž přes validaci akce: nosič (5,2), cíl (15,12) = posun (10,10).
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 2, id: 1)
            ->withBallCarried(1)
            ->build();
        $rules = new RulesEngine();

        $this->assertNotSame([], $rules->validate($state, ActionType::PASS, ['playerId' => 1, 'targetX' => 15, 'targetY' => 12]));
        $this->assertSame([], $rules->validate($state, ActionType::PASS, ['playerId' => 1, 'targetX' => 15, 'targetY' => 2]), 'kontrola: (10,0) je Long Pass');
    }

    public function testNabidkaCiluNesePasmoPodlePravitka(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 2, id: 1)
            ->withBallCarried(1)
            ->build();

        $pasma = [];
        foreach ((new RulesEngine())->getPassTargets($state, $state->requirePlayer(1)) as $cil) {
            $pasma[$cil['x'] . ',' . $cil['y']] = $cil['range'];
        }

        $this->assertSame('short_pass', $pasma['8,5'] ?? null, 'posun (3,3)');
        $this->assertSame('long_pass', $pasma['11,8'] ?? null, 'posun (6,6)');
        $this->assertArrayNotHasKey('15,12', $pasma, 'posun (10,10) pravítko nedosáhne');
    }

    public function testModifiers(): void
    {
        $this->assertEquals(1, PassRange::QUICK_PASS->modifier());
        $this->assertEquals(0, PassRange::SHORT_PASS->modifier());
        $this->assertEquals(-1, PassRange::LONG_PASS->modifier());
        $this->assertEquals(-2, PassRange::LONG_BOMB->modifier());
    }
}
