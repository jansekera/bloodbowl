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

    // ⛔⛔ OPRAVENO 15.09.2026 -- testy kodovaly VADNOU tabulku enginu
    //   (2-3 / 4-5 / 6-8 / 9-10 / 11-12). `rules_bb2016.txt` r. 1476-1494
    //   (standardni tabulka, prvni ze dvou sloupcu; druha je zimni):
    //   2 Sweltering Heat · 3 Very Sunny · 4-10 Nice · 11 Pouring Rain · 12 Blizzard.
    //   Tak to ma i C++ engine (`engine/include/bb/enums.h:240`).

    public function testFromRollSwelteringHeatJenDva(): void
    {
        $this->assertEquals(Weather::SWELTERING_HEAT, Weather::fromRoll(2));
    }

    public function testFromRollVerySunnyJenTri(): void
    {
        $this->assertEquals(Weather::VERY_SUNNY, Weather::fromRoll(3));
    }

    public function testFromRollNiceCtyriAzDeset(): void
    {
        foreach (range(4, 10) as $hod) {
            $this->assertEquals(Weather::NICE, Weather::fromRoll($hod), "hod {$hod}");
        }
    }

    public function testFromRollPouringRainJenJedenact(): void
    {
        $this->assertEquals(Weather::POURING_RAIN, Weather::fromRoll(11));
    }

    public function testFromRollBlizzardJenDvanact(): void
    {
        $this->assertEquals(Weather::BLIZZARD, Weather::fromRoll(12));
    }
}
