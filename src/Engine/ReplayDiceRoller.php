<?php

declare(strict_types=1);

namespace App\Engine;

/**
 * Diving Catch jako otázka kouči (uživatel 30.10.2026). Otázka přijde uprostřed akce, až po hodech
 * (přihrávka, rozptyl, vhazování). PHP kostky nejdou vrátit (`random_int`), proto akce přerušená
 * otázkou zapíše, co hodila, a po odpovědi se přehraje: zapsané hody se vrátí ve stejném pořadí
 * (týž stav + tytéž hody = totéž místo otázky) a dál se hází normálně. Vzor C++ `CoachChoiceScope`
 * (tam se vrací stav i kostky a akce se spouští znovu s frontou odpovědí).
 */
final class ReplayDiceRoller implements DiceRollerInterface
{
    /** @var list<int> */
    private array $log = [];
    /** @var list<int> */
    private array $replay = [];

    public function __construct(private readonly DiceRollerInterface $dice) {}

    /**
     * Začátek akce: `$replay` = hody zapsané při přerušeném pokusu (prázdné = nová akce).
     *
     * @param list<int> $replay
     */
    public function begin(array $replay): void
    {
        $this->log = [];
        $this->replay = $replay;
    }

    /** @return list<int> hody od `begin` (přehrané i nové) */
    public function getLog(): array
    {
        return $this->log;
    }

    public function rollD6(): int
    {
        return $this->next(fn(): int => $this->dice->rollD6());
    }

    public function roll2D6(): int
    {
        return $this->next(fn(): int => $this->dice->roll2D6());
    }

    public function rollD8(): int
    {
        return $this->next(fn(): int => $this->dice->rollD8());
    }

    /** @param callable(): int $roll */
    private function next(callable $roll): int
    {
        $value = $this->replay !== [] ? array_shift($this->replay) : $roll();
        $this->log[] = $value;

        return $value;
    }
}
