<?php

declare(strict_types=1);

namespace App\Event\Listener;

use App\Event\GameEventOccurred;
use App\Repository\MatchEventRepository;

final class GameLogListener
{
    public function __construct(
        private readonly MatchEventRepository $matchEventRepo,
    ) {
    }

    public function __invoke(GameEventOccurred $event): void
    {
        $this->matchEventRepo->saveAll($event->matchId, [$event->gameEvent]);
    }
}
