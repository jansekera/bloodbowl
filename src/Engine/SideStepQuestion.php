<?php

declare(strict_types=1);

namespace App\Engine;

use App\DTO\GameState;
use App\DTO\PendingSideStepDTO;
use App\DTO\ReplayContext;
use App\ValueObject\Position;

/** P312: odtlačený hráč kouče-člověka má Side Step — kam (nebo nepoužít)? Chytá `ActionResolver::resolveSPrehranim`. */
final class SideStepQuestion extends CoachQuestion
{
    /** @param list<Position> $squares */
    public function __construct(
        public readonly int $playerId,
        public readonly array $squares,
        public readonly Position $suggested,
    ) {
        parent::__construct('Side Step question pending');
    }

    public function pending(GameState $state, ReplayContext $replay): GameState
    {
        return $state->withPendingSideStep(new PendingSideStepDTO($this->playerId, $this->squares, $this->suggested, $replay));
    }
}
