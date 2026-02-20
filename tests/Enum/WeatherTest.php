<?php

declare(strict_types=1);

namespace App\Tests\Enum;

use App\Enum\Weather;
use PHPUnit\Framework\TestCase;

final class WeatherTest extends TestCase
{
    public function testLabels(): void
    {
        $this->assertEquals('Sweltering Heat', Weather::SWELTERING_HEAT->label());
        $this->assertEquals('Very Sunny', Weather::VERY_SUNNY->label());
        $this->assertEquals('Nice', Weather::NICE->label());
        $this->assertEquals('Pouring Rain', Weather::POURING_RAIN->label());
        $this->assertEquals('Blizzard', Weather::BLIZZARD->label());
    }

    public function testFromRollSwelteringHeat(): void
    {
        $this->assertEquals(Weather::SWELTERING_HEAT, Weather::fromRoll(2));
        $this->assertEquals(Weather::SWELTERING_HEAT, Weather::fromRoll(3));
    }

    public function testFromRollVerySunny(): void
    {
        $this->assertEquals(Weather::VERY_SUNNY, Weather::fromRoll(4));
        $this->assertEquals(Weather::VERY_SUNNY, Weather::fromRoll(5));
    }

    public function testFromRollNice(): void
    {
        $this->assertEquals(Weather::NICE, Weather::fromRoll(6));
        $this->assertEquals(Weather::NICE, Weather::fromRoll(7));
        $this->assertEquals(Weather::NICE, Weather::fromRoll(8));
    }

    public function testFromRollPouringRain(): void
    {
        $this->assertEquals(Weather::POURING_RAIN, Weather::fromRoll(9));
        $this->assertEquals(Weather::POURING_RAIN, Weather::fromRoll(10));
    }

    public function testFromRollBlizzard(): void
    {
        $this->assertEquals(Weather::BLIZZARD, Weather::fromRoll(11));
        $this->assertEquals(Weather::BLIZZARD, Weather::fromRoll(12));
    }
}
