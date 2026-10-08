<?php

declare(strict_types=1);

namespace App\Enum;

enum PassRange: string
{
    case QUICK_PASS = 'quick_pass';
    case SHORT_PASS = 'short_pass';
    case LONG_PASS = 'long_pass';
    case LONG_BOMB = 'long_bomb';

    public function modifier(): int
    {
        return match ($this) {
            self::QUICK_PASS => 1,
            self::SHORT_PASS => 0,
            self::LONG_PASS => -1,
            self::LONG_BOMB => -2,
        };
    }

    /**
     * ⭐ 15.09.2026: ve vanici jen quick a short (`rules_bb2016.txt` r. 1490-1494:
     *   "the snow means that only quick or short passes can be attempted").
     *   Plati pro ZMERENY dosah -- Strong Arm meni modifikator hodu, ne to,
     *   jak daleko mic leti.
     */
    public function povolenaZaPocasi(\App\Enum\Weather $pocasi): bool
    {
        return $pocasi !== \App\Enum\Weather::BLIZZARD
            || $this === self::QUICK_PASS
            || $this === self::SHORT_PASS;
    }

    /**
     * Pásma pravítka pro posun (|dx|, |dy|) od házejícího: řádek = |dy|, znak = |dx|.
     * Q = Quick, S = Short, L = Long, B = Long Bomb, tečka = pravítko nedosáhne.
     * Pravítko je tvarovaná šablona, ne poloměr: (13,0) je Long Bomb, (5,12) hodit nejde.
     * Převzato z C++ (`engine/include/bb/enums.h`, `passRangeFromOffset`); zdroj je tištěná
     * tabulka přepsaná v `evidence/pass_range_grid_20260810.txt`. Shodu všech polí s tím
     * souborem hlídá `tests/Enum/PassRangeTest.php`.
     */
    private const RULER = [
        'QQQQSSSLLLLBBB', // |dy| = 0
        'QQQQSSSLLLLBBB', // |dy| = 1
        'QQQSSSSLLLLBBB', // |dy| = 2
        'QQSSSSSLLLBBB.', // |dy| = 3
        'SSSSSSLLLLBBB.', // |dy| = 4
        'SSSSSLLLLBBB..', // |dy| = 5
        'SSSSLLLLLBBB..', // |dy| = 6
        'LLLLLLLLBBB...', // |dy| = 7
        'LLLLLLLBBB....', // |dy| = 8
        'LLLLLBBBB.....', // |dy| = 9
        'LLLBBBBB......', // |dy| = 10
        'BBBBBBB.......', // |dy| = 11
        'BBBBB.........', // |dy| = 12
        'BBB...........', // |dy| = 13
    ];

    /**
     * Pásmo hodu podle pravítka pro posun cíle od házejícího; `null` = mimo dosah.
     * (Bere posun, ne dvě `Position` -- vrstva `Enum` smí záviset jen na sobě.)
     *
     * OPRAVENO 08.10.2026 (audit parity, nález 12) -- tady bylo `fromDistance()`: pásma podle
     *   Čebyševovy vzdálenosti max(|dx|,|dy|) s prahy 3/6/10/13. Šikmé hody tím byly lehčí
     *   a delší: (3,3) Quick místo Short, (6,6) Short místo Long, (10,10) i (13,13) šly
     *   hodit. Pravidla ř. 765: "the coach must measure the range using the range ruler."
     */
    public static function fromOffset(int $dx, int $dy): ?self
    {
        $dx = abs($dx);
        $dy = abs($dy);
        if ($dx > 13 || $dy > 13) {
            return null;
        }

        return match (self::RULER[$dy][$dx]) {
            'Q' => self::QUICK_PASS,
            'S' => self::SHORT_PASS,
            'L' => self::LONG_PASS,
            'B' => self::LONG_BOMB,
            default => null,
        };
    }

    public function reduced(): self
    {
        return match ($this) {
            self::LONG_BOMB => self::LONG_PASS,
            self::LONG_PASS => self::SHORT_PASS,
            self::SHORT_PASS => self::QUICK_PASS,
            self::QUICK_PASS => self::QUICK_PASS,
        };
    }
}
