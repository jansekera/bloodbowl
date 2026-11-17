<?php

declare(strict_types=1);

namespace App\Engine;

use App\DTO\GameEvent;
use App\DTO\GameState;
use App\DTO\MatchPlayerDTO;
use App\ValueObject\Position;
use App\Enum\SkillName;
use App\Enum\TeamSide;

/**
 * Jediné místo žebříčku přehozu „Pro -> týmový přehoz s Lonerem" pro všechny přehazovatelné hody (P294): přihrávka,
 * bomba, hod spoluhráčem a Always Hungry (s Pass skillem zdarma: `reroll(…, SkillName::Pass)`), pohyb
 * (`MoveHandler::prehoz`), zvednutí a chytání (`BallResolver`), kontroly velkých hráčů. O každém kroku rozhoduje AI,
 * nebo kouč-člověk otázkou přehráním (`Coach`). Parita s C++ `rerollThrow` (helpers.cpp, P258).
 *
 * Jeden hod = nejvýš jeden přehoz (r. 925-927): má-li hráč Pro a ještě ho nepoužil, jde Pro (po neúspěšném hodu
 * Pro smí týmový přehoz jen hod Pro, ProCheck); jinak týmový přehoz (r. 929-936, Loner 4+ r. 8287-8290).
 * Skill zdarma (Pass) vyřídí `reroll` sám — volající ho NEřeší. Po `rerolled` volající hodí kostku znovu a zapíše
 * svou událost (hod se liší).
 */
final class ThrowReroll
{
    /**
     * Co hráč smí na nepovedený hod použít: Pro (jednou za kolo, ř. 8381) a týmový přehoz (jednou za kolo, ř. 929-931).
     *
     * @return array{pro: bool, team: bool}
     */
    public static function available(GameState $state, MatchPlayerDTO $player, bool $teamAllowed = true): array
    {
        return [
            'pro' => $player->hasSkill(SkillName::Pro) && !$player->isProUsedThisTurn(),
            'team' => $teamAllowed && $state->getTeamState($player->getTeamSide())->canUseReroll(),
        ];
    }

    /**
     * @param list<GameEvent> $events doplní se události Pro / Loner / rerollUsed
     * @param bool $teamAllowed smí se vzít týmový přehoz (bomba s Lonerem ne)
     * @return array{state: GameState, thrower: MatchPlayerDTO, rerolled: bool} `rerolled` = hod se smí hodit znovu
     */
    public static function proThenTeam(
        DiceRollerInterface $dice,
        GameState $state,
        MatchPlayerDTO $thrower,
        TeamSide $side,
        bool $teamAllowed,
        array &$events,
        ?Coach $kouc = null,
        string $druh = '',
        int $hod = 0,
        int $cil = 0,
        ?Position $kam = null,
    ): array {
        // P292: hráče kouče-člověka se zeptá (druh / hod / cíl jen pro dialog). P294: AI je „kouč“, který vždy bere
        //   Pro (bez Pro týmový přehoz) a po neúspěšném hodu Pro týmový přehoz — jeden žebříček pro oba.
        $volba = ($kouc !== null && $kouc->decides($side))
            ? static function (bool $pro, bool $team, bool $proFailed, GameState $stav) use ($kouc, $thrower, $druh, $hod, $cil, $kam, &$events): string {
                // P294 krok 5a: otázka nese stav a dosavadní události (kouč vidí, co padlo); `$kam` = cílové pole hodu (5b).
                //   OPRAVENO 17.11. (review P294 nález 3): stav v okamžiku TÉTO otázky (po hodu Pro je Pro použité), dřív
                //   stav zachycený před žebříčkem
                return $kouc->reroll($thrower->getId(), $druh, $hod, $cil, $pro, $team, $proFailed, $stav, $events, $kam);
            }
        : static fn(bool $pro, bool $team, bool $proFailed, GameState $stav): string => ($pro && !$proFailed) ? 'pro' : 'team_reroll';

        return self::zebricek($dice, $state, $thrower, $side, $teamAllowed, $events, $volba);
    }

