<?php

declare(strict_types=1);

namespace App\Engine;

/**
 * P272: hráč kouče-člověka chytil / zachytil bombu a kouč ještě neřekl, kam ji hodit dál (viz `ReplayDiceRoller`).
 * Chytá ji jen `ActionResolver::resolve`, který vrátí stav PŘED akcí s `PendingBombRethrowDTO`.
 */
final class BombRethrowQuestion extends \RuntimeException
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
}
