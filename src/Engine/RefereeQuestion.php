<?php

declare(strict_types=1);

namespace App\Engine;

/**
 * P291: rozhodčí uprostřed faulu vylučuje hráče kouče-člověka a kouč ještě neřekl, jestli použije úplatek / protest
 * (viz `ReplayDiceRoller`). Chytá ji jen `ActionResolver::resolve`, který vrátí stav PŘED akcí s `PendingRefereeDTO`.
 */
final class RefereeQuestion extends \RuntimeException
{
    public function __construct(
        public readonly int $playerId,
        public readonly bool $canBribe,
        public readonly bool $canArgue,
    ) {
        parent::__construct('Referee question pending');
    }
}
