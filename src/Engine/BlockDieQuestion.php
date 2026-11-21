<?php

declare(strict_types=1);

namespace App\Engine;

use App\DTO\GameState;
use App\DTO\PendingBlockDieDTO;
use App\DTO\ReplayContext;
use App\Enum\BlockDiceFace;

/**
 * P308/P319/P320: kostku bloku vybírá kouč-člověk (`rules_bb2016.txt` ř. 633-634) a ještě
 * nevybral (viz `Coach::blockDie`). Chytá ji jen `ActionResolver::resolve` (stav v okamžiku otázky
 * s `PendingBlockDieDTO`).
 */
final class BlockDieQuestion extends CoachQuestion
{
    /** @param list<BlockDiceFace> $faces */
    public function __construct(
        public readonly int $attackerId,
        public readonly int $defenderId,
        public readonly array $faces,
        public readonly bool $isFrenzy,
        public readonly bool $attackerChooses,
    ) {
        parent::__construct('Block die question pending');
    }

    public function pending(GameState $state, ReplayContext $replay): GameState
    {
        return $state->withPendingBlockDie(new PendingBlockDieDTO(
            $this->attackerId,
            $this->defenderId,
            $this->faces,
            $this->isFrenzy,
            $this->attackerChooses,
            $replay,
        ));
    }
}
