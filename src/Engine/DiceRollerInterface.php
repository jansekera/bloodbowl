<?php
declare(strict_types=1);

namespace App\Engine;

interface DiceRollerInterface
{
    /**
     * Roll a single D6 (1-6).
     */
    public function rollD6(): int;

    /**
     * Roll 2D6 and return sum (2-12).
     */
    public function roll2D6(): int;

    /**
     * Roll a D8 (1-8) for scatter direction.
     */
    public function rollD8(): int;
}
