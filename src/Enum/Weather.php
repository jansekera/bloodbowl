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
     * 2-3: Sweltering Heat, 4-5: Very Sunny, 6-8: Nice, 9-10: Pouring Rain, 11-12: Blizzard
     */
    public static function fromRoll(int $roll): self
    {
        return match (true) {
            $roll <= 3 => self::SWELTERING_HEAT,
            $roll <= 5 => self::VERY_SUNNY,
            $roll <= 8 => self::NICE,
            $roll <= 10 => self::POURING_RAIN,
            default => self::BLIZZARD,
        };
    }
}
