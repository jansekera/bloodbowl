<?php

declare(strict_types=1);

namespace App\Event;

use App\DTO\GameEvent;

final class GameEventOccurred
{
    public function __construct(
        public readonly int $matchId,
        public readonly GameEvent $gameEvent,
    ) {
    }
}
