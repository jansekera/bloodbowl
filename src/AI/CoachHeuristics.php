<?php
declare(strict_types=1);

namespace App\AI;

use App\DTO\GameState;
use App\DTO\MatchPlayerDTO;
use App\Engine\StrengthCalculator;
use App\Engine\TacklezoneCalculator;
use App\Enum\ActionType;
use App\Enum\PassRange;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use App\ValueObject\Position;

/**
 * ⭐⭐ PHP39: SPOLECNA ZAKLADNA OBOU KOUCU (13.09.2026).
 *
 * ⛔ DOLOZENY DUVOD: `LearningAICoach` a `GreedyAICoach` mely tataz pravidla
 *   napsana DVAKRAT -- riziko podle `successChance`, klec jen na diagonaly,
 *   nosic nesmi vedle soupere, nosic nebojuje, ocenení bloku kostkami,
 *   `endZoneX` (7x v codebase) a `match` na dosah prihravky. Komentare
 *   v Greedym to samy priznavaly ("tataz oprava jako v LearningAICoach").
 *
 * ⛔ A UZ TO JEDNOU SELHALO: oprava klicu dosahu prihravky ('short' proti
 *   'short_pass') probehla 13.09. rano jen v Learningu a v Greedym zustaly
 *   rozbite az do odpoledne. Proto se tady dosah cte pres enum `PassRange`:
 *   preklep v retezci uz neni kam napsat.
 *
 * ⭐ A HLAVNE: kouc si nepocita to, co engine UZ UMI.
 *   `TacklezoneCalculator::countTacklezones()` (vcetne `$exceptPlayerId`),
 *   `getMarkingPlayers()`, `StrengthCalculator::countAssists()`,
 *   `GameState::getPlayerAtPosition()`, `Position::distanceTo()`.
 *
 * ⚠️ CO SEM NEPATRI: ciselne hodnoty bonusu a pokut. Ty nesou herni
 *   rozhodnuti uzivatele a u kazdeho kouce jsou v jine stupnici
 *   (Learning 0-10, Greedy 0-1000), takze zustavaji v koucich.
 */
final class CoachHeuristics
{
    /**
     * ⭐⭐⭐ PHP38, uzivatel 13.09.: "vsechny akce si musi hlidat, at maji
     *   turnover sanci pod 50 % -- nebo akci provest jen v nouzi."
     *   ⇒ "Pod 50 %" znamena `pT < 0,5`; rovnych 50 % uz pod prahem NENI.
     */
    public const NOUZE_PRAH = 0.5;

    /**
     * ⛔ Pravdepodobnost, ze JEDNA blokova kostka skonci turnoverem.
     *   `BlockHandler::rollBlockDie()`: 1 = Attacker Down, 2 = Both Down,
     *   3-6 = neco ve prospech utocnika. Attacker Down srazi utocnika vzdy,
     *   Both Down jen tehdy, kdyz utocnik NEMA Block
     *   (`BlockHandler.php:912` -- `if (!$attacker->hasSkill(Block))`).
     */
    private const KOSTKA_TURNOVER = 2 / 6;
    private const KOSTKA_TURNOVER_S_BLOCKEM = 1 / 6;

    /**
     * ⛔ Vyhozeni za faul: `FoulHandler.php:87` -- dvojka na obou kostkach,
     *   tedy 6 z 36. Sneaky Git vyhozeni obchazi uplne.
     */
    private const FAUL_VYHOZENI = 6 / 36;

    private static ?TacklezoneCalculator $tz = null;

    private static function tz(): TacklezoneCalculator
    {
        return self::$tz ??= new TacklezoneCalculator();
    }

    /** Souperova koncova zona -- x, na ktere se dava touchdown. */
    public static function endZoneX(TeamSide $side): int
    {
        return $side === TeamSide::HOME ? 25 : 0;
    }

    /** Kolik poli zbyva do souperovy koncove zony. */
    public static function doKoncoveZony(TeamSide $side, int $x): int
    {
        return abs($x - self::endZoneX($side));
    }

    /**
     * ⭐ KLEC = NOSIC + CTYRI DIAGONALNI ROHY (zadani uzivatele 12.09.).
     *   Ortogonalni soused je k nicemu: souper na nosice dosahne stejne
     *   a jeste si to pole sam zabere.
     */
    public static function jeRohKlece(Position $nosic, Position $pos): bool
    {
        return abs($nosic->getX() - $pos->getX()) === 1
            && abs($nosic->getY() - $pos->getY()) === 1;
    }

