<?php

declare(strict_types=1);

namespace App\Enum;

enum TeamStatus: string
{
    case ACTIVE = 'active';
    case RETIRED = 'retired';
}
