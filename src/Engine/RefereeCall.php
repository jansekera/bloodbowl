<?php

declare(strict_types=1);

namespace App\Engine;

use App\DTO\GameEvent;
use App\DTO\GameState;
use App\Enum\TeamSide;

/**
 * P287 (audit goblinů 10.11.2026): rozhodčí vylučuje hráče — za faul (ř. 1878-1884) nebo tajnou zbraň na konci drive.
 * Parita s C++ helpers.cpp `refereeSendOff`.
 *
 * - Úplatek (ř. 2871-2879): D6 2-6 = výrok neplatí (u faulu i bez turnoveru), 1 = úplatek propadl; každý jednou.
 * - Argue the call (ř. 1091-1102): D6 6 = hráč jen do rezerv (u faulu turnover zůstává), 1 = trenér vyhozen
 *   (dál neprotestuje).
 * - Pořadí (pravidla dovolují obojí): faul napřed úplatek (ruší i turnover), tajná zbraň napřed protest (úspěch =
 *   do rezerv a úplatek zůstane). Engine volí za oba týmy; dialog pro kouče-člověka je P289.
 */
final class RefereeCall
{
    public const CANCELLED = 'cancelled';
    public const RESERVES = 'reserves';
    public const SENT_OFF = 'sent_off';

    /**
     * @param list<GameEvent> $events
     * @return array{state: GameState, outcome: string}
     */
    public static function sendOff(
        GameState $state,
        int $playerId,
        TeamSide $side,
        bool $foul,
        DiceRollerInterface $dice,
        array &$events,
    ): array {
        $bribe = static function () use (&$state, $side, $playerId, $dice, &$events): bool {
            $team = $state->getTeamState($side);
            if ($team->getBribes() <= 0) {
                return false;
            }
            $state = $state->withTeamState($side, $team->withBribes($team->getBribes() - 1));
            $roll = $dice->rollD6();
            $events[] = GameEvent::bribe($playerId, $roll, $roll >= 2);

            return $roll >= 2;
        };
        $argue = static function () use (&$state, $side, $playerId, $dice, &$events): bool {
            $team = $state->getTeamState($side);
            if ($team->isHeadCoachEjected()) {
                return false;
            }
            $roll = $dice->rollD6();
            $events[] = GameEvent::argueTheCall($playerId, $roll);
            if ($roll === 1) {
                $state = $state->withTeamState($side, $team->withHeadCoachEjected());
            }

            return $roll === 6;
        };

        if ($foul) {
            $outcome = $bribe() ? self::CANCELLED : ($argue() ? self::RESERVES : self::SENT_OFF);
        } else {
            $outcome = $argue() ? self::RESERVES : ($bribe() ? self::CANCELLED : self::SENT_OFF);
        }

        return ['state' => $state, 'outcome' => $outcome];
    }
}
