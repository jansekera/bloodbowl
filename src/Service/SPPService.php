<?php

declare(strict_types=1);

namespace App\Service;

use App\DTO\GameEvent;
use App\DTO\MatchStatsDTO;
use App\Entity\Skill;
use App\Enum\SkillCategory;

final class SPPService
{
    /** @var array<int, int> Level thresholds (total SPP required) */
    private const LEVEL_THRESHOLDS = [6, 16, 31, 51, 76, 176];

    /**
     * Collect match stats from game events.
     *
     * @param list<GameEvent> $events
     * @return array<int, MatchStatsDTO> keyed by player ID
     */
    public function collectStats(array $events): array
    {
        /** @var array<int, MatchStatsDTO> */
        $stats = [];

        foreach ($events as $event) {
            $data = $event->getData();
            $type = $event->getType();

            switch ($type) {
                case 'touchdown':
                    $playerId = (int) $data['playerId'];
                    $stats[$playerId] = $this->getOrCreate($stats, $playerId)->withTouchdown();
                    break;

                case 'pass':
                    $result = $data['result'] ?? '';
                    if ($result === 'accurate' || $result === 'inaccurate') {
                        $playerId = (int) $data['playerId'];
                        $stats[$playerId] = $this->getOrCreate($stats, $playerId)->withCompletion();
                    }
                    break;

                case 'interception':
                    if (($data['success'] ?? false) === true) {
                        $playerId = (int) $data['playerId'];
                        $stats[$playerId] = $this->getOrCreate($stats, $playerId)->withInterception();
                    }
                    break;

                case 'injury_roll':
                    if (($data['result'] ?? '') === 'casualty') {
                        // The attacker caused the casualty (not tracked directly in event data)
                        // This would need the attacker ID to be propagated. For now, skip.
                    }
                    break;
            }
        }

        return $stats;
    }

    /**
     * Award MVP to a random player from the list.
     *
     * @param list<int> $playerIds
     * @param array<int, MatchStatsDTO> $stats
     * @return array<int, MatchStatsDTO>
     */
    public function awardMvp(array $playerIds, array $stats, int $randomIndex): array
    {
        if (empty($playerIds)) {
            return $stats;
        }

        $mvpId = $playerIds[$randomIndex % count($playerIds)];
        $stats[$mvpId] = $this->getOrCreate($stats, $mvpId)->withMvp();

        return $stats;
    }

    /**
     * Calculate the level for a given total SPP.
     */
    public function getLevel(int $totalSpp): int
    {
        $level = 1;
        foreach (self::LEVEL_THRESHOLDS as $threshold) {
            if ($totalSpp >= $threshold) {
                $level++;
            } else {
                break;
            }
        }
        return $level;
    }

    /**
     * Get SPP needed for next level.
     */
    public function getSppForNextLevel(int $totalSpp): ?int
    {
        foreach (self::LEVEL_THRESHOLDS as $threshold) {
            if ($totalSpp < $threshold) {
                return $threshold;
            }
        }
        return null; // max level
    }

    /**
     * Check if player leveled up by comparing before/after SPP.
     */
    public function hasLeveledUp(int $previousSpp, int $newSpp): bool
    {
        return $this->getLevel($newSpp) > $this->getLevel($previousSpp);
    }

    /**
     * Get skills available for advancement.
     *
     * @param list<Skill> $ownedSkills Player's current skills
     * @param list<string> $normalAccess Normal access category codes (e.g. ['G'], ['GS'])
     * @param list<string> $doubleAccess Double access category codes
     * @param list<Skill> $allSkills All skills in the game
     * @return array{normal: list<Skill>, double: list<Skill>}
     */
    public function getAvailableSkills(
        array $ownedSkills,
        array $normalAccess,
        array $doubleAccess,
        array $allSkills,
    ): array {
        $ownedNames = array_map(fn(Skill $s) => $s->getName(), $ownedSkills);
        $normalAccessStr = implode('', $normalAccess);
        $doubleAccessStr = implode('', $doubleAccess);

        $normal = [];
        $double = [];

        foreach ($allSkills as $skill) {
            if (in_array($skill->getName(), $ownedNames, true)) {
                continue;
            }
            if ($skill->getCategory() === SkillCategory::EXTRAORDINARY) {
                continue;
            }

            $catCode = self::categoryToCode($skill->getCategory());

            if (str_contains($normalAccessStr, $catCode)) {
                $normal[] = $skill;
            } elseif (str_contains($doubleAccessStr, $catCode)) {
                $double[] = $skill;
            }
        }

        return ['normal' => $normal, 'double' => $double];
    }

    /**
     * Check if player can advance (has pending level-up).
     * A player can advance if their level from SPP is higher than 1 + number of learned (non-starting) skills.
     */
    public function canAdvance(int $totalSpp, int $nonStartingSkillCount): bool
    {
        $level = $this->getLevel($totalSpp);
        return $level > 1 + $nonStartingSkillCount;
    }

    public static function categoryToCode(SkillCategory $category): string
    {
        return match ($category) {
            SkillCategory::GENERAL => 'G',
            SkillCategory::AGILITY => 'A',
            SkillCategory::STRENGTH => 'S',
            SkillCategory::PASSING => 'P',
            SkillCategory::MUTATION => 'M',
            SkillCategory::EXTRAORDINARY => 'X',
        };
    }

    /**
     * @param array<int, MatchStatsDTO> $stats
     */
    private function getOrCreate(array $stats, int $playerId): MatchStatsDTO
    {
        return $stats[$playerId] ?? new MatchStatsDTO($playerId);
    }

}
