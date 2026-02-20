<?php
declare(strict_types=1);

namespace App\DTO;

use App\ValueObject\Position;

final class MoveStep
{
    public function __construct(
        private readonly Position $position,
        private readonly int $movementCost,
        private readonly bool $requiresDodge,
        private readonly int $dodgeTarget,
        private readonly bool $isGfi,
        private readonly bool $isLeap = false,
    ) {
    }

    public function getPosition(): Position { return $this->position; }
    public function getMovementCost(): int { return $this->movementCost; }
    public function requiresDodge(): bool { return $this->requiresDodge; }
    public function getDodgeTarget(): int { return $this->dodgeTarget; }
    public function isGfi(): bool { return $this->isGfi; }
    public function isLeap(): bool { return $this->isLeap; }

    /**
     * @return array<string, mixed>
     */
    public function toArray(): array
    {
        return [
            'position' => $this->position->toArray(),
            'movementCost' => $this->movementCost,
            'requiresDodge' => $this->requiresDodge,
            'dodgeTarget' => $this->dodgeTarget,
            'isGfi' => $this->isGfi,
            'isLeap' => $this->isLeap,
        ];
    }
}
