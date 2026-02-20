<?php

declare(strict_types=1);

namespace App\Event;

interface EventDispatcherInterface
{
    public function dispatch(object $event): void;

    public function subscribe(string $eventClass, callable $listener): void;
}
