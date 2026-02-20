<?php
declare(strict_types=1);

namespace App\Enum;

enum BlockDiceFace: string
{
    case ATTACKER_DOWN = 'attacker_down';
    case BOTH_DOWN = 'both_down';
    case PUSHED = 'pushed';
    case DEFENDER_STUMBLES = 'defender_stumbles';
    case DEFENDER_DOWN = 'defender_down';
    case POW = 'pow';
}
