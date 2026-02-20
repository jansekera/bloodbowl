<?php
declare(strict_types=1);

namespace App\Enum;

enum GamePhase: string
{
    case COIN_TOSS = 'coin_toss';
    case SETUP = 'setup';
    case KICKOFF = 'kickoff';
    case PLAY = 'play';
    case TOUCHDOWN = 'touchdown';
    case HALF_TIME = 'half_time';
    case GAME_OVER = 'game_over';

    public function isPlayable(): bool
    {
        return $this === self::PLAY;
    }

    public function isSetup(): bool
    {
        return $this === self::SETUP;
    }

    public function isFinished(): bool
    {
        return $this === self::GAME_OVER;
    }
}