    /**
     * P294 krok 2: přehoz hodu s volitelným skillem zdarma (Pass u přihrávky, bomby a hodu hráčem, ř. 8336-8337) — jediné
     * místo vzoru „skill zdarma, jinak Pro → týmový přehoz“. Skill má přednost (zdarma, nespotřebuje Pro ani týmový
     * přehoz); kostku znovu hodí a událost zapíše volající.
     *
     * @param list<GameEvent> $events
     * @return array{state: GameState, thrower: MatchPlayerDTO, rerolled: bool}
     */
    public static function reroll(
        DiceRollerInterface $dice,
        GameState $state,
        MatchPlayerDTO $thrower,
        TeamSide $side,
        bool $teamAllowed,
        array &$events,
        ?Coach $kouc,
        string $druh,
        int $hod,
        int $cil,
        ?SkillName $skillZdarma = null,
    ): array {
        if ($skillZdarma !== null && $thrower->hasSkill($skillZdarma)) {
            $events[] = GameEvent::rerollUsed($thrower->getId(), $skillZdarma->value);

            return ['state' => $state, 'thrower' => $thrower, 'rerolled' => true];
        }

        return self::proThenTeam($dice, $state, $thrower, $side, $teamAllowed, $events, $kouc, $druh, $hod, $cil);
    }

    /**
     * Spotřebuje týmový přehoz; s Lonerem napřed 4+ (ř. 8287-8290: na 1-3 přehoz propadne).
     *
     * @param list<GameEvent> $events
     * @return array{state: GameState, ok: bool}
     */
    public static function teamReroll(DiceRollerInterface $dice, GameState $state, MatchPlayerDTO $thrower, TeamSide $side, array &$events): array
    {
        $state = $state->withTeamState($side, $state->getTeamState($side)->withRerollUsed());
        if ($thrower->hasSkill(SkillName::Loner)) {
            $lonerRoll = $dice->rollD6();
            $events[] = GameEvent::lonerCheck($thrower->getId(), $lonerRoll, $lonerRoll >= 4);
            if ($lonerRoll < 4) {
                return ['state' => $state, 'ok' => false];
            }
        }
        $events[] = GameEvent::rerollUsed($thrower->getId(), 'Team Reroll');

        return ['state' => $state, 'ok' => true];
    }

    /**
     * P294: jeden žebříček Pro → týmový přehoz → Loner; o každém kroku rozhoduje `$volba` (AI nebo kouč-člověk).
     * Pořadí hodů: hod Pro, po jeho neúspěchu druhá otázka (týmový přehoz hodu Pro, ř. 8385-8387), Loner, nový hod Pro.
     *
     * @param list<GameEvent> $events
     * @param \Closure(bool, bool, bool, GameState): string $volba (pro smí, týmový smí, Pro už selhal, stav teď) → 'pro'|'team_reroll'|'decline'
     * @return array{state: GameState, thrower: MatchPlayerDTO, rerolled: bool}
     */
    private static function zebricek(
        DiceRollerInterface $dice,
        GameState $state,
        MatchPlayerDTO $thrower,
        TeamSide $side,
        bool $teamAllowed,
        array &$events,
        \Closure $volba,
    ): array {
        $smi = self::available($state, $thrower, $teamAllowed);
        if (!$smi['pro'] && !$smi['team']) {
            return ['state' => $state, 'thrower' => $thrower, 'rerolled' => false];
        }
        $vybral = $volba($smi['pro'], $smi['team'], false, $state);
        if ($vybral === 'decline') {
            return ['state' => $state, 'thrower' => $thrower, 'rerolled' => false];
        }
        if ($vybral === 'pro') {
            $thrower = $thrower->withProUsedThisTurn(true);
            $state = $state->withPlayer($thrower);
            if (ProCheck::roll($dice, $thrower, false, $events)['allowed']) {
                return ['state' => $state, 'thrower' => $thrower, 'rerolled' => true];
            }
            if (!$smi['team'] || $volba(false, true, true, $state) === 'decline') {
                return ['state' => $state, 'thrower' => $thrower, 'rerolled' => false];
            }
        }
        $tym = self::teamReroll($dice, $state, $thrower, $side, $events);
        $state = $tym['state'];
        if (!$tym['ok']) {
            return ['state' => $state, 'thrower' => $thrower, 'rerolled' => false];
        }
        // po neúspěšném Pro přehazuje týmový přehoz HOD PRO (ř. 8387), ne původní kostku
        $rerolled = $vybral === 'pro' ? ProCheck::roll($dice, $thrower, false, $events)['allowed'] : true;

        return ['state' => $state, 'thrower' => $thrower, 'rerolled' => $rerolled];
    }
}
