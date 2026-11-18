<?php

declare(strict_types=1);

namespace App\Enum;

/**
 * P306 A7 (18.11.2026): druh hodu, který kouč-člověk smí přehodit — jediné místo pro popisek, kategorii a následek
 * odmítnutí přehozu. Web je jen vykreslí (dřív tři mapy a popisky v `RerollModal.ts` mimo pravidla). Do 18.11. tu byl
 * nepoužívaný výčet s armour/injury/block.
 */
enum RollType: string
{
    case DODGE = 'dodge';
    case GFI = 'gfi';
    case LEAP = 'leap';
    case LEAP_GFI = 'leap_gfi';
    case PICKUP = 'pickup';
    case CATCH = 'catch';
    case BONE_HEAD = 'bone_head';
    case REALLY_STUPID = 'really_stupid';
    case WILD_ANIMAL = 'wild_animal';
    case PASS = 'pass';
    case BOMB = 'bomb';
    case TTM = 'ttm';
    case TTM_LANDING = 'ttm_landing';
    case ALWAYS_HUNGRY = 'always_hungry';
    case JUMP_UP = 'jump_up';

    public function label(): string
    {
        return match ($this) {
            self::DODGE => 'Dodge',
            self::GFI => 'Going For It',
            self::LEAP => 'Leap',
            self::LEAP_GFI => 'Going For It (Leap)',
            self::PICKUP => 'Pickup',
            self::CATCH => 'Catch',
            self::BONE_HEAD => 'Bone Head',
            self::REALLY_STUPID => 'Really Stupid',
            self::WILD_ANIMAL => 'Wild Animal',
            self::PASS => 'Pass',
            self::BOMB => 'Bomb Throw',
            self::TTM => 'Throw Team-Mate',
            self::TTM_LANDING => 'Landing (Right Stuff)',
            self::ALWAYS_HUNGRY => 'Always Hungry',
            self::JUMP_UP => 'Jump Up (blok z lehu)',
        };
    }

    public function category(): RollCategory
    {
        return match ($this) {
            self::DODGE, self::GFI, self::LEAP, self::LEAP_GFI => RollCategory::MOVE,
            self::PICKUP, self::CATCH => RollCategory::BALL,
            self::BONE_HEAD, self::REALLY_STUPID, self::WILD_ANIMAL => RollCategory::BIG_GUY,
            // bez `default` (review P306): nový druh hodu musí dostat kategorii vědomě
            self::PASS, self::BOMB, self::TTM, self::TTM_LANDING, self::ALWAYS_HUNGRY, self::JUMP_UP => RollCategory::THROW,
        };
    }

    /** Co znamená odmítnout přehoz; null = dialog nic navíc neříká. */
    public function consequence(): ?string
    {
        return match ($this) {
            // pád a turnover (ř. 497-499, 1702-1703, 8264-8277)
            self::DODGE, self::GFI, self::LEAP, self::LEAP_GFI => 'Accept Failure = the player falls (armour roll) and the turn ends.',
            // nepovedené zvednutí = turnover (ř. 371-375), chytání ne (ř. 376-378)
            self::PICKUP => 'Accept Failure = the ball bounces and the turn ends.',
            self::CATCH => 'Accept Failure = the ball bounces.',
            // akce propadne (ř. 7980-7986, 8398-8401, 8668-8669)
            self::BONE_HEAD, self::REALLY_STUPID => 'Accept Failure = the player loses the declared action and his tackle zones.',
            self::WILD_ANIMAL => 'Accept Failure = the player loses the declared action.',
            default => null,
        };
    }
}
