<?php

declare(strict_types=1);

namespace App\Engine;

use App\DTO\GameState;
use App\DTO\PendingJuggernautDTO;
use App\DTO\ReplayContext;

/**
 * P309: blok narazil na otázku Juggernautu pro kouče-člověka (Both Down v blitzu brát jako Pushed?) a odpověď ještě
 * nemá. Chytá ji `ActionResolver::resolveSPrehranim` (jako `DivingCatchQuestion`).
 */
final class JuggernautQuestion extends CoachQuestion
{
    public function __construct(
        public readonly int $playerId,
        public readonly int $defenderId,
    ) {
        parent::__construct('Juggernaut question pending');
    }

    public function pending(GameState $state, ReplayContext $replay): GameState
    {
        return $state->withPendingJuggernaut(new PendingJuggernautDTO($this->playerId, $this->defenderId, $replay));
    }
}
