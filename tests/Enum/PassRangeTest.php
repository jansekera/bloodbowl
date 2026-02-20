<?php

declare(strict_types=1);

namespace Tests\Enum;

use App\Enum\PassRange;
use PHPUnit\Framework\TestCase;

final class PassRangeTest extends TestCase
{
    public function testFromDistanceQuickPass(): void
    {
        $this->assertEquals(PassRange::QUICK_PASS, PassRange::fromDistance(1));
        $this->assertEquals(PassRange::QUICK_PASS, PassRange::fromDistance(3));
    }

    public function testFromDistanceShortPass(): void
    {
        $this->assertEquals(PassRange::SHORT_PASS, PassRange::fromDistance(4));
        $this->assertEquals(PassRange::SHORT_PASS, PassRange::fromDistance(6));
    }

    public function testFromDistanceLongPass(): void
    {
        $this->assertEquals(PassRange::LONG_PASS, PassRange::fromDistance(7));
        $this->assertEquals(PassRange::LONG_PASS, PassRange::fromDistance(10));
    }

    public function testFromDistanceLongBomb(): void
    {
        $this->assertEquals(PassRange::LONG_BOMB, PassRange::fromDistance(11));
        $this->assertEquals(PassRange::LONG_BOMB, PassRange::fromDistance(13));
    }

    public function testFromDistanceOutOfRange(): void
    {
        $this->assertNull(PassRange::fromDistance(14));
        $this->assertNull(PassRange::fromDistance(20));
    }

    public function testModifiers(): void
    {
        $this->assertEquals(1, PassRange::QUICK_PASS->modifier());
        $this->assertEquals(0, PassRange::SHORT_PASS->modifier());
        $this->assertEquals(-1, PassRange::LONG_PASS->modifier());
        $this->assertEquals(-2, PassRange::LONG_BOMB->modifier());
    }
}
