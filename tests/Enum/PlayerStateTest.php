<?php
declare(strict_types=1);

namespace App\Tests\Enum;

use App\Enum\PlayerState;
use PHPUnit\Framework\TestCase;

final class PlayerStateTest extends TestCase
{
    public function testOnPitch(): void
    {
        $this->assertTrue(PlayerState::STANDING->isOnPitch());
        $this->assertTrue(PlayerState::PRONE->isOnPitch());
        $this->assertTrue(PlayerState::STUNNED->isOnPitch());
        $this->assertFalse(PlayerState::KO->isOnPitch());
        $this->assertFalse(PlayerState::INJURED->isOnPitch());
        $this->assertFalse(PlayerState::OFF_PITCH->isOnPitch());
    }

    public function testCanAct(): void
    {
        $this->assertTrue(PlayerState::STANDING->canAct());
        $this->assertFalse(PlayerState::PRONE->canAct());
        $this->assertFalse(PlayerState::STUNNED->canAct());
    }

    public function testExertsTacklezone(): void
    {
        $this->assertTrue(PlayerState::STANDING->exertsTacklezone());
        $this->assertFalse(PlayerState::PRONE->exertsTacklezone());
        $this->assertFalse(PlayerState::STUNNED->exertsTacklezone());
    }
}