    /** Nosic mice z vlastniho tymu, nebo `null`. */
    public static function nosic(GameState $state, TeamSide $side): ?MatchPlayerDTO
    {
        $ball = $state->getBall();
        if (!$ball->isHeld() || $ball->getCarrierId() === null) {
            return null;
        }
        $nosic = $state->getPlayer($ball->getCarrierId());

        return ($nosic !== null && $nosic->getTeamSide() === $side) ? $nosic : null;
    }

    /** Pozice vlastniho nosice, nebo `null`. */
    public static function pozicNosice(GameState $state, TeamSide $side): ?Position
    {
        return self::nosic($state, $side)?->getPosition();
    }

    /** Drzi tenhle hrac mic? */
    public static function jeNosic(GameState $state, int $playerId): bool
    {
        $ball = $state->getBall();

        return $ball->isHeld() && $ball->getCarrierId() === $playerId;
    }

    /**
     * ⛔⛔ NOSIC NEBOJUJE (uzivatel 12.09.: "nosic nesmi blitzovat ani jit
     *   vedle soupere -- na blitz mame mit lepsi kandidaty a asistenty").
     *   Blok, blitz, multiblok i faul ho postavi k souperi a riskuji jeho
     *   srazeni s micem.
     */
    public static function nosicNebojuje(ActionType $type): bool
    {
        return in_array($type, [
            ActionType::BLOCK,
            ActionType::BLITZ,
            ActionType::MULTIPLE_BLOCK,
            ActionType::FOUL,
        ], true);
    }

    /**
     * Kolik SOUPEROVYCH zon zachyceni je na danem poli.
     *
     * ⭐ PHP39: prevzato z `TacklezoneCalculator::countTacklezones()` misto
     *   vlastni smycky. Engine navic odecita hrace, kteri zonu ZTRATILI
     *   (`hasLostTacklezones()`, tedy napr. po Hypnotic Gaze) -- kouci si to
     *   ve svych kopiich nehlidali a stali se hypnotizovaneho soupere bali.
     */
    public static function zonyZachyceni(GameState $state, Position $pos, TeamSide $side, ?int $krome = null): int
    {
        return self::tz()->countTacklezones($state, $pos, $side, $krome);
    }

    /**
     * ⛔⛔ "Postavit nosice vedle soupere nesmime uz vubec" (uzivatel 12.09.).
     *   Odtud ho souper BLOKUJE, a blok je neomezeny.
     */
    public static function jeVedleSoupere(GameState $state, Position $pos, TeamSide $side, ?int $krome = null): bool
    {
        return self::zonyZachyceni($state, $pos, $side, $krome) > 0;
    }

    /**
     * Souperovi hraci, kteri dane pole drzi v zone zachyceni.
     *
     * @return list<MatchPlayerDTO>
     */
    public static function znackujici(GameState $state, Position $pos, TeamSide $side): array
    {
        return self::tz()->getMarkingPlayers($state, $pos, $side);
    }

    /**
     * ⭐ PHP38: pravdepodobnost turnoveru za CESTU na dane pole.
     *   `successChance` z `getValidMoveTargets` uz agilitu, dodge, GFI
     *   i zony zachyceni zohlednuje (AG 2 => 33, AG 3 => 50, AG 4 => 67).
     *   ⛔ Kdyz klic chybi, pocita se 100 % uspechu -- bezpecny vychozi stav.
     *
     * @param array<string, mixed> $target polozka z `getValidMoveTargets`
     */
    public static function pTurnoverCesty(array $target): float
    {
        $sance = max(0, min(100, (int) ($target['successChance'] ?? 100)));

        return 1 - $sance / 100;
    }

    /**
     * ⭐⭐⭐ PHP38: SKLADANI RIZIK. Turnover nastane, kdyz selze KTERAKOLI
     *   z casti akce, takze se pravdepodobnosti NESCITAJI, ale skladaji:
     *   `1 - Π(1-pᵢ)`.
     *
     * ⛔ DOLOZENY ROZPOR, ktery tim mizi: cesta k mici 60 % a zvednuti 60 %
     *   se do 13.09. odecitaly jako DVE NEZAVISLE POKUTY, prestoze skutecna
     *   sance turnoveru je `1 - 0,6*0,6 = 64 %` -- a to cislo nikde nevznikalo.
     */
    public static function pTurnoverCelkem(float ...$p): float
    {
        $prezije = 1.0;
        foreach ($p as $jedna) {
            $prezije *= 1 - max(0.0, min(1.0, $jedna));
        }

        return 1 - $prezije;
    }

    /** ⭐ PHP38: kandidat s timhle rizikem uz patri do vrstvy NOUZE. */
    public static function jeNouze(float $pT): bool
    {
        return $pT >= self::NOUZE_PRAH;
    }

