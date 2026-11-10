<?php

declare(strict_types=1);

namespace App\Engine;

use App\DTO\GameState;
use App\DTO\MatchPlayerDTO;
use App\Enum\PassRange;
use App\Enum\SkillName;
use App\Enum\Weather;
use App\ValueObject\Position;

/**
 * Smí se hodit na tento cíl? Jedno místo pro přihrávku i bombu (handlery, validátor, seznam cílů);
 * parita s C++ `helpers.cpp passTargetLegal`.
 *
 * - pásmo pravítka; ve vánici jen rychlá a krátká (`rules_bb2016.txt` r. 1490-1494);
 * - mimo pravítko jen s Hail Mary Pass (r. 8164-8169) a ne ve vánici (r. 8170-8171);
 *   bomba se hází stejně (r. 7952-7954, „including … use of Hail Mary Pass“).
 */
final class ThrowTarget
{
    /**
     * @return array{range: ?PassRange, hailMary: bool, chyba: ?string}
     */
    public static function posud(GameState $state, MatchPlayerDTO $thrower, Position $from, Position $target): array
    {
        $range = PassRange::fromOffset($target->getX() - $from->getX(), $target->getY() - $from->getY());
        $vanice = $state->getWeather() === Weather::BLIZZARD;
        if ($range === null) {
            $hailMary = $thrower->hasSkill(SkillName::HailMaryPass) && !$vanice;
            return ['range' => null, 'hailMary' => $hailMary, 'chyba' => $hailMary ? null : 'Target is out of range'];
        }
        if (!$range->povolenaZaPocasi($state->getWeather())) {
            return ['range' => $range, 'hailMary' => false, 'chyba' => 'Blizzard: only quick or short passes can be attempted'];
        }
        return ['range' => $range, 'hailMary' => false, 'chyba' => null];
    }

    /**
     * Všechna legální pole hodu míčem / bombou z pole hráče (bez vlastního pole). Jedno místo pro cíle přihrávky
     * (RulesEngine::getPassTargets) i nabídku kouči-člověku, kam hodit chycenou bombu (P272, BombThrowHandler).
     *
     * @return list<array{x: int, y: int, range: string}> range = pásmo pravítka, nebo 'hail_mary'
     */
    public static function cile(GameState $state, MatchPlayerDTO $hazec, bool $sHailMary): array
    {
        $odkud = $hazec->getPosition();
        if ($odkud === null) {
            return [];
        }
        $cile = [];
        for ($x = 0; $x < Position::PITCH_WIDTH; $x++) {
            for ($y = 0; $y < Position::PITCH_HEIGHT; $y++) {
                $cil = new Position($x, $y);
                if ($odkud->equals($cil)) {
                    continue;
                }
                ['range' => $range, 'hailMary' => $hailMary, 'chyba' => $chyba] = self::posud($state, $hazec, $odkud, $cil);
                if ($chyba !== null || ($hailMary && !$sHailMary)) {
                    continue;
                }
                $cile[] = ['x' => $x, 'y' => $y, 'range' => $range === null ? 'hail_mary' : $range->value];
            }
        }
        return $cile;
    }

    /**
     * Hod spoluhráčem jen rychlou/krátkou přihrávkou (r. 8608-8609: „Long Pass or Long Bomb range passes are not
     * possible“). Jedno místo pro validátor, handler i kouče AI (simplify 9.11.); C++ helpers.cpp ttmTargetLegal.
     * Vrací pásmo, nebo null, když hod nejde.
     */
    public static function ttm(Position $from, Position $to): ?PassRange
    {
        $range = PassRange::fromOffset($to->getX() - $from->getX(), $to->getY() - $from->getY());
        return $range === PassRange::QUICK_PASS || $range === PassRange::SHORT_PASS ? $range : null;
    }
}
