<?php
declare(strict_types=1);

namespace App\Tests\Enum;

use App\Enum\GamePhase;
use PHPUnit\Framework\TestCase;

final class GamePhaseTest extends TestCase
{
    public function testPlayable(): void
    {
        $this->assertTrue(GamePhase::PLAY->isPlayable());
        $this->assertFalse(GamePhase::SETUP->isPlayable());
        $this->assertFalse(GamePhase::GAME_OVER->isPlayable());
    }

    public function testSetup(): void
    {
        $this->assertTrue(GamePhase::SETUP->isSetup());
        $this->assertFalse(GamePhase::PLAY->isSetup());
    }

    public function testFinished(): void
    {
        $this->assertTrue(GamePhase::GAME_OVER->isFinished());
        $this->assertFalse(GamePhase::PLAY->isFinished());
    }
}
