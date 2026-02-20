<?php
declare(strict_types=1);

namespace App\Engine;

use App\DTO\GameState;
use App\DTO\MatchPlayerDTO;
use App\Enum\SkillName;
use App\ValueObject\Position;

final class StrengthCalculator
{
    /**
     * Calculate effective blocking strength (base ST + assists).
     */
    public function calculateEffectiveStrength(
        GameState $state,
        MatchPlayerDTO $player,
        Position $targetPosition,
    ): int {
        return $player->getStats()->getStrength()
            + $this->countAssists($state, $player, $targetPosition);
    }

    /**
     * Count friendly assists for a block.
     *
     * An assist is a friendly standing player adjacent to the target position,
     * who is not in any enemy tackle zone OTHER THAN the block target
     * (unless they have the Guard skill).
     */
    public function countAssists(
        GameState $state,
        MatchPlayerDTO $blocker,
        Position $targetPosition,
    ): int {
        $assists = 0;
        $friendlySide = $blocker->getTeamSide();
        $target = $state->getPlayerAtPosition($targetPosition);
        $targetId = $target?->getId();

        foreach ($state->getPlayersOnPitch($friendlySide) as $friend) {
            if ($friend->getId() === $blocker->getId()) {
                continue;
            }
            if (!$friend->getState()->canAct()) {
                continue;
            }

            $friendPos = $friend->getPosition();
            if ($friendPos === null || $friendPos->distanceTo($targetPosition) !== 1) {
                continue;
            }

            // Must not be in an enemy TZ (excluding the block target), unless has Guard
            if (!$friend->hasSkill(SkillName::Guard) && $this->isInTackleZoneExcluding($state, $friend, $targetId)) {
                continue;
            }

            $assists++;
        }

        return $assists;
    }

    /**
     * Check if player is in any enemy TZ, excluding a specific player.
     */
    private function isInTackleZoneExcluding(GameState $state, MatchPlayerDTO $player, ?int $excludeId): bool
    {
        $pos = $player->getPosition();
        if ($pos === null) {
            return false;
        }

        $enemySide = $player->getTeamSide()->opponent();
        foreach ($state->getPlayersOnPitch($enemySide) as $enemy) {
            if ($enemy->getId() === $excludeId) {
                continue;
            }
            if (!$enemy->getState()->exertsTacklezone()) {
                continue;
            }
            $enemyPos = $enemy->getPosition();
            if ($enemyPos !== null && $pos->distanceTo($enemyPos) === 1) {
                return true;
            }
        }

        return false;
    }

    /**
     * Determine block dice count and who chooses.
     *
     * @return array{count: int, attackerChooses: bool}
     */
    public function getBlockDiceInfo(int $attackerStrength, int $defenderStrength): array
    {
        if ($attackerStrength >= 2 * $defenderStrength) {
            return ['count' => 3, 'attackerChooses' => true];
        }
        if ($attackerStrength > $defenderStrength) {
            return ['count' => 2, 'attackerChooses' => true];
        }
        if ($attackerStrength === $defenderStrength) {
            return ['count' => 1, 'attackerChooses' => true];
        }
        if ($defenderStrength >= 2 * $attackerStrength) {
            return ['count' => 3, 'attackerChooses' => false];
        }

        return ['count' => 2, 'attackerChooses' => false];
    }
}
