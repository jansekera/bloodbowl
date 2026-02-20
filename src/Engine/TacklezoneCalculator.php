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
    public function countTacklezones(GameState $state, Position $position, TeamSide $friendlySide): int
    {
        $count = 0;
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

        // Base: 7 - AG, modifier: +1 per TZ at destination (first TZ is "free" with basic dodge)
        $target = 7 - $agility + max(0, $tzAtDest - 1);

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

        // Dodge skill gives +1 bonus (effectively -1 to target)
        if ($player->hasSkill(SkillName::Dodge)) {
            $target--;
        }

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
