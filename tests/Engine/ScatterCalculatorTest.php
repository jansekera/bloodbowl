<?php

declare(strict_types=1);

namespace Tests\Engine;

use App\Engine\ScatterCalculator;
use App\Enum\TeamSide;
use App\ValueObject\Position;
use PHPUnit\Framework\TestCase;

final class ScatterCalculatorTest extends TestCase
{
    private ScatterCalculator $calc;

    protected function setUp(): void
    {
        $this->calc = new ScatterCalculator();
    }

    public function testScatterOnceNorth(): void
    {
        $result = $this->calc->scatterOnce(new Position(10, 7), 1);
        $this->assertEquals(10, $result->getX());
        $this->assertEquals(6, $result->getY());
    }

    public function testScatterOnceNorthEast(): void
    {
        $result = $this->calc->scatterOnce(new Position(10, 7), 2);
        $this->assertEquals(11, $result->getX());
        $this->assertEquals(6, $result->getY());
    }

    public function testScatterOnceEast(): void
    {
        $result = $this->calc->scatterOnce(new Position(10, 7), 3);
        $this->assertEquals(11, $result->getX());
        $this->assertEquals(7, $result->getY());
    }

    public function testScatterOnceSouthWest(): void
    {
        $result = $this->calc->scatterOnce(new Position(10, 7), 6);
        $this->assertEquals(9, $result->getX());
        $this->assertEquals(8, $result->getY());
    }

    public function testScatterWithDistanceEast3(): void
    {
        $result = $this->calc->scatterWithDistance(new Position(10, 7), 3, 3);
        $this->assertEquals(13, $result->getX());
        $this->assertEquals(7, $result->getY());
    }

    public function testScatterWithDistanceSouth5(): void
    {
        $result = $this->calc->scatterWithDistance(new Position(10, 7), 5, 5);
        $this->assertEquals(10, $result->getX());
        $this->assertEquals(12, $result->getY());
    }

    public function testScatterOffPitch(): void
    {
        $result = $this->calc->scatterOnce(new Position(0, 0), 8); // NW from corner
        $this->assertFalse($result->isOnPitch());
    }

    public function testScatterWithDistanceOffPitch(): void
    {
        $result = $this->calc->scatterWithDistance(new Position(3, 7), 7, 6); // W by 6
        $this->assertEquals(-3, $result->getX());
        $this->assertFalse($result->isOnPitch());
    }

    public function testIsInReceivingHalfHomeReceives(): void
    {
        $this->assertTrue($this->calc->isInReceivingHalf(new Position(5, 7), TeamSide::HOME));
        $this->assertTrue($this->calc->isInReceivingHalf(new Position(12, 7), TeamSide::HOME));
        $this->assertFalse($this->calc->isInReceivingHalf(new Position(13, 7), TeamSide::HOME));
        $this->assertFalse($this->calc->isInReceivingHalf(new Position(20, 7), TeamSide::HOME));
    }

    public function testIsInReceivingHalfAwayReceives(): void
    {
        $this->assertTrue($this->calc->isInReceivingHalf(new Position(20, 7), TeamSide::AWAY));
        $this->assertTrue($this->calc->isInReceivingHalf(new Position(13, 7), TeamSide::AWAY));
        $this->assertFalse($this->calc->isInReceivingHalf(new Position(12, 7), TeamSide::AWAY));
        $this->assertFalse($this->calc->isInReceivingHalf(new Position(5, 7), TeamSide::AWAY));
    }

    public function testIsInReceivingHalfOffPitchReturnsFalse(): void
    {
        $this->assertFalse($this->calc->isInReceivingHalf(new Position(-1, 7), TeamSide::HOME));
        $this->assertFalse($this->calc->isInReceivingHalf(new Position(30, 7), TeamSide::AWAY));
    }

    public function testGetDirectionOffset(): void
    {
        $this->assertEquals([0, -1], $this->calc->getDirectionOffset(1));
        $this->assertEquals([1, 0], $this->calc->getDirectionOffset(3));
        $this->assertEquals([0, 1], $this->calc->getDirectionOffset(5));
        $this->assertEquals([-1, 0], $this->calc->getDirectionOffset(7));
    }
}
