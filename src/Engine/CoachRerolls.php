<?php

declare(strict_types=1);

namespace App\Engine;

use App\Enum\TeamSide;

/**
 * P292 (uživatel 10.11.2026: „oprav ať se ptá“): kouče-člověka se `ThrowReroll` zeptá, jestli nepovedený hod přehodí
 * (Pro / týmový přehoz / ne). Odpovědi z dřívějších přerušení téže akce dostává v pořadí; když odpověď chybí, akce
 * se přeruší `RerollQuestion` a po odpovědi přehraje. Parita s C++ `coachAskReroll` (helpers.cpp).
 */
final class CoachRerolls
{
    /** @var list<TeamSide> */
    private array $sides = [];
    /** @var list<string> */
    private array $answers = [];

    /** @param list<TeamSide> $sides strany kouče-člověka */
    public function setSides(array $sides): void
    {
        $this->sides = $sides;
    }

    /** @param list<string> $answers */
    public function setAnswers(array $answers): void
    {
        $this->answers = $answers;
    }

    public function decides(TeamSide $side): bool
    {
        return in_array($side, $this->sides, true);
    }

    /** @return 'pro'|'team_reroll'|'decline' */
    public function ask(int $playerId, string $rollType, int $roll, int $target, bool $pro, bool $team, bool $proFailed = false): string
    {
        $a = array_shift($this->answers)
            ?? throw new RerollQuestion($playerId, $rollType, $roll, $target, $pro, $team, $proFailed);

        return match (true) {
            $a === 'decline' => 'decline',
            $a === 'pro' && $pro => 'pro',
            $a === 'team_reroll' && $team => 'team_reroll',
            default => throw new \InvalidArgumentException("Reroll choice {$a} not available"),
        };
    }
}
