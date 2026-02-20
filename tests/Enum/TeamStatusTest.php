<?php

declare(strict_types=1);

namespace App\Tests\Enum;

use App\Enum\TeamStatus;
use PHPUnit\Framework\TestCase;

final class TeamStatusTest extends TestCase
{
    public function testValues(): void
    {
        $this->assertSame('active', TeamStatus::ACTIVE->value);
        $this->assertSame('retired', TeamStatus::RETIRED->value);
        $this->assertCount(2, TeamStatus::cases());
    }

    public function testFromString(): void
    {
        $this->assertSame(TeamStatus::ACTIVE, TeamStatus::from('active'));
        $this->assertSame(TeamStatus::RETIRED, TeamStatus::from('retired'));
    }
}
