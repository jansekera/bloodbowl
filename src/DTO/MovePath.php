<?php
declare(strict_types=1);

namespace App\DTO;

use App\ValueObject\Position;

final class MovePath
{
    /**
     * @param list<MoveStep> $steps
     */
    public function __construct(
        private readonly Position $destination,
        private readonly array $steps,
        private readonly int $totalCost,
        private readonly int $dodgeCount,
        private readonly int $gfiCount,
    ) {
    }

    public function getDestination(): Position { return $this->destination; }
    /** @return list<MoveStep> */
    public function getSteps(): array { return $this->steps; }
    public function getTotalCost(): int { return $this->totalCost; }
    public function getDodgeCount(): int { return $this->dodgeCount; }
    public function getGfiCount(): int { return $this->gfiCount; }

    public function requiresRolls(): bool
    {
        return $this->dodgeCount > 0 || $this->gfiCount > 0;
    }

    /**
     * @return array<string, mixed>
     */
    public function toArray(): array
    {
        return [
            'destination' => $this->destination->toArray(),
            'steps' => array_map(fn(MoveStep $s) => $s->toArray(), $this->steps),
            'totalCost' => $this->totalCost,
            'dodgeCount' => $this->dodgeCount,
            'gfiCount' => $this->gfiCount,
        ];
    }
}
