<?php

declare(strict_types=1);

namespace App\Event;

final class EventDispatcher implements EventDispatcherInterface
{
    /** @var array<string, list<callable>> */
    private array $listeners = [];

    public function subscribe(string $eventClass, callable $listener): void
    {
        $this->listeners[$eventClass][] = $listener;
    }

    public function dispatch(object $event): void
    {
        $class = $event::class;
        foreach ($this->listeners[$class] ?? [] as $listener) {
            $listener($event);
        }
    }
}
