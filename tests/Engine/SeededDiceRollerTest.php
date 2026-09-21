<?php

declare(strict_types=1);

namespace App\Tests\Engine;

use App\Engine\DiceRollerInterface;
use App\Engine\RandomDiceRoller;
use App\Engine\SeededDiceRoller;
use PHPUnit\Framework\TestCase;

/**
 * ⭐ Opakovatelnost kostek (21.09.2026) -- viz `SeededDiceRoller`.
 *
 * Testy jsou psané tak, aby **spadly i na starém chování**: kdyby roller
 * uvnitř sáhl po `random_int()` nebo po globálním `mt_srand`, první test
 * padne hned.
 */
final class SeededDiceRollerTest extends TestCase
{
    /** @return list<int> */
    private function serie(DiceRollerInterface $d, int $n = 60): array
    {
        $out = [];
        for ($i = 0; $i < $n; $i++) { $out[] = $d->rollD6(); }

        return $out;
    }

    public function testTyzSeedDaTutezPosloupnost(): void
    {
        $a = $this->serie(new SeededDiceRoller(12345));
        $b = $this->serie(new SeededDiceRoller(12345));

        $this->assertSame($a, $b, 'týž seed musí dát týž běh -- na tom stojí párové A/B');
    }

    public function testJinySeedDaJinouPosloupnost(): void
    {
        // Pozitivní kontrola k testu výše: kdyby roller vracel pořád totéž
        // (třeba samé 1), první test by prošel a neznamenal by nic.
        $a = $this->serie(new SeededDiceRoller(12345));
        $b = $this->serie(new SeededDiceRoller(12346));

        $this->assertNotSame($a, $b, 'jiný seed musí dát jiný běh');
    }

    public function testGlobalniMtSrandNaVysledekNemaVliv(): void
    {
        mt_srand(1);
        $a = $this->serie(new SeededDiceRoller(999));
        mt_srand(2);
        $b = $this->serie(new SeededDiceRoller(999));

        $this->assertSame($a, $b, 'roller nesmí viset na globálním stavu');
    }

    public function testDvaRollerySeNavzajemNeruzi(): void
    {
        $samostatne = $this->serie(new SeededDiceRoller(7), 20);

        $prvni = new SeededDiceRoller(7);
        $druhy = new SeededDiceRoller(8);
        $prolozene = [];
        for ($i = 0; $i < 20; $i++) {
            $prolozene[] = $prvni->rollD6();
            $druhy->rollD6(); // druhý roller mezi tím taky hází
        }

        $this->assertSame($samostatne, $prolozene, 'rollery nesmí sdílet stav');
    }

    public function testRozsahyHodu(): void
    {
        $d = new SeededDiceRoller(20260921);
        $d6 = []; $d8 = []; $dd = [];
        for ($i = 0; $i < 3000; $i++) {
            $d6[] = $d->rollD6();
            $d8[] = $d->rollD8();
            $dd[] = $d->roll2D6();
        }

        $this->assertSame([1, 2, 3, 4, 5, 6], $this->hodnoty($d6), 'D6 padne 1..6 a nic jiného');
        $this->assertSame([1, 2, 3, 4, 5, 6, 7, 8], $this->hodnoty($d8), 'D8 padne 1..8');
        $this->assertSame(2, min($dd));
        $this->assertSame(12, max($dd));
    }

    public function testDvojkostkaJeSouctemDvouHodu(): void
    {
        $a = new SeededDiceRoller(555);
        $b = new SeededDiceRoller(555);

        $this->assertSame($b->rollD6() + $b->rollD6(), $a->roll2D6(), '2D6 spotřebuje právě dva hody');
    }

    public function testJeZaminitelnyZaOstryRoller(): void
    {
        $this->assertInstanceOf(DiceRollerInterface::class, new SeededDiceRoller(1));
        $this->assertInstanceOf(DiceRollerInterface::class, new RandomDiceRoller());
    }

    /**
     * @param list<int> $hody
     * @return list<int>
     */
    private function hodnoty(array $hody): array
    {
        $u = array_values(array_unique($hody));
        sort($u);

        return $u;
    }
}
