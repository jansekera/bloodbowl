<?php
declare(strict_types=1);

namespace App\DTO;

use App\Enum\PlayerState;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use App\ValueObject\PlayerStats;
use App\ValueObject\Position;

final class MatchPlayerDTO
{
    /**
     * @param list<SkillName> $skills
     */
    public function __construct(
        private readonly int $id,
        private readonly int $playerId,
        private readonly string $name,
        private readonly int $number,
        private readonly string $positionalName,
        private readonly PlayerStats $stats,
        private readonly array $skills,
        private readonly TeamSide $teamSide,
        private PlayerState $state,
        private ?Position $position,
        private bool $hasMoved,
        private bool $hasActed,
        private int $movementRemaining,
        private bool $usedBlitz,
        private bool $lostTacklezones = false,
        private bool $proUsedThisTurn = false,
        private readonly ?string $raceName = null,
    ) {
    }

    /**
     * @param list<SkillName> $skills
     */
    public static function create(
        int $id,
        int $playerId,
        string $name,
        int $number,
        string $positionalName,
        PlayerStats $stats,
        array $skills,
        TeamSide $teamSide,
        Position $position,
        ?string $raceName = null,
    ): self {
        return new self(
            id: $id,
            playerId: $playerId,
            name: $name,
            number: $number,
            positionalName: $positionalName,
            stats: $stats,
            skills: $skills,
            teamSide: $teamSide,
            state: PlayerState::STANDING,
            position: $position,
            hasMoved: false,
            hasActed: false,
            movementRemaining: $stats->getMovement(),
            usedBlitz: false,
            raceName: $raceName,
        );
    }

    public function getId(): int { return $this->id; }
    public function getPlayerId(): int { return $this->playerId; }
    public function getName(): string { return $this->name; }
    public function getNumber(): int { return $this->number; }
    public function getPositionalName(): string { return $this->positionalName; }
    public function getStats(): PlayerStats { return $this->stats; }
    /** @return list<SkillName> */
    public function getSkills(): array { return $this->skills; }
    public function getTeamSide(): TeamSide { return $this->teamSide; }
    public function getState(): PlayerState { return $this->state; }
    public function getPosition(): ?Position { return $this->position; }
    public function hasMoved(): bool { return $this->hasMoved; }
    public function hasActed(): bool { return $this->hasActed; }
    public function getMovementRemaining(): int { return $this->movementRemaining; }
    public function hasUsedBlitz(): bool { return $this->usedBlitz; }
    public function hasLostTacklezones(): bool { return $this->lostTacklezones; }
    public function isProUsedThisTurn(): bool { return $this->proUsedThisTurn; }
    public function getRaceName(): ?string { return $this->raceName; }

    public function hasSkill(SkillName $skillName): bool
    {
        return in_array($skillName, $this->skills, true);
    }

    public function canAct(): bool
    {
        return $this->state->canAct() && !$this->hasActed;
    }

    public function canMove(): bool
    {
        return ($this->state === PlayerState::STANDING || $this->state === PlayerState::PRONE) && !$this->hasMoved;
    }

    // --- Mutation methods (used by engine) ---

    public function withState(PlayerState $state): self
    {
        $clone = clone $this;
        $clone->state = $state;
        return $clone;
    }

    public function withPosition(?Position $position): self
    {
        $clone = clone $this;
        $clone->position = $position;
        return $clone;
    }

    public function withHasMoved(bool $hasMoved): self
    {
        $clone = clone $this;
        $clone->hasMoved = $hasMoved;
        return $clone;
    }

    public function withHasActed(bool $hasActed): self
    {
        $clone = clone $this;
        $clone->hasActed = $hasActed;
        return $clone;
    }

    public function withMovementRemaining(int $remaining): self
    {
        $clone = clone $this;
        $clone->movementRemaining = $remaining;
        return $clone;
    }

    public function withUsedBlitz(bool $usedBlitz): self
    {
        $clone = clone $this;
        $clone->usedBlitz = $usedBlitz;
        return $clone;
    }

    public function withLostTacklezones(bool $lost): self
    {
        $clone = clone $this;
        $clone->lostTacklezones = $lost;
        return $clone;
    }

    public function withProUsedThisTurn(bool $used): self
    {
        $clone = clone $this;
        $clone->proUsedThisTurn = $used;
        return $clone;
    }

    /** @param list<SkillName> $skills */
    public function withSkills(array $skills): self
    {
        return new self(
            id: $this->id,
            playerId: $this->playerId,
            name: $this->name,
            number: $this->number,
            positionalName: $this->positionalName,
            stats: $this->stats,
            skills: $skills,
            teamSide: $this->teamSide,
            state: $this->state,
            position: $this->position,
            hasMoved: $this->hasMoved,
            hasActed: $this->hasActed,
            movementRemaining: $this->movementRemaining,
            usedBlitz: $this->usedBlitz,
            lostTacklezones: $this->lostTacklezones,
            proUsedThisTurn: $this->proUsedThisTurn,
            raceName: $this->raceName,
        );
    }

    /**
     * @return array<string, mixed>
     */
    public function toArray(): array
    {
        return [
            'id' => $this->id,
            'playerId' => $this->playerId,
            'name' => $this->name,
            'number' => $this->number,
            'positionalName' => $this->positionalName,
            'stats' => $this->stats->toArray(),
            'skills' => array_map(fn(SkillName $s) => $s->value, $this->skills),
            'teamSide' => $this->teamSide->value,
            'state' => $this->state->value,
            'position' => $this->position?->toArray(),
            'hasMoved' => $this->hasMoved,
            'hasActed' => $this->hasActed,
            'movementRemaining' => $this->movementRemaining,
            'lostTacklezones' => $this->lostTacklezones,
            'proUsedThisTurn' => $this->proUsedThisTurn,
            'raceName' => $this->raceName,
        ];
    }

    /**
     * @param array<string, mixed> $data
     */
    public static function fromArray(array $data): self
    {
        return new self(
            id: (int) $data['id'],
            playerId: (int) $data['playerId'],
            name: (string) $data['name'],
            number: (int) $data['number'],
            positionalName: (string) $data['positionalName'],
            stats: new PlayerStats(
                movement: (int) $data['stats']['movement'],
                strength: (int) $data['stats']['strength'],
                agility: (int) $data['stats']['agility'],
                armour: (int) $data['stats']['armour'],
            ),
            skills: array_map(fn(string $s) => SkillName::from($s), array_values((array) $data['skills'])),
            teamSide: TeamSide::from((string) $data['teamSide']),
            state: PlayerState::from((string) $data['state']),
            position: isset($data['position']) ? new Position((int) $data['position']['x'], (int) $data['position']['y']) : null,
            hasMoved: (bool) $data['hasMoved'],
            hasActed: (bool) $data['hasActed'],
            movementRemaining: (int) ($data['movementRemaining'] ?? 0),
            usedBlitz: (bool) ($data['usedBlitz'] ?? false),
            lostTacklezones: (bool) ($data['lostTacklezones'] ?? false),
            proUsedThisTurn: (bool) ($data['proUsedThisTurn'] ?? false),
            raceName: $data['raceName'] ?? null,
        );
    }
}
