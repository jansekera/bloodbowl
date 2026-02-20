<?php

declare(strict_types=1);

namespace App\ValueObject;

final class PlayerStats
{
    public function __construct(
        private readonly int $movement,
        private readonly int $strength,
        private readonly int $agility,
        private readonly int $armour,
    ) {
    }

    public function getMovement(): int
    {
        return $this->movement;
    }

    public function getStrength(): int
    {
        return $this->strength;
    }

    public function getAgility(): int
    {
        return $this->agility;
    }

    public function getArmour(): int
    {
        return $this->armour;
    }

    /**
     * @return array{movement: int, strength: int, agility: int, armour: int}
     */
    public function toArray(): array
    {
        return [
            'movement' => $this->movement,
            'strength' => $this->strength,
            'agility' => $this->agility,
            'armour' => $this->armour,
        ];
    }

    public function equals(self $other): bool
    {
        return $this->movement === $other->movement
            && $this->strength === $other->strength
            && $this->agility === $other->agility
            && $this->armour === $other->armour;
    }
}
