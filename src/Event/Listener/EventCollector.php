<?php

declare(strict_types=1);

namespace App\Event\Listener;

use App\DTO\GameEvent;
use App\Event\GameEventOccurred;

final class EventCollector
{
    /** @var list<GameEvent> */
    private array $events = [];

    public function __invoke(GameEventOccurred $event): void
    {
        $this->events[] = $event->gameEvent;
    }

    /** @return list<GameEvent> */
    public function getEvents(): array
    {
        return $this->events;
    }

    public function clear(): void
    {
        $this->events = [];
    }
}
