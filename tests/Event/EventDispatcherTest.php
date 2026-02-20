<?php

declare(strict_types=1);

namespace App\Tests\Event;

use App\Event\EventDispatcher;
use PHPUnit\Framework\TestCase;

final class EventDispatcherTest extends TestCase
{
    public function testDispatchCallsSubscribedListener(): void
    {
        $dispatcher = new EventDispatcher();
        $called = false;

        $dispatcher->subscribe(\stdClass::class, function () use (&$called) {
            $called = true;
        });

        $dispatcher->dispatch(new \stdClass());
        $this->assertTrue($called);
    }

    public function testDispatchCallsMultipleListeners(): void
    {
        $dispatcher = new EventDispatcher();
        $count = 0;

        $dispatcher->subscribe(\stdClass::class, function () use (&$count) { $count++; });
        $dispatcher->subscribe(\stdClass::class, function () use (&$count) { $count++; });

        $dispatcher->dispatch(new \stdClass());
        $this->assertSame(2, $count);
    }

    public function testDispatchIgnoresUnrelatedListeners(): void
    {
        $dispatcher = new EventDispatcher();
        $called = false;

        $dispatcher->subscribe(\RuntimeException::class, function () use (&$called) {
            $called = true;
        });

        $dispatcher->dispatch(new \stdClass());
        $this->assertFalse($called);
    }

    public function testDispatchPassesEventToListener(): void
    {
        $dispatcher = new EventDispatcher();
        $received = null;

        $dispatcher->subscribe(\stdClass::class, function (object $e) use (&$received) {
            $received = $e;
        });

        $event = new \stdClass();
        $event->value = 42;
        $dispatcher->dispatch($event);

        $this->assertSame(42, $received->value);
    }

    public function testNoListenersDoesNotFail(): void
    {
        $dispatcher = new EventDispatcher();
        $dispatcher->dispatch(new \stdClass());
        $this->assertTrue(true);
    }
}
