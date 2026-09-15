<?php

declare(strict_types=1);

namespace App\Enum;

enum Weather: string
{
    case SWELTERING_HEAT = 'sweltering_heat';
    case VERY_SUNNY = 'very_sunny';
    case NICE = 'nice';
    case POURING_RAIN = 'pouring_rain';
    case BLIZZARD = 'blizzard';

    public function label(): string
    {
        return match ($this) {
            self::SWELTERING_HEAT => 'Sweltering Heat',
            self::VERY_SUNNY => 'Very Sunny',
            self::NICE => 'Nice',
            self::POURING_RAIN => 'Pouring Rain',
            self::BLIZZARD => 'Blizzard',
        };
    }

    /**
     * Map 2D6 roll to weather.
     *
     * ⛔⛔ OPRAVENO 15.09.2026 -- tady bylo 2-3 / 4-5 / 6-8 / 9-10 / 11-12, takze
     *   pekne pocasi padalo v 16 z 36 hodu misto 30 a vanice 3x casteji.
     *   `rules_bb2016.txt` r. 1476-1494 (standardni tabulka):
     *   2 Sweltering Heat · 3 Very Sunny · 4-10 Nice · 11 Pouring Rain · 12 Blizzard.
     *   Shodne s C++ enginem (`engine/include/bb/enums.h:240`).
     */
    public static function fromRoll(int $roll): self
    {
        return match (true) {
            $roll <= 2 => self::SWELTERING_HEAT,
            $roll === 3 => self::VERY_SUNNY,
            $roll <= 10 => self::NICE,
            $roll === 11 => self::POURING_RAIN,
            default => self::BLIZZARD,
        };
    }
}
