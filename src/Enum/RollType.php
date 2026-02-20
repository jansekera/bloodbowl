<?php
declare(strict_types=1);

namespace App\Enum;

enum RollType: string
{
    case DODGE = 'dodge';
    case GFI = 'gfi';
    case PICKUP = 'pickup';
    case PASS = 'pass';
    case CATCH = 'catch';
    case ARMOUR = 'armour';
    case INJURY = 'injury';
    case BLOCK = 'block';
}
