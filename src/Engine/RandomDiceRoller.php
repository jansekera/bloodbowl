<?php
declare(strict_types=1);

namespace App\Engine;

final class RandomDiceRoller implements DiceRollerInterface
{
    public function rollD6(): int
    {
        return random_int(1, 6);
    }

    public function roll2D6(): int
    {
        return $this->rollD6() + $this->rollD6();
    }

    public function rollD8(): int
    {
        return random_int(1, 8);
    }
}
