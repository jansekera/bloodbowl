<?php

declare(strict_types=1);

namespace App\Engine;

/**
 * P292: nepovedený hod hráče kouče-člověka (přihrávka, bomba, hod spoluhráčem…) a kouč ještě neřekl, jestli přehodí
 * (viz `CoachRerolls`, `ReplayDiceRoller`). Chytá ji jen `ActionResolver::resolve`, který vrátí stav PŘED akcí
 * s `PendingRerollDTO` (replay).
 */
final class RerollQuestion extends \RuntimeException
{
    public function __construct(
        public readonly int $playerId,
        public readonly string $rollType,
        public readonly int $roll,
        public readonly int $target,
        public readonly bool $pro,
        public readonly bool $team,
        public readonly bool $proFailed,
    ) {
        parent::__construct('Reroll question pending');
    }
}
