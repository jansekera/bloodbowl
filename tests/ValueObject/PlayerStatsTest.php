<?php

declare(strict_types=1);

namespace App\Tests\ValueObject;

use App\ValueObject\PlayerStats;
use PHPUnit\Framework\TestCase;

final class PlayerStatsTest extends TestCase
{
    public function testGetters(): void
    {
        $stats = new PlayerStats(6, 3, 3, 8);

        $this->assertSame(6, $stats->getMovement());
        $this->assertSame(3, $stats->getStrength());
        $this->assertSame(3, $stats->getAgility());
        $this->assertSame(8, $stats->getArmour());
    }

    public function testToArray(): void
    {
        $stats = new PlayerStats(7, 4, 2, 9);

        $this->assertSame([
            'movement' => 7,
            'strength' => 4,
            'agility' => 2,
            'armour' => 9,
        ], $stats->toArray());
    }

    public function testEquals(): void
    {
        $stats1 = new PlayerStats(6, 3, 3, 8);
        $stats2 = new PlayerStats(6, 3, 3, 8);
        $stats3 = new PlayerStats(7, 3, 3, 8);

        $this->assertTrue($stats1->equals($stats2));
        $this->assertFalse($stats1->equals($stats3));
    }
}
