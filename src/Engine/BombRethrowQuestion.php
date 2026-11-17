<?php

declare(strict_types=1);

namespace App\Engine;

use App\DTO\PendingBombRethrowDTO;
use App\DTO\GameState;
use App\DTO\ReplayContext;

/**
 * P272: hráč kouče-člověka chytil / zachytil bombu a kouč ještě neřekl, kam ji hodit dál (viz `ReplayDiceRoller`).
 * Chytá ji jen `ActionResolver::resolve`, který vrátí stav PŘED akcí s `PendingBombRethrowDTO`.
 */
final class BombRethrowQuestion extends CoachQuestion
{
    /**
     * @param list<array{x: int, y: int, range: string}> $targets legální pole hodu (ThrowTarget::cile)
     * @param array{x: int, y: int}|null $suggested pole, které by zvolil engine (BombTarget::vyber)
     */
    public function __construct(
        public readonly int $playerId,
        public readonly array $targets,
        public readonly ?array $suggested,
    ) {
        parent::__construct('Bomb rethrow target question pending');
    }

    public function pending(GameState $state, ReplayContext $replay): GameState
    {
        return $state->withPendingBombRethrow(new PendingBombRethrowDTO(
            $this->playerId,
            $this->targets,
            $this->suggested,
            $replay,
        ));
    }
}
