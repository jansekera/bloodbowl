<?php
declare(strict_types=1);

namespace App\Engine;

use App\DTO\GameState;
use App\DTO\MatchPlayerDTO;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use App\ValueObject\Position;

final class TacklezoneCalculator
{
    /**
     * Count enemy tackle zones on a given position.
     */
    /**
     * @param int|null $exceptPlayerId hráč, který se do počtu NEPOČÍTÁ.
     *   ⭐ Kvůli Hypnotic Gaze: `rules_bb2016.txt` r. 8183-8185 chce „-1 for
     *   each opposing tackle zone … **other than the victim's**". Do 11.09.
     *   měl handler vlastní kopii téhle smyčky; výjimka patří sem, aby byl
     *   ten průchod jen jeden.
     */
    public function countTacklezones(
        GameState $state,
        Position $position,
        TeamSide $friendlySide,
        ?int $exceptPlayerId = null,
    ): int {
        $count = 0;
        $enemySide = $friendlySide->opponent();

        foreach ($state->getPlayersOnPitch($enemySide) as $enemy) {
            if ($exceptPlayerId !== null && $enemy->getId() === $exceptPlayerId) {
                continue;
            }
            if (!$enemy->getState()->exertsTacklezone()) {
                continue;
            }
            if ($enemy->hasLostTacklezones()) {
                continue;
            }

            $enemyPos = $enemy->getPosition();
            if ($enemyPos !== null && $position->distanceTo($enemyPos) === 1) {
                $count++;
            }
        }

        return $count;
    }

    /**
     * Check if a player is in any enemy tackle zone.
     */
    public function isInTacklezone(GameState $state, MatchPlayerDTO $player): bool
    {
        $pos = $player->getPosition();
        if ($pos === null) {
            return false;
        }

        return $this->countTacklezones($state, $pos, $player->getTeamSide()) > 0;
    }

    /**
     * Get all enemy players exerting tackle zones on a position.
     *
     * @return list<MatchPlayerDTO>
     */
    public function getMarkingPlayers(GameState $state, Position $position, TeamSide $friendlySide): array
    {
        $markers = [];
        $enemySide = $friendlySide->opponent();

        foreach ($state->getPlayersOnPitch($enemySide) as $enemy) {
            if (!$enemy->getState()->exertsTacklezone()) {
                continue;
            }
            if ($enemy->hasLostTacklezones()) {
                continue;
            }

            $enemyPos = $enemy->getPosition();
            if ($enemyPos !== null && $position->distanceTo($enemyPos) === 1) {
                $markers[] = $enemy;
            }
        }

        return $markers;
    }

    /**
     * Count Disturbing Presence enemies within 3 squares.
     */
    public function countDisturbingPresence(GameState $state, Position $position, TeamSide $friendlySide): int
    {
        $count = 0;
        $enemySide = $friendlySide->opponent();

        foreach ($state->getPlayersOnPitch($enemySide) as $enemy) {
            if (!$enemy->hasSkill(SkillName::DisturbingPresence)) {
                continue;
            }
            if (!$enemy->getState()->canAct()) {
                continue;
            }
            $enemyPos = $enemy->getPosition();
            if ($enemyPos !== null && $position->distanceTo($enemyPos) <= 3) {
                $count++;
            }
        }

        return $count;
    }

