<?php

declare(strict_types=1);

namespace App\Engine;

use App\DTO\GameEvent;
use App\DTO\GameState;
use App\DTO\MatchPlayerDTO;
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
    ): array {
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
        $state = $state->withTeamState($side, $state->getTeamState($side)->withRerollUsed());
        if ($thrower->hasSkill(SkillName::Loner)) {
            $lonerRoll = $dice->rollD6();
            $events[] = GameEvent::lonerCheck($thrower->getId(), $lonerRoll, $lonerRoll >= 4);
            if ($lonerRoll < 4) {
                return ['state' => $state, 'thrower' => $thrower, 'rerolled' => false];
            }
        }
        $events[] = GameEvent::rerollUsed($thrower->getId(), 'Team Reroll');

        return ['state' => $state, 'thrower' => $thrower, 'rerolled' => true];
    }
}
