<?php

declare(strict_types=1);

namespace App\Engine;

use App\DTO\PendingRerollDTO;
use App\DTO\GameState;
use App\DTO\ReplayContext;

/**
 * P292: nepovedený hod hráče kouče-člověka (přihrávka, bomba, hod spoluhráčem…) a kouč ještě neřekl, jestli přehodí
 * (viz `Coach::reroll`, `ReplayDiceRoller`). Chytá ji jen `ActionResolver::resolve`, který vrátí stav PŘED akcí
 * s `PendingRerollDTO` (replay).
 */
final class RerollQuestion extends CoachQuestion
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

    public function pending(GameState $state, ReplayContext $replay): GameState
    {
        $hrac = $state->getPlayer($this->playerId)?->getPosition();

        return $state->withPendingReroll(new PendingRerollDTO(
            $this->rollType,
            $this->playerId,
            $this->target,
            $this->roll,
            $this->pro,
            $this->team,
            $hrac?->getX() ?? 0,
            $hrac?->getY() ?? 0,
            $this->proFailed,
            replay: $replay,
        ));
    }
}
