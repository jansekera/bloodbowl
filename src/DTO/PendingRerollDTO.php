<?php
declare(strict_types=1);

namespace App\DTO;

/**
 * Captures a failed roll awaiting player reroll decision (dodge, GFI, pickup).
 */
final class PendingRerollDTO
{
    public function __construct(
        private readonly string $rollType,
        private readonly int $playerId,
        private readonly int $target,
        private readonly int $roll,
        private readonly bool $proAvailable,
        private readonly bool $teamRerollAvailable,
        private readonly int $targetX,
        private readonly int $targetY,
    ) {
    }

    public function getRollType(): string { return $this->rollType; }
    public function getPlayerId(): int { return $this->playerId; }
    public function getTarget(): int { return $this->target; }
    public function getRoll(): int { return $this->roll; }
    public function isProAvailable(): bool { return $this->proAvailable; }
    public function isTeamRerollAvailable(): bool { return $this->teamRerollAvailable; }
    public function getTargetX(): int { return $this->targetX; }
    public function getTargetY(): int { return $this->targetY; }

    public function withProUsed(): self
    {
        return new self(
            $this->rollType, $this->playerId, $this->target, $this->roll,
            false, $this->teamRerollAvailable, $this->targetX, $this->targetY,
        );
    }

    public function withRoll(int $roll): self
    {
        return new self(
            $this->rollType, $this->playerId, $this->target, $roll,
            $this->proAvailable, $this->teamRerollAvailable, $this->targetX, $this->targetY,
        );
    }

    /**
     * @return array<string, mixed>
     */
    public function toArray(): array
    {
        return [
            'rollType' => $this->rollType,
            'playerId' => $this->playerId,
            'target' => $this->target,
            'roll' => $this->roll,
            'proAvailable' => $this->proAvailable,
            'teamRerollAvailable' => $this->teamRerollAvailable,
            'targetX' => $this->targetX,
            'targetY' => $this->targetY,
        ];
    }

    /**
     * @param array<string, mixed> $data
     */
    public static function fromArray(array $data): self
    {
        return new self(
            rollType: (string) $data['rollType'],
            playerId: (int) $data['playerId'],
            target: (int) $data['target'],
            roll: (int) $data['roll'],
            proAvailable: (bool) ($data['proAvailable'] ?? false),
            teamRerollAvailable: (bool) ($data['teamRerollAvailable'] ?? false),
            targetX: (int) ($data['targetX'] ?? 0),
            targetY: (int) ($data['targetY'] ?? 0),
        );
    }
}
