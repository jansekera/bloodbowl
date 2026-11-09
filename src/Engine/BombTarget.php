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
 * P264). Parita s C++ helpers.cpp bombSquareValue / chooseBombTarget (review dávky 9.11., M2, M4, L6, L10).
 */
final class BombTarget
{
    /**
     * Bombu smí chytit / zachytit stojící hráč se zónou a s rukama (r. 7959-7961, jako míč).
     */
    public static function mozeChytit(MatchPlayerDTO $p): bool
    {
        return $p->getState() === PlayerState::STANDING && !$p->hasLostTacklezones() && !$p->hasSkill(SkillName::NoHands);
    }

    /**
     * Cena výbuchu na poli `$c` pro stranu `$strana`: cílové pole srazí vždy, sousední na 4+ (r. 7968-7972); stojící
     * soupeř +1 (ležící 0,3, jen zranění), náš −2 (turnover) × šance zásahu. Soupeř v cílovém poli, který bombu chytit
     * může, jen × šance, že přesnou bombu nechytí (+1, Catch přehodí; review M2).
     *
     * @return array{0: float, 1: bool} cena, zasáhne našeho
     */
    public static function cena(GameState $state, Position $c, TeamSide $strana, BallResolver $ball): array
    {
        $v = 0.0;
        $nas = false;
        for ($dx = -1; $dx <= 1; $dx++) {
            for ($dy = -1; $dy <= 1; $dy++) {
                $q = new Position($c->getX() + $dx, $c->getY() + $dy);
                if (!$q->isOnPitch()) {
                    continue;
                }
                $p = $state->getPlayerAtPosition($q);
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
                    $nechyti = ($ball->getCatchTarget($state, $p, 1, true) - 1) / 6.0;
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
    public static function vyber(GameState $state, MatchPlayerDTO $hazec, bool $musi, BallResolver $ball): ?array
    {
        $odkud = $hazec->getPosition();
        if ($odkud === null) {
            return null;
        }
        $strana = $hazec->getTeamSide();
        $nej = [null, null];   // [0] pravítko, [1] Hail Mary
        $nejCena = [$musi ? -1e9 : 0.999, 0.999];
        $nejDal = -1;
        for ($x = 0; $x < Position::PITCH_WIDTH; $x++) {
            for ($y = 0; $y < Position::PITCH_HEIGHT; $y++) {
                $c = new Position($x, $y);
                if ($c->equals($odkud)) {
                    continue;
                }
                $posudek = ThrowTarget::posud($state, $hazec, $odkud, $c);
                if ($posudek['chyba'] !== null) {
                    continue;
                }
                [$v, $nas] = self::cena($state, $c, $strana, $ball);
                if ($musi) {
                    $dal = 99;
                    foreach ($state->getPlayersOnPitch($strana) as $p) {
                        $pp = $p->getPosition();
                        if ($pp !== null) {
                            $dal = min($dal, $pp->distanceTo($c));
                        }
                    }
                    if ($v > $nejCena[0] || ($v === $nejCena[0] && $dal > $nejDal)) {
                        $nejCena[0] = $v;
                        $nejDal = $dal;
                        $nej[0] = ['pos' => $c, 'cena' => $v, 'hailMary' => $posudek['hailMary']];
                    }
                    continue;
                }
                if ($nas || ($posudek['hailMary'] && $state->maHraceDo($strana, $c, 4, false))) {
                    continue;
                }
                $k = $posudek['hailMary'] ? 1 : 0;
                if ($v > $nejCena[$k]) {
                    $nejCena[$k] = $v;
                    $nej[$k] = ['pos' => $c, 'cena' => $v, 'hailMary' => $posudek['hailMary']];
                }
            }
        }
        return $nej[0] ?? $nej[1];
    }
}
