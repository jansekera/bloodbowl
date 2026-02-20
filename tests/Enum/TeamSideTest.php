<?php
declare(strict_types=1);

namespace App\Tests\Enum;

use App\Enum\TeamSide;
use PHPUnit\Framework\TestCase;

final class TeamSideTest extends TestCase
{
    public function testOpponent(): void
    {
        $this->assertSame(TeamSide::AWAY, TeamSide::HOME->opponent());
        $this->assertSame(TeamSide::HOME, TeamSide::AWAY->opponent());
    }
}