    /**
     * Calculate dodge target number (2+ to 6+).
     * Formula: 7 - agility + modifiers (tackle zones at destination - 1)
     * Minimum 2+, maximum 6+.
     *
     * @param Position|null $source Source position (for Prehensile Tail calculation)
     */
    public function calculateDodgeTarget(GameState $state, MatchPlayerDTO $player, Position $destination, ?Position $source = null): int
    {
        // Break Tackle: use ST instead of AG for dodge
        $agility = $player->hasSkill(SkillName::BreakTackle)
            ? $player->getStats()->getStrength()
            : $player->getStats()->getAgility();

        $tzAtDest = $this->countTacklezones($state, $destination, $player->getTeamSide());

        // ⛔⛔ OPRAVA 14.09.2026 -- CHYBEL BONUS ZA DODGE NA VOLNE POLE.
        //   `rules_bb2016.txt` r. 503-505: k hodu se pricita "+1 Making a Dodge
        //   roll" a odecita "-1 per opposing tackle zone on the square that the
        //   player is dodging to". Cil je tedy `(7-AG) - 1 + TZ`.
        //   Puvodni zapis `(7-AG) + max(0, TZ-1)` delal tyz bonus jako "prvni
        //   TZ zadarmo" -- coz je TOTEZ pro TZ >= 1, ale pro TZ == 0 o jedno
        //   HORSI: pravidla davaji AG4 cil `2+`, engine daval `3+`.
        //   ⇒ Lisi se JEN utek na uplne volne pole, a to je prave ten pripad,
        //   ktery rozhoduje, jestli nosic unikne z obklicení.
        $target = 7 - $agility - 1 + $tzAtDest;

        // Prehensile Tail: +1 for each enemy with the skill at the source position
        if ($source !== null) {
            $enemySide = $player->getTeamSide()->opponent();
            foreach ($state->getPlayersOnPitch($enemySide) as $enemy) {
                if (!$enemy->hasSkill(SkillName::PrehensileTail)) {
                    continue;
                }
                if ($enemy->hasLostTacklezones() || !$enemy->getState()->exertsTacklezone()) {
                    continue;
                }
                $enemyPos = $enemy->getPosition();
                if ($enemyPos !== null && $source->distanceTo($enemyPos) === 1) {
                    $target++;
                }
            }

            // Diving Tackle: +2 from one adjacent enemy at source with the skill
            foreach ($state->getPlayersOnPitch($enemySide) as $enemy) {
                if (!$enemy->hasSkill(SkillName::DivingTackle)) {
                    continue;
                }
                if ($enemy->hasLostTacklezones() || !$enemy->getState()->exertsTacklezone()) {
                    continue;
                }
                $enemyPos = $enemy->getPosition();
                if ($enemyPos !== null && $source->distanceTo($enemyPos) === 1) {
                    $target += 2;
                    break; // only one DT per dodge
                }
            }
        }

        // ⛔⛔ ODEBRANO 14.09.2026 -- SKILL `Dodge` SE POCITAL DVAKRAT.
        //   Pravidla (`rules_bb2016.txt` r. 8086-8092) davaji Dodgi
        //   RE-ROLL, ne modifikator: "is allowed to re-roll the D6 if he
        //   fails to dodge... may only re-roll ONE failed Dodge roll PER TURN".
        //   Ten re-roll engine UZ MA a ma ho spravne -- `MoveHandler.php:250`,
        //   vcetne zruseni sousednim `Tackle`. Zdejsi `-1` k cili byla tedy
        //   DRUHA porce tehoz skillu: hrac s Dodge dostaval lehci hod A JESTE
        //   opakovani. A skutecny hod bere cil odsud (`$step->getDodgeTarget()`),
        //   takze to nebyl jen odhad v pathfinderu.
        //   ⇒ Zustava jen re-roll. Tackle tim znovu znamena to, co ma:
        //   rusi Dodgi jedinou vyhodu, misto aby mu nechaval tichy bonus.

        // Stunty: -1 dodge target (easier dodge)
        if ($player->hasSkill(SkillName::Stunty)) {
            $target--;
        }

        // Titchy: -1 dodge target (easier dodge, stacks with Stunty)
        if ($player->hasSkill(SkillName::Titchy)) {
            $target--;
        }

        // Two Heads: -1 dodge target
        if ($player->hasSkill(SkillName::TwoHeads)) {
            $target--;
        }

        // Titchy enemies: easier to dodge away from (-1 per Titchy enemy exerting TZ at destination)
        $enemySideForTitchy = $player->getTeamSide()->opponent();
        foreach ($state->getPlayersOnPitch($enemySideForTitchy) as $enemy) {
            if (!$enemy->hasSkill(SkillName::Titchy)) {
                continue;
            }
            if (!$enemy->getState()->exertsTacklezone() || $enemy->hasLostTacklezones()) {
                continue;
            }
            $enemyPos = $enemy->getPosition();
            if ($enemyPos !== null && $destination->distanceTo($enemyPos) === 1) {
                $target--;
            }
        }

        // Clamp to 2-6 range
        return max(2, min(6, $target));
    }
}
