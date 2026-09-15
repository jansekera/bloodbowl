<?php

declare(strict_types=1);

namespace App\Enum;

enum PassRange: string
{
    case QUICK_PASS = 'quick_pass';
    case SHORT_PASS = 'short_pass';
    case LONG_PASS = 'long_pass';
    case LONG_BOMB = 'long_bomb';

    public function modifier(): int
    {
        return match ($this) {
            self::QUICK_PASS => 1,
            self::SHORT_PASS => 0,
            self::LONG_PASS => -1,
            self::LONG_BOMB => -2,
        };
    }

    /**
     * ⭐ 15.09.2026: ve vanici jen quick a short (`rules_bb2016.txt` r. 1490-1494:
     *   "the snow means that only quick or short passes can be attempted").
     *   Plati pro ZMERENY dosah -- Strong Arm meni modifikator hodu, ne to,
     *   jak daleko mic leti.
     */
    public function povolenaZaPocasi(\App\Enum\Weather $pocasi): bool
    {
        return $pocasi !== \App\Enum\Weather::BLIZZARD
            || $this === self::QUICK_PASS
            || $this === self::SHORT_PASS;
    }

    public static function fromDistance(int $distance): ?self
    {
        return match (true) {
            $distance <= 3 => self::QUICK_PASS,
            $distance <= 6 => self::SHORT_PASS,
            $distance <= 10 => self::LONG_PASS,
            $distance <= 13 => self::LONG_BOMB,
            default => null,
        };
    }

    public function reduced(): self
    {
        return match ($this) {
            self::LONG_BOMB => self::LONG_PASS,
            self::LONG_PASS => self::SHORT_PASS,
            self::SHORT_PASS => self::QUICK_PASS,
            self::QUICK_PASS => self::QUICK_PASS,
        };
    }
}
