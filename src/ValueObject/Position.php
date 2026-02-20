<?php

declare(strict_types=1);

namespace App\ValueObject;

final class Position
{
    public const PITCH_WIDTH = 26;
    public const PITCH_HEIGHT = 15;

    public function __construct(
        private readonly int $x,
        private readonly int $y,
    ) {
    }

    public function getX(): int
    {
        return $this->x;
    }

    public function getY(): int
    {
        return $this->y;
    }

    public function isOnPitch(): bool
    {
        return $this->x >= 0 && $this->x < self::PITCH_WIDTH
            && $this->y >= 0 && $this->y < self::PITCH_HEIGHT;
    }

    public function isInEndZone(bool $home): bool
    {
        if (!$this->isOnPitch()) {
            return false;
        }

        return $home ? $this->x === 0 : $this->x === self::PITCH_WIDTH - 1;
    }

    public function isInWideZone(): bool
    {
        if (!$this->isOnPitch()) {
            return false;
        }

        return $this->y < 4 || $this->y >= self::PITCH_HEIGHT - 4;
    }

    public function distanceTo(self $other): int
    {
        return max(abs($this->x - $other->x), abs($this->y - $other->y));
    }

    public function equals(self $other): bool
    {
        return $this->x === $other->x && $this->y === $other->y;
    }

    /**
     * @return list<self>
     */
    public function getAdjacentPositions(): array
    {
        $positions = [];

        for ($dx = -1; $dx <= 1; $dx++) {
            for ($dy = -1; $dy <= 1; $dy++) {
                if ($dx === 0 && $dy === 0) {
                    continue;
                }

                $pos = new self($this->x + $dx, $this->y + $dy);
                if ($pos->isOnPitch()) {
                    $positions[] = $pos;
                }
            }
        }

        return $positions;
    }

    /**
     * @return array{x: int, y: int}
     */
    public function toArray(): array
    {
        return ['x' => $this->x, 'y' => $this->y];
    }

    public function __toString(): string
    {
        return "({$this->x},{$this->y})";
    }
}
