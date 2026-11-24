<?php

declare(strict_types=1);

namespace App\Engine;

use App\DTO\GameState;
use App\DTO\PendingPilingOnDTO;
use App\DTO\ReplayContext;

/**
 * P310: blok narazil na otázku Piling On pro kouče-člověka (za týmový přehoz přehodit brnění / zranění a lehnout si?)
 * a odpověď ještě nemá. Chytá ji `ActionResolver::resolveSPrehranim` (jako `JuggernautQuestion`).
 */
final class PilingOnQuestion extends CoachQuestion
{
    public function __construct(
        public readonly int $playerId,
        public readonly int $defenderId,
        public readonly bool $armour,
    ) {
        parent::__construct('Piling On question pending');
    }

    public function pending(GameState $state, ReplayContext $replay): GameState
    {
        return $state->withPendingPilingOn(new PendingPilingOnDTO($this->playerId, $this->defenderId, $this->armour, $replay));
    }
}
