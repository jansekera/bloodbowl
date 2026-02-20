<?php

declare(strict_types=1);

namespace App\Entity;

use App\Enum\TeamStatus;
use App\ValueObject\Treasury;

final class Team
{
    /**
     * @param list<Player> $players
     */
    public function __construct(
        private readonly int $id,
        private readonly int $coachId,
        private readonly int $raceId,
        private readonly string $name,
        private readonly Treasury $treasury,
        private readonly int $rerolls,
        private readonly int $fanFactor,
        private readonly bool $hasApothecary,
        private readonly int $assistantCoaches,
        private readonly int $cheerleaders,
        private readonly TeamStatus $status,
        private readonly string $createdAt,
        private readonly string $updatedAt,
        private readonly ?string $raceName = null,
        private readonly ?string $coachName = null,
        private readonly array $players = [],
    ) {
    }

    /**
     * @param array<string, mixed> $row
     */
    public static function fromRow(array $row): self
    {
        return new self(
            id: (int) $row['id'],
            coachId: (int) $row['coach_id'],
            raceId: (int) $row['race_id'],
            name: (string) $row['name'],
            treasury: new Treasury((int) $row['treasury']),
            rerolls: (int) $row['rerolls'],
            fanFactor: (int) $row['fan_factor'],
            hasApothecary: (bool) $row['has_apothecary'],
            assistantCoaches: (int) $row['assistant_coaches'],
            cheerleaders: (int) $row['cheerleaders'],
            status: TeamStatus::from((string) $row['status']),
            createdAt: (string) $row['created_at'],
            updatedAt: (string) $row['updated_at'],
            raceName: isset($row['race_name']) ? (string) $row['race_name'] : null,
            coachName: isset($row['coach_name']) ? (string) $row['coach_name'] : null,
        );
    }

    /**
     * @param list<Player> $players
     */
    public function withPlayers(array $players): self
    {
        return new self(
            id: $this->id,
            coachId: $this->coachId,
            raceId: $this->raceId,
            name: $this->name,
            treasury: $this->treasury,
            rerolls: $this->rerolls,
            fanFactor: $this->fanFactor,
            hasApothecary: $this->hasApothecary,
            assistantCoaches: $this->assistantCoaches,
            cheerleaders: $this->cheerleaders,
            status: $this->status,
            createdAt: $this->createdAt,
            updatedAt: $this->updatedAt,
            raceName: $this->raceName,
            coachName: $this->coachName,
            players: $players,
        );
    }

    public function getId(): int
    {
        return $this->id;
    }

    public function getCoachId(): int
    {
        return $this->coachId;
    }

    public function getRaceId(): int
    {
        return $this->raceId;
    }

    public function getName(): string
    {
        return $this->name;
    }

    public function getTreasury(): Treasury
    {
        return $this->treasury;
    }

    public function getRerolls(): int
    {
        return $this->rerolls;
    }

    public function getFanFactor(): int
    {
        return $this->fanFactor;
    }

    public function hasApothecary(): bool
    {
        return $this->hasApothecary;
    }

    public function getAssistantCoaches(): int
    {
        return $this->assistantCoaches;
    }

    public function getCheerleaders(): int
    {
        return $this->cheerleaders;
    }

    public function getStatus(): TeamStatus
    {
        return $this->status;
    }

    public function getCreatedAt(): string
    {
        return $this->createdAt;
    }

    public function getUpdatedAt(): string
    {
        return $this->updatedAt;
    }

    public function getRaceName(): ?string
    {
        return $this->raceName;
    }

    public function getCoachName(): ?string
    {
        return $this->coachName;
    }

    /**
     * @return list<Player>
     */
    public function getPlayers(): array
    {
        return $this->players;
    }

    /**
     * @return list<Player>
     */
    public function getActivePlayers(): array
    {
        return array_values(array_filter(
            $this->players,
            fn(Player $p) => $p->isActive(),
        ));
    }

    public function getPlayerCount(): int
    {
        return count($this->getActivePlayers());
    }

    public function getTeamValue(): int
    {
        $playerValue = 0;
        foreach ($this->getActivePlayers() as $player) {
            // In a full implementation, this would factor in skill costs
            // For now, we'd need the positional template cost
            $playerValue += 0; // Will be calculated via repository
        }

        return $playerValue + ($this->rerolls * 0); // Needs race reroll cost
    }

    /**
     * @return array<string, mixed>
     */
    public function toArray(): array
    {
        return [
            'id' => $this->id,
            'coach_id' => $this->coachId,
            'race_id' => $this->raceId,
            'race_name' => $this->raceName,
            'coach_name' => $this->coachName,
            'name' => $this->name,
            'treasury' => $this->treasury->getGold(),
            'rerolls' => $this->rerolls,
            'fan_factor' => $this->fanFactor,
            'has_apothecary' => $this->hasApothecary,
            'assistant_coaches' => $this->assistantCoaches,
            'cheerleaders' => $this->cheerleaders,
            'status' => $this->status->value,
            'player_count' => $this->getPlayerCount(),
            'players' => array_map(fn(Player $p) => $p->toArray(), $this->players),
        ];
    }
}
