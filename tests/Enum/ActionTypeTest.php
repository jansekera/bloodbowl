<?php
declare(strict_types=1);

namespace App\Tests\Enum;

use App\Enum\ActionType;
use PHPUnit\Framework\TestCase;

final class ActionTypeTest extends TestCase
{
    public function testRequiresPlayer(): void
    {
        $this->assertTrue(ActionType::MOVE->requiresPlayer());
        $this->assertTrue(ActionType::BLOCK->requiresPlayer());
        $this->assertFalse(ActionType::END_TURN->requiresPlayer());
        $this->assertFalse(ActionType::END_SETUP->requiresPlayer());
    }

    public function testOncePerTurn(): void
    {
        $this->assertTrue(ActionType::BLITZ->isOncePerTurn());
        $this->assertTrue(ActionType::PASS->isOncePerTurn());
        $this->assertTrue(ActionType::FOUL->isOncePerTurn());
        $this->assertFalse(ActionType::MOVE->isOncePerTurn());
        $this->assertFalse(ActionType::BLOCK->isOncePerTurn());
    }
}
