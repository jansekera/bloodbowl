<?php

declare(strict_types=1);

namespace App\Engine;

use App\Enum\BlockDiceFace;
use App\Enum\RollCategory;
use App\Enum\RollType;
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
    /** @var list<int> */
    private array $blockDice = [];

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
        $this->blockDice = $answers->blockDice;
    }

    public function end(): void
    {
        $this->divingCatch = $this->bombTargets = $this->referee = $this->rerolls = $this->blockDice = [];
    }

    /** Skočí hráč pro míč? (ř. 8065 "can attempt") */
    public function divingCatch(int $playerId, Position $landing, int $otherCandidates, ?int $attemptingId, GameState $stav): bool
    {
        return array_shift($this->divingCatch)
            ?? throw (new DivingCatchQuestion($playerId, $landing, $otherCandidates, $attemptingId))->naStavu($stav);
    }

    /**
     * Kam hodit chycenou bombu (P272).
     *
     * @param \Closure(): list<array{x: int, y: int, range: string}> $targets legální pole (počítají se jen pro otázku)
     * @param array{x: int, y: int} $suggested volba enginu (AI ji vezme, kouči se doporučí)
     * @return array{x: int, y: int}
     */
    public function bombTarget(TeamSide $strana, int $playerId, \Closure $targets, array $suggested, GameState $stav): array
    {
        if (!$this->decides($strana)) {
            return $suggested;   // P306 A5: AI hodí, kam radí engine
        }

        return array_shift($this->bombTargets) ?? throw (new BombRethrowQuestion($playerId, $targets(), $suggested))->naStavu($stav);
    }

    /**
     * Úplatek / protest u vyloučení (P291).
     *
     * @return array{bribe: bool, argue: bool}
     */
    public function referee(TeamSide $strana, int $playerId, bool $canBribe, bool $canArgue, GameState $stav): array
    {
        if (!$this->decides($strana)) {
            return ['bribe' => true, 'argue' => true];   // P306 A5: AI použije, co jde
        }

        return array_shift($this->referee) ?? throw (new RefereeQuestion($playerId, $canBribe, $canArgue))->naStavu($stav);
    }

    /**
     * Index kostky bloku, kterou volí kouč-člověk (P308). `rules_bb2016.txt` ř. 633-634: "The coach of
     * the stronger player picks which block dice is used." Komu se ptát, rozhoduje `BlockHandler::kostkaKouce`.
     *
     * @param list<BlockDiceFace> $faces
     */
    public function blockDie(int $attackerId, int $defenderId, array $faces, bool $isFrenzy, GameState $stav): int
    {
        $i = array_shift($this->blockDice)
            ?? throw (new BlockDieQuestion($attackerId, $defenderId, $faces, $isFrenzy))->naStavu($stav->withPendingBlock(null));

        return isset($faces[$i]) ? $i : throw new \InvalidArgumentException("Block die {$i} not rolled");
    }

    /**
     * Přehoz nepovedeného hodu (P292).
     *
     * @return 'pro'|'team_reroll'|'decline'
     */
    public function reroll(
        TeamSide $strana,
        int $playerId,
        RollType $rollType,
        int $roll,
        int $target,
        bool $pro,
        bool $team,
        bool $proFailed,
        GameState $stav,
        ?Position $kam = null,
    ): string {
        if (!$this->decides($strana)) {
            // P306 A5: odpověď AI na jednom místě — Pro (dokud nepadl), jinak týmový přehoz (dřív druhá uzávěra
            //   v ThrowReroll); kontroly velkých hráčů AI nepřehazuje (parita s C++ P130; review P306: dřív větev
            //   v BigGuyCheckResolver)
            if ($rollType->category() === RollCategory::BIG_GUY) {
                return 'decline';
            }

            return ($pro && !$proFailed) ? 'pro' : 'team_reroll';
        }
        $a = array_shift($this->rerolls)
            ?? throw (new RerollQuestion($playerId, $rollType, $roll, $target, $pro, $team, $proFailed, $kam))->naStavu($stav);

        return match (true) {
            $a === 'decline' => 'decline',
            $a === 'pro' && $pro => 'pro',
            $a === 'team_reroll' && $team => 'team_reroll',
            default => throw new \InvalidArgumentException("Reroll choice {$a} not available"),
        };
    }
}
