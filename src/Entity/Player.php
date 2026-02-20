<?php

declare(strict_types=1);

namespace App\Entity;

use App\Enum\PlayerStatus;
use App\ValueObject\PlayerStats;

final class Player
{
    /**
     * @param list<Skill> $skills
     */
    public function __construct(
        private readonly int $id,
        private readonly int $teamId,
        private readonly int $positionalTemplateId,
        private readonly string $name,
        private readonly int $number,
        private readonly PlayerStats $stats,
        private readonly int $spp,
        private readonly int $level,
        private readonly PlayerStatus $status,
        private readonly string $createdAt,
        private readonly ?string $positionalName = null,
        private readonly array $skills = [],
    ) {
    }

    /**
     * @param array<string, mixed> $row
     */
    public static function fromRow(array $row): self
    {
        return new self(
            id: (int) $row['id'],
            teamId: (int) $row['team_id'],
            positionalTemplateId: (int) $row['positional_template_id'],
            name: (string) $row['name'],
            number: (int) $row['number'],
            stats: new PlayerStats(
                movement: (int) $row['ma'],
                strength: (int) $row['st'],
                agility: (int) $row['ag'],
                armour: (int) $row['av'],
            ),
            spp: (int) $row['spp'],
            level: (int) $row['level'],
            status: PlayerStatus::from((string) $row['status']),
            createdAt: (string) $row['created_at'],
            positionalName: isset($row['positional_name']) ? (string) $row['positional_name'] : null,
        );
    }

    /**
     * @param list<Skill> $skills
     */
    public function withSkills(array $skills): self
    {
        return new self(
            id: $this->id,
            teamId: $this->teamId,
            positionalTemplateId: $this->positionalTemplateId,
            name: $this->name,
            number: $this->number,
            stats: $this->stats,
            spp: $this->spp,
            level: $this->level,
            status: $this->status,
            createdAt: $this->createdAt,
            positionalName: $this->positionalName,
            skills: $skills,
        );
    }

    public function getId(): int
    {
        return $this->id;
    }

    public function getTeamId(): int
    {
        return $this->teamId;
    }

    public function getPositionalTemplateId(): int
    {
        return $this->positionalTemplateId;
    }

    public function getName(): string
    {
        return $this->name;
    }

    public function getNumber(): int
    {
        return $this->number;
    }

    public function getStats(): PlayerStats
    {
        return $this->stats;
    }

    public function getSpp(): int
    {
        return $this->spp;
    }

    public function getLevel(): int
    {
        return $this->level;
    }

    public function getStatus(): PlayerStatus
    {
        return $this->status;
    }

    public function getCreatedAt(): string
    {
        return $this->createdAt;
    }

    public function getPositionalName(): ?string
    {
        return $this->positionalName;
    }

    /**
     * @return list<Skill>
     */
    public function getSkills(): array
    {
        return $this->skills;
    }

    public function isActive(): bool
    {
        return $this->status->isAvailable();
    }

    /**
     * @return array<string, mixed>
     */
    public function toArray(): array
    {
        return [
            'id' => $this->id,
            'team_id' => $this->teamId,
            'positional_template_id' => $this->positionalTemplateId,
            'positional_name' => $this->positionalName,
            'name' => $this->name,
            'number' => $this->number,
            'stats' => $this->stats->toArray(),
            'spp' => $this->spp,
            'level' => $this->level,
            'status' => $this->status->value,
            'skills' => array_map(fn(Skill $s) => $s->toArray(), $this->skills),
        ];
    }
}
