<?php

declare(strict_types=1);

namespace App\Engine;

use App\DTO\GameState;
use App\DTO\MatchPlayerDTO;
use App\Enum\PlayerState;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use App\ValueObject\Position;

/**
 * Kam hodit bombu — jedno místo pro kouče AI (Greedy, Learning) i povinný hod chycené bomby (BombThrowHandler,
 * P264). Parita s C++ helpers.cpp bombSquareValue / chooseBombTarget / shouldInterceptBomb (review dávky 9.11.,
 * M2, M4, L6, L10).
 */
final class BombTarget
{
    private static ?TacklezoneCalculator $tz = null;

    /** Bombu smí chytit / zachytit stojící hráč se zónou a s rukama (r. 7959-7961, jako míč). */
    public static function mozeChytit(MatchPlayerDTO $p): bool
    {
        return $p->getState() === PlayerState::STANDING && !$p->hasLostTacklezones() && !$p->hasSkill(SkillName::NoHands);
    }

    /**
     * Zachytit bombu (volba kouče, r. 7959-7961) zkusí jen hráč, jehož týmu výbuch v cíli hrozí. Engine rozhoduje za
     * oba kouče (P272); tady je jediné místo té volby.
     */
    public static function zachytit(GameState $state, TeamSide $strana, Position $cil): bool
    {
        return $state->maHraceDo($strana, $cil, 1, false);
    }

    /**
     * Cena výbuchu na poli `$c` pro stranu `$strana`: cílové pole srazí vždy, sousední na 4+ (r. 7968-7972); stojící
     * soupeř +1 (ležící 0,3, jen zranění), náš −2 (turnover) × šance zásahu. Soupeř v cílovém poli, který bombu chytit
     * může, jen × šance, že přesnou bombu nechytí (+1, Catch přehodí; review M2).
     *
     * @param array<int, array<int, MatchPlayerDTO>>|null $obsazeni mapa [x][y] (vyber); null = dotaz na stav
     * @return array{0: float, 1: bool} cena, zasáhne našeho
     */
    public static function cena(GameState $state, Position $c, TeamSide $strana, ?array $obsazeni = null): array
    {
        $v = 0.0;
        $nas = false;
        for ($dx = -1; $dx <= 1; $dx++) {
            for ($dy = -1; $dy <= 1; $dy++) {
                $x = $c->getX() + $dx;
                $y = $c->getY() + $dy;
                if ($x < 0 || $y < 0 || $x >= Position::PITCH_WIDTH || $y >= Position::PITCH_HEIGHT) {
                    continue;
                }
                $p = $obsazeni !== null ? ($obsazeni[$x][$y] ?? null) : $state->getPlayerAtPosition(new Position($x, $y));
                if ($p === null) {
                    continue;
                }
                $stred = $dx === 0 && $dy === 0;
                $zasah = $stred ? 1.0 : 0.5;
                if ($p->getTeamSide() === $strana) {
                    $nas = true;
                    $v -= 2.0 * $zasah;
                    continue;
                }
                $w = $p->getState() === PlayerState::STANDING ? 1.0 : 0.3;
                if ($stred && self::mozeChytit($p)) {
                    self::$tz ??= new TacklezoneCalculator();
                    $nechyti = (BallResolver::cilChyceni(self::$tz, $state, $p, 1, true) - 1) / 6.0;
                    if ($p->hasSkill(SkillName::Catch)) {
                        $nechyti *= $nechyti;
                    }
                    $w *= $nechyti;
                }
                $v += $zasah * $w;
            }
        }
        return [$v, $nas];
    }

    /**
     * `$musi = false` (AI): nejlepší pole na pravítku s cenou ≥ 1 bez našeho v dosahu výbuchu; když žádné, Hail Mary
     *   (P263) se stejnou podmínkou a bez našeho do 4 polí od cíle (3 rozptyly + výbuch). Nic ⇒ null.
     * `$musi = true` (hod chycené bomby): nejvyšší cena ze všech legálních polí, při shodě nejdál od našich.
     *
     * @return array{pos: Position, cena: float, hailMary: bool}|null
     */
    public static function vyber(GameState $state, MatchPlayerDTO $hazec, bool $musi): ?array
    {
        $odkud = $hazec->getPosition();
        if ($odkud === null) {
            return null;
        }
        $strana = $hazec->getTeamSide();
        // mapa obsazení jednou (jinak 9 dotazů × ~390 polí, každý přes všechny hráče); při shodě platí první hráč
        $obsazeni = [];
        foreach ($state->getPlayers() as $p) {
            $pp = $p->getPosition();
            if ($pp !== null && !isset($obsazeni[$pp->getX()][$pp->getY()])) {   // jako getPlayerAtPosition
                $obsazeni[$pp->getX()][$pp->getY()] = $p;
            }
        }
        // AI: cena ≥ 1 jde jen na poli se soupeřem v dosahu výbuchu (prázdné pole 0, naši jen ubírají)
        $uSoupere = [];
        if (!$musi) {
            foreach ($obsazeni as $ox => $sloupec) {
                foreach ($sloupec as $oy => $e) {
                    if ($e->getTeamSide() === $strana) {
                        continue;
                    }
                    for ($dx = -1; $dx <= 1; $dx++) {
                        for ($dy = -1; $dy <= 1; $dy++) {
                            $uSoupere[$ox + $dx][$oy + $dy] = true;
                        }
                    }
                }
            }
        }
        $nej = [null, null];   // [0] pravítko, [1] Hail Mary
        $nejCena = [$musi ? -1e9 : 0.999, 0.999];
        $nejDal = -1;
        for ($x = 0; $x < Position::PITCH_WIDTH; $x++) {
            for ($y = 0; $y < Position::PITCH_HEIGHT; $y++) {
                if (!$musi && !isset($uSoupere[$x][$y])) {
                    continue;
                }
                $c = new Position($x, $y);
                if ($c->equals($odkud)) {
                    continue;
                }
                $posudek = ThrowTarget::posud($state, $hazec, $odkud, $c);
                if ($posudek['chyba'] !== null) {
                    continue;
                }
                [$v, $nas] = self::cena($state, $c, $strana, $obsazeni);
                if ($musi) {
                    if ($v < $nejCena[0]) {
                        continue;
                    }
                    $dal = 99;   // vzdálenost k našim jen při možné shodě
                    foreach ($state->getPlayersOnPitch($strana) as $p) {
                        $pp = $p->getPosition();
                        if ($pp !== null) {
                            $dal = min($dal, $pp->distanceTo($c));
                        }
                    }
                    if ($v > $nejCena[0] || $dal > $nejDal) {
                        $nejCena[0] = $v;
                        $nejDal = $dal;
                        $nej[0] = ['pos' => $c, 'cena' => $v, 'hailMary' => $posudek['hailMary']];
                    }
                    continue;
                }
                $k = $posudek['hailMary'] ? 1 : 0;
                if ($nas || $v <= $nejCena[$k] || ($posudek['hailMary'] && $state->maHraceDo($strana, $c, 4, false))) {
                    continue;
                }
                $nejCena[$k] = $v;
                $nej[$k] = ['pos' => $c, 'cena' => $v, 'hailMary' => $posudek['hailMary']];
            }
        }
        return $nej[0] ?? $nej[1];
    }
}
