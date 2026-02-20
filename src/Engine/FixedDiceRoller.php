<?php
declare(strict_types=1);

namespace App\Engine;

final class FixedDiceRoller implements DiceRollerInterface
{
    /** @var list<int> */
    private array $rolls;
    private int $index = 0;

    /**
     * @param list<int> $rolls Sequence of rolls to return
     */
    public function __construct(array $rolls)
    {
        $this->rolls = $rolls;
    }

    public function rollD6(): int
    {
        return $this->nextRoll();
    }

    public function roll2D6(): int
    {
        return $this->nextRoll() + $this->nextRoll();
    }

    public function rollD8(): int
    {
        return $this->nextRoll();
    }

    public function getRollCount(): int
    {
        return $this->index;
    }

    public function hasRemainingRolls(): bool
    {
        return $this->index < count($this->rolls);
    }

    private function nextRoll(): int
    {
        if ($this->index >= count($this->rolls)) {
            throw new \RuntimeException(
                "FixedDiceRoller: no more rolls available (used {$this->index} rolls)"
            );
        }

        return $this->rolls[$this->index++];
    }
}
