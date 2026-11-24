<?php

declare(strict_types=1);

namespace App\Engine;

use App\DTO\GameState;
use App\DTO\MatchPlayerDTO;
use App\Enum\PlayerState;
use App\Enum\SkillName;

/**
 * P310 (uživatel 24.11.2026): Piling On, `rules_bb2016.txt` ř. 8361-8371: "The player may use this skill after they
 * have made a block as part a Block or Blitz action, but only if they are currently standing adjacent to the victim
 * and the victim was Knocked Down. You can use a team re-roll to re-roll the Armour roll or Injury roll. Then the
 * Piling On player is Placed Prone in their own square - no Armour roll is made for them, and this does not cause a
 * turnover unless the Piling On player is carrying the ball. ... If a player with the Loner skill wishes to use
 * Piling On, roll a D6. On a 1-3, the team re-roll is spent, but they remain standing and cannot re-roll the Armour
 * or Injury roll." Kdy smí a pravidlo AI na jednom místě; parita s C++ `piling_on.cpp`.
 */
final class PilingOn
{
    /** Smí útočník Piling On teď použít: skill, stojí, sousedí se sraženým, tým má v tahu přehoz. */
    public static function smi(GameState $state, MatchPlayerDTO $attacker, MatchPlayerDTO $defender): bool
    {
        $a = $attacker->getPosition();
        $d = $defender->getPosition();

        return $attacker->hasSkill(SkillName::PilingOn) && $attacker->getState() === PlayerState::STANDING
            && $a !== null && $d !== null && $a->distanceTo($d) === 1
            && $state->getTeamState($attacker->getTeamSide())->canUseReroll();
    }

    /**
     * Rozhodnutí AI (uživatel 24.11.: „1 vše, 2 ano“ + „piling on dává našeho hráče na zem - cíl pro faul“): ne s míčem
     * (turnover), ne v Multiple Block; šetří přehoz, dokud tým hraje s míčem (jen když míč drží soupeř, nebo náš nosič
     * už v tahu dohrál); a jen když zisk (soupeř po přehozu vyřazen) > ztráta (náš ležící útočník vyřazen faulem).
     *
     * @param bool $brneni přehazuje se brnění (vydrželo), jinak zranění (Stunned) s modifikátorem `$injuryMod`
     */
    public static function aiPouzit(
        GameState $state,
        MatchPlayerDTO $attacker,
        MatchPlayerDTO $defender,
        bool $brneni,
        int $armourMod,
        bool $mightyBlow,
        bool $claw,
        int $injuryMod,
        bool $multipleBlock,
    ): bool {
        $nosicId = $state->getBall()->getCarrierId();
        if ($multipleBlock || $nosicId === $attacker->getId()) {
            return false;
        }
        $nosic = $nosicId !== null && $state->getBall()->isHeld() ? $state->getPlayer($nosicId) : null;
        if ($nosic === null || ($nosic->getTeamSide() === $attacker->getTeamSide() && !$nosic->hasActed())) {
            return false;   // otázka 2: přehoz se šetří, dokud tým hraje s míčem
        }
        $zisk = $brneni ? self::brneniPakVen($defender, $armourMod, $mightyBlow, $claw) : self::zraneniVen($defender, $injuryMod);

        return $zisk > self::faulVen($state, $attacker, $defender->getId());
    }

    /** Šance, že zranění 2k6 + `$mod` vyřadí (KO a horší); Stunty 7 = KO (ř. 8534-8536), Thick Skull 8 = Stunned. */
    public static function zraneniVen(MatchPlayerDTO $obet, int $mod): float
    {
        $stunty = $obet->hasSkill(SkillName::Stunty);
        $thickSkull = $obet->hasSkill(SkillName::ThickSkull);
        $ven = 0;
        for ($s = 2; $s <= 12; $s++) {
            $m = $s + $mod;
            if (!($m <= ($stunty ? 6 : 7) || ($thickSkull && $m === 8))) {
                $ven += self::zpusobu($s);
            }
        }

        return $ven / 36;
    }

    /** Šance, že brnění 2k6 + `$mod` padne (Mighty Blow jako `InjuryResolver::resolve`) a zranění vyřadí. */
    public static function brneniPakVen(MatchPlayerDTO $obet, int $mod, bool $mightyBlow, bool $claw): float
    {
        $av = $obet->getStats()->getArmour();
        $padne = static fn(int $hod): bool => $hod > $av || ($claw && $hod >= 8);
        $p = 0.0;
        for ($s = 2; $s <= 12; $s++) {
            if ($padne($s + $mod)) {
                $p += self::zpusobu($s) / 36 * self::zraneniVen($obet, $mightyBlow ? 1 : 0);
            } elseif ($mightyBlow && $padne($s + $mod + 1)) {
                $p += self::zpusobu($s) / 36 * self::zraneniVen($obet, 0);
            }
        }

        return $p;
    }

    /** Šance, že faul na ležícího ho vyřadí: asistují všichni stojící soupeři vedle, Dirty Player na hřišti +1. */
    public static function faulVen(GameState $state, MatchPlayerDTO $hrac, int $bezId): float
    {
        $pole = $hrac->getPosition();
        $asistence = 0;
        $dirty = false;
        foreach ($state->getPlayersOnPitch($hrac->getTeamSide()->opponent()) as $s) {
            if ($s->getState() !== PlayerState::STANDING || $s->getId() === $bezId) {
                continue;
            }
            $dirty = $dirty || $s->hasSkill(SkillName::DirtyPlayer);
            if ($pole !== null && $s->getPosition()?->distanceTo($pole) === 1) {
                $asistence++;
            }
        }

        return self::brneniPakVen($hrac, $asistence + ($dirty ? 1 : 0), false, false);
    }

    /** Počet kombinací 2k6 se součtem `$s` z 36. */
    private static function zpusobu(int $s): int
    {
        return $s < 2 || $s > 12 ? 0 : 6 - abs($s - 7);
    }
}
