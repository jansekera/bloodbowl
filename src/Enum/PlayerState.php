<?php
declare(strict_types=1);

namespace App\Enum;

enum PlayerState: string
{
    case STANDING = 'standing';
    case PRONE = 'prone';
    case STUNNED = 'stunned';
    case KO = 'ko';
    case INJURED = 'injured';
    case DEAD = 'dead';
    case EJECTED = 'ejected';
    case OFF_PITCH = 'off_pitch';

    public function isOnPitch(): bool
    {
        return match ($this) {
            self::STANDING, self::PRONE, self::STUNNED => true,
            default => false,
        };
    }

    public function canAct(): bool
    {
        return $this === self::STANDING;
    }

    public function exertsTacklezone(): bool
    {
        return $this === self::STANDING;
    }
}
