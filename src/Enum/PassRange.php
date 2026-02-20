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
