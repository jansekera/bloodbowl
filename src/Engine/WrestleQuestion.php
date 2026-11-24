<?php

declare(strict_types=1);

namespace App\Engine;

use App\DTO\GameState;
use App\DTO\PendingWrestleDTO;
use App\DTO\ReplayContext;

/** P312: Both Down, hráč kouče-člověka má Wrestle — použít? Chytá `ActionResolver::resolveSPrehranim`. */
final class WrestleQuestion extends CoachQuestion
{
    public function __construct(
        public readonly int $playerId,
        public readonly int $opponentId,
    ) {
        parent::__construct('Wrestle question pending');
    }

    public function pending(GameState $state, ReplayContext $replay): GameState
    {
        return $state->withPendingWrestle(new PendingWrestleDTO($this->playerId, $this->opponentId, $replay));
    }
}
