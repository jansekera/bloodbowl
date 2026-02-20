<?php
declare(strict_types=1);

namespace App\Enum;

enum TeamSide: string
{
    case HOME = 'home';
    case AWAY = 'away';

    public function opponent(): self
    {
        return $this === self::HOME ? self::AWAY : self::HOME;
    }
}
