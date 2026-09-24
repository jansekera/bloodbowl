<?php

declare(strict_types=1);

namespace App\Enum;

/**
 * Result of the CRP Casualty Table (D68) -- `rules_bb2016.txt` r. 2405-2423.
 *
 * ⭐ Rozlozeni: D6 da desitky, D8 jednotky, takze je to 48 poli, ne 68.
 *   11-38 Badly Hurt (24/48 = 50 %) · 41-48 Miss Next Game (8/48) ·
 *   51-52 Niggling · 53-54 -1 MA · 55-56 -1 AV · 57 -1 AG · 58 -1 ST ·
 *   61-68 DEAD (8/48 = kazda sesta casualty).
 *
 * ⚠️ V JEDNOM ZAPASE jsou vsechny krome DEAD rovnocenne -- hrac je venku do
 *   konce zapasu tak jako tak. Rozdil je az v LIZE, kde se prenasi do dalsiho
 *   zapasu (M/N do soupisky, trvale -1 k vlastnosti). Proto se vysledek
 *   ZAZNAMENAVA, i kdyz ho zatim nic necte.
 */
enum CasualtyResult: string
{
    case BADLY_HURT     = 'badly_hurt';
    case MISS_NEXT_GAME = 'miss_next_game';
    case NIGGLING       = 'niggling';
    case MA_LOSS        = 'ma_loss';
    case AV_LOSS        = 'av_loss';
    case AG_LOSS        = 'ag_loss';
    case ST_LOSS        = 'st_loss';
    case DEAD           = 'dead';

    public function label(): string
    {
        return match ($this) {
            self::BADLY_HURT     => 'Badly Hurt',
            self::MISS_NEXT_GAME => 'Miss Next Game',
            self::NIGGLING       => 'Niggling Injury',
            self::MA_LOSS        => '-1 MA',
            self::AV_LOSS        => '-1 AV',
            self::AG_LOSS        => '-1 AG',
            self::ST_LOSS        => '-1 ST',
            self::DEAD           => 'DEAD',
        };
    }

    /**
     * Zavaznost pro porovnani dvou hodu -- nizsi je mirnejsi.
     *
     * Pouzije ji apothecary: "you can use the Apothecary to make your opponent
     * roll again on the Casualty table and then you CHOOSE which of the two
     * results to apply" (r. 3283-3286). Bez usporadani se "mirnejsi" vybrat neda.
     */
    public function severity(): int
    {
        return match ($this) {
            self::BADLY_HURT     => 0,
            self::MISS_NEXT_GAME => 1,
            self::NIGGLING       => 2,
            self::MA_LOSS, self::AV_LOSS, self::AG_LOSS, self::ST_LOSS => 3,
            self::DEAD           => 4,
        };
    }

    /** Prezije hrac do dalsiho zapasu? V lize jediny rozdil, ktery je nevratny. */
    public function isPermanent(): bool
    {
        return $this === self::DEAD;
    }
}
