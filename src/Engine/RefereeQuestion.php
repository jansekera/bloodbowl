<?php

declare(strict_types=1);

namespace App\Engine;

use App\DTO\PendingRefereeDTO;
use App\DTO\GameState;
use App\DTO\ReplayContext;

/**
 * P291: rozhodčí uprostřed faulu vylučuje hráče kouče-člověka a kouč ještě neřekl, jestli použije úplatek / protest
 * (viz `ReplayDiceRoller`). Chytá ji jen `ActionResolver::resolve`, který vrátí stav v okamžiku otázky (rozpracovaná akce, jen k zobrazení; P294 krok 5a) s `PendingRefereeDTO`.
 */
final class RefereeQuestion extends CoachQuestion
{
    public function __construct(
        public readonly int $playerId,
        public readonly bool $canBribe,
        public readonly bool $canArgue,
    ) {
        parent::__construct('Referee question pending');
    }

    public function pending(GameState $state, ReplayContext $replay): GameState
    {
        return $state->withPendingReferee(new PendingRefereeDTO(
            $this->playerId,
            true,
            $this->canBribe,
            $this->canArgue,
            $replay,
        ));
    }
}
