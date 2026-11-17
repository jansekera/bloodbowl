<?php

declare(strict_types=1);

namespace App\Engine;

use App\DTO\GameEvent;
use App\DTO\GameState;
use App\DTO\ReplayAnswers;
use App\Enum\TeamSide;
use App\ValueObject\Position;

/**
 * P294 krok 3: kouč-člověk pro otázky uprostřed akce — jeho strany a odpovědi z dřívějších přerušení téže akce
 * (`ReplayAnswers`). Každá otázka vezme další odpověď svého druhu, a když chybí, akci přeruší (`CoachQuestion`).
 * Jeden objekt sdílený všemi handlery místo setterů stran a odpovědí v každém z nich (BallResolver, BombThrowHandler,
 * FoulHandler, GameFlowResolver, dřívější `CoachRerolls`). Parita s C++ `askCoach`.
 */
final class Coach
{
    /** @var list<TeamSide> */
    private array $sides = [];
    /** @var list<bool> */
    private array $divingCatch = [];
    /** @var list<array{x: int, y: int}> */
    private array $bombTargets = [];
    /** @var list<array{bribe: bool, argue: bool}> */
    private array $referee = [];
    /** @var list<string> */
    private array $rerolls = [];

    /** @param list<TeamSide> $sides strany kouče-člověka */
    public function __construct(array $sides = [])
    {
        $this->sides = $sides;
    }

    /** @param list<TeamSide> $sides strany kouče-člověka */
    public function setSides(array $sides): void
    {
        $this->sides = $sides;
    }

    public function decides(TeamSide $side): bool
    {
        return in_array($side, $this->sides, true);
    }

    /** Odpovědi pro (přehrávanou) akci; `end()` je po akci smaže. */
    public function begin(ReplayAnswers $answers): void
    {
        $this->divingCatch = $answers->divingCatch;
        $this->bombTargets = $answers->bombTargets;
        $this->referee = $answers->referee;
        $this->rerolls = $answers->rerolls;
    }

    public function end(): void
    {
        $this->divingCatch = $this->bombTargets = $this->referee = $this->rerolls = [];
    }

    /** Skočí hráč pro míč? (ř. 8065 "can attempt") */
    public function divingCatch(int $playerId, Position $landing, int $otherCandidates, ?int $attemptingId, GameState $stav): bool
    {
        return array_shift($this->divingCatch)
            ?? throw self::sPrubehem(new DivingCatchQuestion($playerId, $landing, $otherCandidates, $attemptingId), $stav, []);
    }

    /**
     * Kam hodit chycenou bombu (P272).
     *
     * @param list<array{x: int, y: int, range: string}> $targets
     * @param array{x: int, y: int}|null $suggested
     * @param list<GameEvent> $udalosti
     * @return array{x: int, y: int}
     */
    public function bombTarget(int $playerId, array $targets, ?array $suggested, GameState $stav, array $udalosti): array
    {
        return array_shift($this->bombTargets) ?? throw self::sPrubehem(new BombRethrowQuestion($playerId, $targets, $suggested), $stav, $udalosti);
    }

    /**
     * Úplatek / protest u vyloučení (P291).
     *
     * @param list<GameEvent> $udalosti
     * @return array{bribe: bool, argue: bool}
     */
    public function referee(int $playerId, bool $canBribe, bool $canArgue, GameState $stav, array $udalosti): array
    {
        return array_shift($this->referee) ?? throw self::sPrubehem(new RefereeQuestion($playerId, $canBribe, $canArgue), $stav, $udalosti);
    }

    /**
     * Přehoz nepovedeného hodu (P292).
     *
     * @param list<GameEvent> $udalosti
     * @return 'pro'|'team_reroll'|'decline'
     */
    public function reroll(
        int $playerId,
        string $rollType,
        int $roll,
        int $target,
        bool $pro,
        bool $team,
        bool $proFailed,
        GameState $stav,
        array $udalosti,
        ?Position $kam = null,
    ): string {
        $a = array_shift($this->rerolls)
            ?? throw self::sPrubehem(new RerollQuestion($playerId, $rollType, $roll, $target, $pro, $team, $proFailed, $kam), $stav, $udalosti);

        return match (true) {
            $a === 'decline' => 'decline',
            $a === 'pro' && $pro => 'pro',
            $a === 'team_reroll' && $team => 'team_reroll',
            default => throw new \InvalidArgumentException("Reroll choice {$a} not available"),
        };
    }

    /**
     * Otázka s průběhem akce v okamžiku hodu (P294 krok 5a).
     *
     * @param list<GameEvent> $udalosti
     */
    private static function sPrubehem(CoachQuestion $q, GameState $stav, array $udalosti): CoachQuestion
    {
        return $q->naStavu($stav, $udalosti);
    }
}
