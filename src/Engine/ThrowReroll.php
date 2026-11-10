<?php

declare(strict_types=1);

namespace App\Engine;

use App\DTO\GameEvent;
use App\DTO\GameState;
use App\DTO\MatchPlayerDTO;
use App\DTO\PendingRerollDTO;
use App\Enum\ActionType;
use App\ValueObject\Position;
use App\Enum\SkillName;
use App\Enum\TeamSide;

/**
 * Jediné místo žebříčku přehozu „Pro -> týmový přehoz s Lonerem" bez dialogu: hody, které nejdou přes Pass skill
 * (přihrávka, bomba, Always Hungry u Throw Team-Mate) a přehozy AI u Leapu (MoveHandler::prehozAi). Parita s C++ `rerollThrow` (helpers.cpp, P258).
 *
 * Jeden hod = nejvýš jeden přehoz (r. 925-927): má-li hráč Pro a ještě ho nepoužil, jde Pro (po neúspěšném hodu
 * Pro smí týmový přehoz jen hod Pro, ProCheck); jinak týmový přehoz (r. 929-936, Loner 4+ r. 8287-8290).
 * Volající napřed vyřídí Pass skill (zdarma) a po `rerolled` sám hodí kostku znovu a zapíše svou událost
 * (passAttempt / bombThrow / alwaysHungry).
 */
final class ThrowReroll
{
    /**
     * P289: nabídka přehozu kouči-člověku (dialog uprostřed akce); null = není co nabídnout. Jediné místo stavby
     * nabídky pro Leap (MoveHandler) i velké hráče (ActionResolver).
     *
     * @param array<string, mixed> $resumeParams
     */
    public static function offer(
        GameState $state,
        MatchPlayerDTO $player,
        string $rollType,
        int $target,
        int $roll,
        ?Position $at,
        bool $teamAllowed = true,
        int $gfiRemaining = 0,
        ?ActionType $resumeAction = null,
        array $resumeParams = [],
    ): ?PendingRerollDTO {
        $smi = self::available($state, $player, $teamAllowed);
        if (!$smi['pro'] && !$smi['team']) {
            return null;
        }

        return new PendingRerollDTO(
            $rollType,
            $player->getId(),
            $target,
            $roll,
            $smi['pro'],
            $smi['team'],
            $at?->getX() ?? 0,
            $at?->getY() ?? 0,
            resumeAction: $resumeAction,
            resumeParams: $resumeParams,
            gfiRemaining: $gfiRemaining,
        );
    }

    /**
     * Co hráč smí na nepovedený hod použít: Pro (jednou za kolo, ř. 8381) a týmový přehoz (jednou za kolo, ř. 929-931).
     * Jediné místo i pro nabídky kouči-člověku (P289: MoveHandler, ActionResolver).
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
        ?CoachRerolls $kouc = null,
        string $druh = '',
        int $hod = 0,
        int $cil = 0,
    ): array {
        // P292: hráče kouče-člověka se zeptá (druh / hod / cíl jen pro dialog)
        if ($kouc !== null && $kouc->decides($side)) {
            return self::sKoucem($dice, $state, $thrower, $side, $teamAllowed, $events, $kouc, $druh, $hod, $cil);
        }
        $teamAvailable = $teamAllowed && $state->getTeamState($side)->canUseReroll();

        if ($thrower->hasSkill(SkillName::Pro) && !$thrower->isProUsedThisTurn()) {
            $thrower = $thrower->withProUsedThisTurn(true);
            $state = $state->withPlayer($thrower);
            $pro = ProCheck::roll($dice, $thrower, $teamAvailable, $events);
            if ($pro['teamRerollUsed']) {
                $state = $state->withTeamState($side, $state->getTeamState($side)->withRerollUsed());
            }

            return ['state' => $state, 'thrower' => $thrower, 'rerolled' => $pro['allowed']];
        }

        if (!$teamAvailable) {
            return ['state' => $state, 'thrower' => $thrower, 'rerolled' => false];
        }
        $tym = self::teamReroll($dice, $state, $thrower, $side, $events);

        return ['state' => $tym['state'], 'thrower' => $thrower, 'rerolled' => $tym['ok']];
    }

    /**
     * Spotřebuje týmový přehoz; s Lonerem napřed 4+ (ř. 8287-8290: na 1-3 přehoz propadne).
     *
     * @param list<GameEvent> $events
     * @return array{state: GameState, ok: bool}
     */
    private static function teamReroll(DiceRollerInterface $dice, GameState $state, MatchPlayerDTO $thrower, TeamSide $side, array &$events): array
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
     * P292: totéž jako výše, ale o Pro / týmovém přehozu rozhoduje kouč. Hody v témže pořadí jako u AI: hod Pro, po
     * jeho neúspěchu druhá otázka (týmový přehoz hodu Pro, ř. 8385-8387), Loner, nový hod Pro.
     *
     * @param list<GameEvent> $events
     * @return array{state: GameState, thrower: MatchPlayerDTO, rerolled: bool}
     */
    private static function sKoucem(
        DiceRollerInterface $dice,
        GameState $state,
        MatchPlayerDTO $thrower,
        TeamSide $side,
        bool $teamAllowed,
        array &$events,
        CoachRerolls $kouc,
        string $druh,
        int $hod,
        int $cil,
    ): array {
        $smi = self::available($state, $thrower, $teamAllowed);
        if (!$smi['pro'] && !$smi['team']) {
            return ['state' => $state, 'thrower' => $thrower, 'rerolled' => false];
        }
        $volba = $kouc->ask($thrower->getId(), $druh, $hod, $cil, $smi['pro'], $smi['team']);
        if ($volba === 'decline') {
            return ['state' => $state, 'thrower' => $thrower, 'rerolled' => false];
        }
        if ($volba === 'pro') {
            $thrower = $thrower->withProUsedThisTurn(true);
            $state = $state->withPlayer($thrower);
            if (ProCheck::roll($dice, $thrower, false, $events)['allowed']) {
                return ['state' => $state, 'thrower' => $thrower, 'rerolled' => true];
            }
            if (!$smi['team'] || $kouc->ask($thrower->getId(), $druh, $hod, $cil, false, true, true) === 'decline') {
                return ['state' => $state, 'thrower' => $thrower, 'rerolled' => false];
            }
        }
        $tym = self::teamReroll($dice, $state, $thrower, $side, $events);
        $state = $tym['state'];
        if (!$tym['ok']) {
            return ['state' => $state, 'thrower' => $thrower, 'rerolled' => false];
        }
        // po neúspěšném Pro přehazuje týmový přehoz HOD PRO (ř. 8387), ne původní kostku
        $rerolled = $volba === 'pro' ? ProCheck::roll($dice, $thrower, false, $events)['allowed'] : true;

        return ['state' => $state, 'thrower' => $thrower, 'rerolled' => $rerolled];
    }
}
