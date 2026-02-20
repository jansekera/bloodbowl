<?php

declare(strict_types=1);

namespace App\Tests\Enum;

use App\Enum\PlayerStatus;
use PHPUnit\Framework\TestCase;

final class PlayerStatusTest extends TestCase
{
    public function testValues(): void
    {
        $this->assertSame('active', PlayerStatus::ACTIVE->value);
        $this->assertSame('injured', PlayerStatus::INJURED->value);
        $this->assertSame('dead', PlayerStatus::DEAD->value);
        $this->assertSame('retired', PlayerStatus::RETIRED->value);
        $this->assertCount(4, PlayerStatus::cases());
    }

    public function testIsAvailable(): void
    {
        $this->assertTrue(PlayerStatus::ACTIVE->isAvailable());
        $this->assertFalse(PlayerStatus::INJURED->isAvailable());
        $this->assertFalse(PlayerStatus::DEAD->isAvailable());
        $this->assertFalse(PlayerStatus::RETIRED->isAvailable());
    }
}
