<?php

declare(strict_types=1);

namespace App\Engine;

use App\DTO\GameEvent;
use App\DTO\MatchPlayerDTO;
use App\Enum\SkillName;

/**
 * ⭐ PRIDANO 18.09.2026: hod na Pro podle `rules_bb2016.txt` r. 8381-8387.
 *
 * "before the re-roll may be made, his coach must roll a D6. On a roll of 4, 5
 * or 6 the re-roll may be made. On a roll of 1, 2 or 3 the original result
 * stands and may not be re-rolled with a skill or team re-roll; however you
 * can re-roll the Pro roll with a Team re-roll."
 *
 * ⇒ Tymovy prehoz po Pro NIKDY nejde na puvodni kostku (r. 926: kostka se
 *   prehazuje nejvys jednou), jen na neuspesny hod Pro. Volajici po Pro uz
 *   zadny dalsi prehoz te kostky nezkousi.
 *
 * Volajici si sam zapise `withProUsedThisTurn(true)` a sam prehodi kostku,
 * kdyz `allowed`.
 */
final class ProCheck
{
    /**
     * @param list<GameEvent> $events doplni se udalosti hodu Pro (a Loner)
     * @return array{allowed: bool, teamRerollUsed: bool}
     */
    public static function roll(
        DiceRollerInterface $dice,
        MatchPlayerDTO $player,
        bool $teamRerollAvailable,
        array &$events,
    ): array {
        $proRoll = $dice->rollD6();
        $allowed = $proRoll >= 4;
        $events[] = GameEvent::proReroll($player->getId(), $proRoll, $allowed, null);
        if ($allowed || !$teamRerollAvailable) {
            return ['allowed' => $allowed, 'teamRerollUsed' => false];
        }

        // r. 8387: tymovy prehoz HODU PRO (Loner plati jako u kazdeho tymoveho prehozu)
        if ($player->hasSkill(SkillName::Loner)) {
            $lonerRoll = $dice->rollD6();
            $events[] = GameEvent::lonerCheck($player->getId(), $lonerRoll, $lonerRoll >= 4);
            if ($lonerRoll < 4) {
                return ['allowed' => false, 'teamRerollUsed' => true];
            }
        }
        $events[] = GameEvent::rerollUsed($player->getId(), 'Team Reroll');
        $proRoll = $dice->rollD6();
        $allowed = $proRoll >= 4;
        $events[] = GameEvent::proReroll($player->getId(), $proRoll, $allowed, null);

        return ['allowed' => $allowed, 'teamRerollUsed' => true];
    }
}
