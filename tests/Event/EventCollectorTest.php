<?php

declare(strict_types=1);

namespace App\Tests\Event;

use App\DTO\GameEvent;
use App\Event\EventDispatcher;
use App\Event\GameEventOccurred;
use App\Event\Listener\EventCollector;
use PHPUnit\Framework\TestCase;

final class EventCollectorTest extends TestCase
{
    public function testCollectsGameEvents(): void
    {
        $collector = new EventCollector();
        $dispatcher = new EventDispatcher();
        $dispatcher->subscribe(GameEventOccurred::class, $collector);

        $event1 = GameEvent::endTurn('Home');
        $event2 = GameEvent::turnover('Failed dodge');

        $dispatcher->dispatch(new GameEventOccurred(1, $event1));
        $dispatcher->dispatch(new GameEventOccurred(1, $event2));

        $this->assertCount(2, $collector->getEvents());
        $this->assertSame($event1, $collector->getEvents()[0]);
        $this->assertSame($event2, $collector->getEvents()[1]);
    }

    public function testClearResetsEvents(): void
    {
        $collector = new EventCollector();

        $collector(new GameEventOccurred(1, GameEvent::endTurn('Home')));
        $this->assertCount(1, $collector->getEvents());

        $collector->clear();
        $this->assertCount(0, $collector->getEvents());
    }

    public function testEmptyByDefault(): void
    {
        $collector = new EventCollector();
        $this->assertSame([], $collector->getEvents());
    }
}
