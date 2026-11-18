<?php

declare(strict_types=1);

namespace App\Enum;

/**
 * Kategorie přehazovaného hodu (`RollType::category`): co web ukáže na hřišti — pohyb = odkud kam, míč = pole
 * hodu, velký hráč a hod = nic navíc. Velké hráče AI nepřehazuje (`Coach::reroll`, C++ P130).
 */
enum RollCategory: string
{
    case MOVE = 'move';
    case BALL = 'ball';
    case BIG_GUY = 'big_guy';
    case THROW = 'throw';
}
