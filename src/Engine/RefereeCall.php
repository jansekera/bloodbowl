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
 *   do rezerv a úplatek zůstane).
 * - P291 (10.11.2026): kouč-člověk volí sám (`$volba`: úplatek ano/ne, protest ano/ne; pořadí výše platí dál). Bez
 *   volby (AI) se použije, co jde.
 */
final class RefereeCall
{
    public const CANCELLED = 'cancelled';
    public const RESERVES = 'reserves';
    public const SENT_OFF = 'sent_off';

    /** Co tým může proti výroku udělat: úplatek v zásobě, trenér na lavičce (smí protestovat). */
    public static function canBribe(GameState $state, TeamSide $side): bool
    {
        return $state->getTeamState($side)->getBribes() > 0;
    }

    public static function canArgue(GameState $state, TeamSide $side): bool
    {
        return !$state->getTeamState($side)->isHeadCoachEjected();
    }

    /**
     * @param list<GameEvent> $events
     * @param array{bribe: bool, argue: bool}|null $volba volba kouče-člověka; null = AI (použije, co jde)
     * @return array{state: GameState, outcome: string}
     */
    public static function sendOff(
        GameState $state,
        int $playerId,
        TeamSide $side,
        bool $foul,
        DiceRollerInterface $dice,
        array &$events,
        ?array $volba = null,
    ): array {
        $bribe = static function () use (&$state, $side, $playerId, $dice, &$events, $volba): bool {
            $team = $state->getTeamState($side);
            if ($team->getBribes() <= 0 || ($volba !== null && !$volba['bribe'])) {
                return false;
            }
            $state = $state->withTeamState($side, $team->withBribes($team->getBribes() - 1));
            $roll = $dice->rollD6();
            $events[] = GameEvent::bribe($playerId, $roll, $roll >= 2);

            return $roll >= 2;
        };
        $argue = static function () use (&$state, $side, $playerId, $dice, &$events, $volba): bool {
            $team = $state->getTeamState($side);
            if ($team->isHeadCoachEjected() || ($volba !== null && !$volba['argue'])) {
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
