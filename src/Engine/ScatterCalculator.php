<?php

declare(strict_types=1);

namespace App\Engine;

use App\Enum\TeamSide;
use App\ValueObject\Position;

final class ScatterCalculator
{
    /** @var array<int, array{int, int}> D8 direction offsets: 1=N, 2=NE, 3=E, 4=SE, 5=S, 6=SW, 7=W, 8=NW */
    private const DIRECTION_OFFSETS = [
        1 => [0, -1],
        2 => [1, -1],
        3 => [1, 0],
        4 => [1, 1],
        5 => [0, 1],
        6 => [-1, 1],
        7 => [-1, 0],
        8 => [-1, -1],
    ];

    /**
     * Scatter one square in a D8 direction (for bounce).
     */
    public function scatterOnce(Position $from, int $d8Direction): Position
    {
        [$dx, $dy] = self::DIRECTION_OFFSETS[$d8Direction];
        return new Position($from->getX() + $dx, $from->getY() + $dy);
    }

    /**
     * Scatter D8 direction × D6 distance (for kickoff scatter / inaccurate pass).
     */
    public function scatterWithDistance(Position $from, int $d8Direction, int $d6Distance): Position
    {
        [$dx, $dy] = self::DIRECTION_OFFSETS[$d8Direction];
        return new Position(
            $from->getX() + $dx * $d6Distance,
            $from->getY() + $dy * $d6Distance,
        );
    }

    /**
     * @return array{int, int} [dx, dy]
     */
    public function getDirectionOffset(int $d8Direction): array
    {
        return self::DIRECTION_OFFSETS[$d8Direction];
    }

    /**
     * Check if position is in the receiving team's half.
     * Home receives: x 0..12. Away receives: x 13..25.
     */
    public function isInReceivingHalf(Position $pos, TeamSide $receivingTeam): bool
    {
        if (!$pos->isOnPitch()) {
            return false;
        }

        return $receivingTeam === TeamSide::HOME
            ? $pos->getX() <= 12
            : $pos->getX() >= 13;
    }
}
