<?php

declare(strict_types=1);

namespace App\Entity;

use App\ValueObject\PlayerStats;

final class PositionalTemplate
{
    /**
     * @param list<Skill> $startingSkills
     * @param list<string> $normalAccess
     * @param list<string> $doubleAccess
     */
    public function __construct(
        private readonly int $id,
        private readonly int $raceId,
        private readonly string $name,
        private readonly int $maxCount,
        private readonly int $cost,
        private readonly PlayerStats $stats,
        private readonly array $startingSkills = [],
        private readonly array $normalAccess = [],
        private readonly array $doubleAccess = [],
    ) {
    }

    /**
     * @param array<string, mixed> $row
     */
    public static function fromRow(array $row): self
    {
        $normalAccess = isset($row['normal_access'])
            ? explode(',', (string) $row['normal_access'])
            : [];
        $doubleAccess = isset($row['double_access'])
            ? explode(',', (string) $row['double_access'])
            : [];

        return new self(
            id: (int) $row['id'],
            raceId: (int) $row['race_id'],
            name: (string) $row['name'],
            maxCount: (int) $row['max_count'],
            cost: (int) $row['cost'],
            stats: new PlayerStats(
                movement: (int) $row['ma'],
                strength: (int) $row['st'],
                agility: (int) $row['ag'],
                armour: (int) $row['av'],
            ),
            normalAccess: array_values(array_filter($normalAccess)),
            doubleAccess: array_values(array_filter($doubleAccess)),
        );
    }

    /**
     * @param list<Skill> $skills
     */
    public function withStartingSkills(array $skills): self
    {
        return new self(
            id: $this->id,
            raceId: $this->raceId,
            name: $this->name,
            maxCount: $this->maxCount,
            cost: $this->cost,
            stats: $this->stats,
            startingSkills: $skills,
            normalAccess: $this->normalAccess,
            doubleAccess: $this->doubleAccess,
        );
    }

    public function getId(): int
    {
        return $this->id;
    }

    public function getRaceId(): int
    {
        return $this->raceId;
    }

    public function getName(): string
    {
        return $this->name;
    }

    public function getMaxCount(): int
    {
        return $this->maxCount;
    }

    public function getCost(): int
    {
        return $this->cost;
    }

    public function getStats(): PlayerStats
    {
        return $this->stats;
    }

    /**
     * @return list<Skill>
     */
    public function getStartingSkills(): array
    {
        return $this->startingSkills;
    }

    /**
     * @return list<string>
     */
    public function getNormalAccess(): array
    {
        return $this->normalAccess;
    }

    /**
     * @return list<string>
     */
    public function getDoubleAccess(): array
    {
        return $this->doubleAccess;
    }

    /**
     * @return array<string, mixed>
     */
    public function toArray(): array
    {
        return [
            'id' => $this->id,
            'race_id' => $this->raceId,
            'name' => $this->name,
            'max_count' => $this->maxCount,
            'cost' => $this->cost,
            'stats' => $this->stats->toArray(),
            'starting_skills' => array_map(
                fn(Skill $s) => $s->toArray(),
                $this->startingSkills,
            ),
            'normal_access' => $this->normalAccess,
            'double_access' => $this->doubleAccess,
        ];
    }
}
