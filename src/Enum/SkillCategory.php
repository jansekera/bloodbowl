<?php

declare(strict_types=1);

namespace App\Enum;

enum SkillCategory: string
{
    case GENERAL = 'General';
    case AGILITY = 'Agility';
    case STRENGTH = 'Strength';
    case PASSING = 'Passing';
    case MUTATION = 'Mutation';
    case EXTRAORDINARY = 'Extraordinary';
}