    /**
     * Kostky bloku VCETNE ASISTENCI -- pres `StrengthCalculator`, ktery je
     * pocita uz pro engine.
     *
     * @param int $bonusObrance +2 u Multiple Blocku (`BlockHandler:468`)
     * @return array{count: int, attackerChooses: bool, signed: int}
     */
    public static function kostkyBloku(
        StrengthCalculator $str,
        GameState $state,
        MatchPlayerDTO $utocnik,
        MatchPlayerDTO $obrance,
        int $bonusObrance = 0,
    ): array {
        $up = $utocnik->getPosition();
        $op = $obrance->getPosition();
        if ($up === null || $op === null) {
            return ['count' => 1, 'attackerChooses' => true, 'signed' => 1];
        }

        $info = $str->getBlockDiceInfo(
            $str->calculateEffectiveStrength($state, $utocnik, $op),
            $str->calculateEffectiveStrength($state, $obrance, $up) + $bonusObrance,
        );

        // ⭐ Znamenkovy pocet kostek: kladne = vybira utocnik. Oba kouci si
        //   ho pocitali sami, Greedy dokonce na trech mistech.
        $info['signed'] = $info['attackerChooses'] ? $info['count'] : -$info['count'];

        return $info;
    }

    /**
     * ⭐⭐ PHP38: PRAVDEPODOBNOST TURNOVERU Z BLOKU -- misto zaporne pokuty.
     *
     * ⛔ DOLOZENY ROZPOR, ktery tim mizi: `BLOCK_DICE_VALUE` mela zapornou
     *   pulku (`2-` = -1,5, `3-` = -3,0) a komentar u ni sliboval vyjimku
     *   "bonus za nosice to muze prebit" -- jenze -3,0 neprebije +0,8 NIKDY.
     *   Zakaz byl zapsany VELIKOSTI CISLA a nedelal to, co tvrdil.
     *   ⇒ Ted je nebezpeci bloku PRAVDEPODOBNOST, ne konstanta: dve kostky
     *   proti vyjdou na 55,6 %, tri proti na 70,4 % -- oboje nad prahem
     *   NOUZE, takze se takovy blok zahraje jen tehdy, kdyz nic jineho neni.
     *
     * @param array{count: int, attackerChooses: bool} $kostky
     */
    public static function pTurnoverBloku(array $kostky, MatchPlayerDTO $utocnik): float
    {
        // ⭐ Block (a Wrestle) meni Both Down z turnoveru na pouhy pad --
        //   viz `BlockHandler.php:864-917`.
        $naKostku = ($utocnik->hasSkill(SkillName::Block) || $utocnik->hasSkill(SkillName::Wrestle))
            ? self::KOSTKA_TURNOVER_S_BLOCKEM
            : self::KOSTKA_TURNOVER;

        $pocet = max(1, $kostky['count']);

        // Utocnik si vybira => turnover jen kdyz jsou SPATNE VSECHNY kostky.
        if ($kostky['attackerChooses']) {
            return $naKostku ** $pocet;
        }

        // Souper vybira => staci, aby byla spatna JEDNA.
        return 1 - (1 - $naKostku) ** $pocet;
    }

    /**
     * ⛔ FAUL NENI TURNOVER -- `FoulHandler.php:110` to rika doslova:
     *   "Foul is NEVER a turnover (even with ejection)". Riziko faulu je tedy
     *   ZTRATA HRACE, ne ztrata tahu, a nepatri do turnoverove brany.
     *   Vraci se pravdepodobnost VYHOZENI (dvojka, 6 z 36; Sneaky Git nula).
     */
    public static function pVyhozeniZaFaul(MatchPlayerDTO $faulujici): float
    {
        return $faulujici->hasSkill(SkillName::SneakyGit) ? 0.0 : self::FAUL_VYHOZENI;
    }

    /**
     * ⛔ DOSAH PRIHRAVKY PRES ENUM, NE PRES RETEZEC (PHP39).
     *   Engine vraci 'quick_pass'/'short_pass'/'long_pass'/'long_bomb';
     *   oba kouci mely v `match` klice 'quick'/'short'/'long'/'bomb', takze
     *   vetev NIKDY nesedla a vsechny dosahy mely tutez cenu.
     *   ⇒ `PassRange::tryFrom()` na preklep spadne do `null`, a ten uz kazdy
     *   kouc resi vlastni vychozi vetvi.
     */
    public static function dosahPrihravky(mixed $range): ?PassRange
    {
        return is_string($range) ? PassRange::tryFrom($range) : null;
    }
}
