<?php
declare(strict_types=1);

namespace App\Tests\Engine;

use App\Engine\FixedDiceRoller;
use App\Engine\RandomDiceRoller;
use PHPUnit\Framework\TestCase;

final class DiceRollerTest extends TestCase
{
    public function testFixedDiceRollerReturnsSequence(): void
    {
        $roller = new FixedDiceRoller([3, 5, 2]);
        $this->assertSame(3, $roller->rollD6());
        $this->assertSame(5, $roller->rollD6());
        $this->assertSame(2, $roller->rollD6());
    }

    public function testFixedDiceRollerThrowsWhenExhausted(): void
    {
        $roller = new FixedDiceRoller([1]);
        $roller->rollD6();

        $this->expectException(\RuntimeException::class);
        $roller->rollD6();
    }

    public function testFixedDiceRoller2D6UsesTwoRolls(): void
    {
        $roller = new FixedDiceRoller([3, 4]);
        $this->assertSame(7, $roller->roll2D6());
        $this->assertSame(2, $roller->getRollCount());
    }

    public function testFixedDiceRollerD8(): void
    {
        $roller = new FixedDiceRoller([7]);
        $this->assertSame(7, $roller->rollD8());
    }

    public function testFixedDiceRollerTracksCounts(): void
    {
        $roller = new FixedDiceRoller([1, 2, 3]);
        $this->assertTrue($roller->hasRemainingRolls());
        $this->assertSame(0, $roller->getRollCount());

        $roller->rollD6();
        $this->assertSame(1, $roller->getRollCount());
        $this->assertTrue($roller->hasRemainingRolls());

        $roller->rollD6();
        $roller->rollD6();
        $this->assertSame(3, $roller->getRollCount());
        $this->assertFalse($roller->hasRemainingRolls());
    }

    public function testRandomDiceRollerD6InRange(): void
    {
        $roller = new RandomDiceRoller();
        for ($i = 0; $i < 100; $i++) {
            $roll = $roller->rollD6();
            $this->assertGreaterThanOrEqual(1, $roll);
            $this->assertLessThanOrEqual(6, $roll);
        }
    }

    public function testRandomDiceRoller2D6InRange(): void
    {
        $roller = new RandomDiceRoller();
        for ($i = 0; $i < 100; $i++) {
            $roll = $roller->roll2D6();
            $this->assertGreaterThanOrEqual(2, $roll);
            $this->assertLessThanOrEqual(12, $roll);
        }
    }

    public function testRandomDiceRollerD8InRange(): void
    {
        $roller = new RandomDiceRoller();
        for ($i = 0; $i < 100; $i++) {
            $roll = $roller->rollD8();
            $this->assertGreaterThanOrEqual(1, $roll);
            $this->assertLessThanOrEqual(8, $roll);
        }
    }
}
