<?php
declare(strict_types=1);

namespace App\Tests\DTO;

use App\DTO\BallState;
use App\ValueObject\Position;
use PHPUnit\Framework\TestCase;

final class BallStateTest extends TestCase
{
    public function testOnGround(): void
    {
        $ball = BallState::onGround(new Position(5, 5));
        $this->assertFalse($ball->isHeld());
        $this->assertNull($ball->getCarrierId());
        $this->assertTrue($ball->isOnPitch());
        $this->assertSame(5, $ball->getPosition()?->getX());
    }

    public function testCarried(): void
    {
        $ball = BallState::carried(new Position(5, 5), 7);
        $this->assertTrue($ball->isHeld());
        $this->assertSame(7, $ball->getCarrierId());
        $this->assertTrue($ball->isOnPitch());
    }

    public function testOffPitch(): void
    {
        $ball = BallState::offPitch();
        $this->assertFalse($ball->isHeld());
        $this->assertNull($ball->getCarrierId());
        $this->assertFalse($ball->isOnPitch());
    }

    public function testSerializationRoundTrip(): void
    {
        $ball = BallState::carried(new Position(10, 7), 3);
        $array = $ball->toArray();
        $restored = BallState::fromArray($array);

        $this->assertTrue($restored->isHeld());
        $this->assertSame(3, $restored->getCarrierId());
        $this->assertSame(10, $restored->getPosition()?->getX());
        $this->assertSame(7, $restored->getPosition()->getY());
    }
}
