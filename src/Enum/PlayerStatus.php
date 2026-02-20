<?php

declare(strict_types=1);

namespace App\Enum;

enum PlayerStatus: string
{
    case ACTIVE = 'active';
    case INJURED = 'injured';
    case DEAD = 'dead';
    case RETIRED = 'retired';

    public function isAvailable(): bool
    {
        return $this === self::ACTIVE;
    }
}
