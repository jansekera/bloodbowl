<?php

declare(strict_types=1);

namespace App\Tests\ValueObject;

use App\ValueObject\Position;
use PHPUnit\Framework\TestCase;

final class PositionTest extends TestCase
{
    public function testGetters(): void
    {
        $pos = new Position(5, 7);

        $this->assertSame(5, $pos->getX());
        $this->assertSame(7, $pos->getY());
    }

    public function testIsOnPitch(): void
    {
        $this->assertTrue((new Position(0, 0))->isOnPitch());
        $this->assertTrue((new Position(25, 14))->isOnPitch());
        $this->assertTrue((new Position(13, 7))->isOnPitch());

        $this->assertFalse((new Position(-1, 0))->isOnPitch());
        $this->assertFalse((new Position(0, -1))->isOnPitch());
        $this->assertFalse((new Position(26, 0))->isOnPitch());
        $this->assertFalse((new Position(0, 15))->isOnPitch());
    }

    public function testIsInEndZone(): void
    {
        // Home end zone is x=0
        $this->assertTrue((new Position(0, 7))->isInEndZone(true));
        $this->assertFalse((new Position(1, 7))->isInEndZone(true));

        // Away end zone is x=25
        $this->assertTrue((new Position(25, 7))->isInEndZone(false));
        $this->assertFalse((new Position(24, 7))->isInEndZone(false));
    }

    public function testIsInWideZone(): void
    {
        // Wide zones: y < 4 or y >= 11
        $this->assertTrue((new Position(13, 0))->isInWideZone());
        $this->assertTrue((new Position(13, 3))->isInWideZone());
        $this->assertFalse((new Position(13, 4))->isInWideZone());
        $this->assertFalse((new Position(13, 10))->isInWideZone());
        $this->assertTrue((new Position(13, 11))->isInWideZone());
        $this->assertTrue((new Position(13, 14))->isInWideZone());
    }

    public function testDistanceTo(): void
    {
        $pos1 = new Position(5, 5);

        $this->assertSame(0, $pos1->distanceTo(new Position(5, 5)));
        $this->assertSame(1, $pos1->distanceTo(new Position(6, 5)));
        $this->assertSame(1, $pos1->distanceTo(new Position(6, 6)));
        $this->assertSame(3, $pos1->distanceTo(new Position(8, 7)));
    }

    public function testEquals(): void
    {
        $pos1 = new Position(3, 7);
        $pos2 = new Position(3, 7);
        $pos3 = new Position(3, 8);

        $this->assertTrue($pos1->equals($pos2));
        $this->assertFalse($pos1->equals($pos3));
    }

    public function testGetAdjacentPositions(): void
    {
        // Center position: 8 adjacent
        $center = new Position(5, 5);
        $adjacent = $center->getAdjacentPositions();
        $this->assertCount(8, $adjacent);

        // Corner position: 3 adjacent
        $corner = new Position(0, 0);
        $adjacent = $corner->getAdjacentPositions();
        $this->assertCount(3, $adjacent);

        // Edge position: 5 adjacent
        $edge = new Position(0, 5);
        $adjacent = $edge->getAdjacentPositions();
        $this->assertCount(5, $adjacent);
    }

    public function testToArray(): void
    {
        $pos = new Position(5, 7);
        $this->assertSame(['x' => 5, 'y' => 7], $pos->toArray());
    }

    public function testToString(): void
    {
        $pos = new Position(5, 7);
        $this->assertSame('(5,7)', (string) $pos);
    }
}
