<?php

declare(strict_types=1);

namespace App\Enum;

enum KickoffEvent: int
{
    case GetTheRef = 2;
    case Riot = 3;
    case PerfectDefence = 4;
    case HighKick = 5;
    case Cheering = 6;
    case BrilliantCoaching = 7;
    case ChangingWeather = 8;
    case QuickSnap = 9;
    case Blitz = 10;
    case ThrowARock = 11;
    case PitchInvasion = 12;

    public function label(): string
    {
        return match ($this) {
            self::GetTheRef => 'Get the Ref!',
            self::Riot => 'Riot!',
            self::PerfectDefence => 'Perfect Defence',
            self::HighKick => 'High Kick',
            self::Cheering => 'Cheering Fans',
            self::BrilliantCoaching => 'Brilliant Coaching',
            self::ChangingWeather => 'Changing Weather',
            self::QuickSnap => 'Quick Snap!',
            self::Blitz => 'Blitz!',
            self::ThrowARock => 'Throw a Rock!',
            self::PitchInvasion => 'Pitch Invasion!',
        };
    }
}
