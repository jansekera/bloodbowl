<?php

declare(strict_types=1);

namespace App\Engine;

use Random\Engine\PcgOneseq128XslRr64;
use Random\Randomizer;

/**
 * ⭐ OPAKOVATELNÉ KOSTKY (21.09.2026).
 *
 * `RandomDiceRoller` hází přes `random_int()`, což je CSPRNG a **`mt_srand()` ho
 * neovlivní**. Argument `seed` v diagnostických skriptech proto do 21.09. řídil
 * jen los ras (`mt_rand`), NE kostky -- dva běhy téhož skriptu s týmž seedem
 * byly dvě různé trajektorie (12 her dalo 366 / 370 / 373 / 381 kol).
 * Tím pádem se nedalo dělat **párové A/B** v PHP vrstvě tak, jak se dělá
 * v C++ (common random numbers).
 *
 * Tenhle roller je deterministický: **týž seed = táž posloupnost hodů**.
 * Nepoužívá globální stav (`mt_srand`), takže víc rollerů vedle sebe se neruší.
 *
 * ⛔ Není kryptografický a nemá být. Do hry hrané člověkem patří
 * `RandomDiceRoller`; tenhle je pro měření.
 */
final class SeededDiceRoller implements DiceRollerInterface
{
    private readonly Randomizer $rng;

    public function __construct(private readonly int $seed)
    {
        $this->rng = new Randomizer(new PcgOneseq128XslRr64($seed));
    }

    public function getSeed(): int
    {
        return $this->seed;
    }

    public function rollD6(): int
    {
        return $this->rng->getInt(1, 6);
    }

    public function roll2D6(): int
    {
        return $this->rollD6() + $this->rollD6();
    }

    public function rollD8(): int
    {
        return $this->rng->getInt(1, 8);
    }
}
