<?php

declare(strict_types=1);

/**
 * ⭐ VOLBA KOSTEK PRO DIAGNOSTIKY (21.09.2026).
 *
 * ⛔ Do 21.09. platilo: argument `seed` v těchhle skriptech řídil jen los ras
 *   (`mt_rand`), protože `RandomDiceRoller` hází přes `random_int()`, na který
 *   `mt_srand()` nesahá. Dva běhy s týmž seedem tedy byly dvě různé trajektorie
 *   a **párové A/B v PHP vrstvě nešlo udělat**.
 *
 * Teď: když je nastavená proměnná prostředí `BB_DICE_SEED`, kostky hází
 * `SeededDiceRoller` a běh je opakovatelný.
 *
 *     BB_DICE_SEED=20260921 php cli/diag_event_histogram_20260911.php greedy 12 1 dev
 *
 * ⭐ Každá hra dostane vlastní odvozený seed (`seed`, `index hry`), takže
 *   **hra č. 5 má tytéž kostky v obou ramenech A/B** bez ohledu na to, jak
 *   dlouho trvaly hry před ní -- to je celý smysl společných náhodných čísel.
 *
 * ⚠️ Determinismus platí jen pro kouče, kteří sami nesahají po `random_int()`:
 *   `greedy` ano; `random` a `learning` používají `shuffle`/`array_rand`/`mt_rand`,
 *   tedy globální `mt_srand($seed)` -- ten skripty nastavují.
 */

require_once __DIR__ . '/../vendor/autoload.php';

use App\Engine\DiceRollerInterface;
use App\Engine\RandomDiceRoller;
use App\Engine\SeededDiceRoller;

/**
 * Kostky pro jednu hru. `$gameIndex` odliší hry mezi sebou.
 */
function bbDice(int $gameIndex = 0): DiceRollerInterface
{
    $seed = getenv('BB_DICE_SEED');
    if ($seed === false || $seed === '') {
        return new RandomDiceRoller();
    }

    return new SeededDiceRoller(((int) $seed) * 1_000_003 + $gameIndex);
}

/**
 * Řádek do výpisu, ať je v každém protokolu vidět, čím se házelo.
 */
function bbDicePopis(): string
{
    $seed = getenv('BB_DICE_SEED');

    return ($seed === false || $seed === '')
        ? 'kostky: random_int (NEOPAKOVATELNÉ -- pro párové A/B nastav BB_DICE_SEED)'
        : "kostky: SeededDiceRoller, BB_DICE_SEED={$seed} (opakovatelné)";
}
