<?php

declare(strict_types=1);

namespace App\DTO;

/**
 * P292 (10.11.2026): odpovědi kouče-člověka z dřívějších přerušení TÉŽE akce, všech druhů otázek najednou. Akce se
 * po každé odpovědi přehraje od začátku se zapsanými hody (`ReplayDiceRoller`) a otázky dostanou odpovědi v pořadí.
 * Dřív si každá otázka nesla jen své odpovědi ⇒ akce se dvěma druhy otázek (přehoz přihrávky, pak Diving Catch)
 * by po druhé odpovědi znovu položila první otázku (review 10.11., P293 bod 4).
 *
 * P312: `withX` přes jediné `pridat` (dřív každá metoda vypisovala všechna pole — s každým novým druhem otázky
 * se měnilo osm míst).
 */
final class ReplayAnswers
{
    /**
     * @param list<bool> $divingCatch Diving Catch ano / ne
     * @param list<array{x: int, y: int}> $bombTargets kam hodit chycenou bombu (P272)
     * @param list<array{bribe: bool, argue: bool}> $referee úplatek / protest u faulu (P291)
     * @param list<string> $rerolls přehoz hodu: 'pro' | 'team_reroll' | 'decline' (P292)
     * @param list<int> $blockDice index kostky bloku, kterou zvolil obránce-člověk v tahu AI (P308)
     * @param list<bool> $juggernaut Both Down brát jako Pushed ano / ne (P309)
     * @param list<bool> $pilingOn použít Piling On ano / ne (P310)
     * @param list<bool> $wrestle použít Wrestle ano / ne (P312)
     * @param list<array{x: int, y: int}|null> $sideStep pole Side Step, null = nepoužít (P312)
     */
    public function __construct(
        public readonly array $divingCatch = [],
        public readonly array $bombTargets = [],
        public readonly array $referee = [],
        public readonly array $rerolls = [],
        public readonly array $blockDice = [],
        public readonly array $juggernaut = [],
        public readonly array $pilingOn = [],
        public readonly array $wrestle = [],
        public readonly array $sideStep = [],
    ) {}

    public function withDivingCatch(bool $use): self
    {
        return $this->pridat('divingCatch', $use);
    }

    /** @param array{x: int, y: int} $target */
    public function withBombTarget(array $target): self
    {
        return $this->pridat('bombTargets', $target);
    }

    /** @param array{bribe: bool, argue: bool} $choice */
    public function withReferee(array $choice): self
    {
        return $this->pridat('referee', $choice);
    }

    public function withReroll(string $choice): self
    {
        return $this->pridat('rerolls', $choice);
    }

    public function withBlockDie(int $faceIndex): self
    {
        return $this->pridat('blockDice', $faceIndex);
    }

    public function withJuggernaut(bool $push): self
    {
        return $this->pridat('juggernaut', $push);
    }

    public function withPilingOn(bool $use): self
    {
        return $this->pridat('pilingOn', $use);
    }

    public function withWrestle(bool $use): self
    {
        return $this->pridat('wrestle', $use);
    }

    /** @param array{x: int, y: int}|null $square */
    public function withSideStep(?array $square): self
    {
        return $this->pridat('sideStep', $square);
    }

    /**
     * P312: odpovědi `$a` před odpověďmi této sady (každý druh zvlášť) — pokračování akce přerušené otázkou
     * Stand Firm / Grab (`Coach::predradit`).
     */
    public function predradit(self $a): self
    {
        $pole = [];
        foreach ($this->toArray() as $k => $v) {
            $pole[$k] = [...$a->toArray()[$k], ...$v];
        }

        return self::zPoli($pole);
    }

    /** @return array<string, list<mixed>> */
    public function toArray(): array
    {
        return [
            'divingCatch' => $this->divingCatch,
            'bombTargets' => $this->bombTargets,
            'referee' => $this->referee,
            'rerolls' => $this->rerolls,
            'blockDice' => $this->blockDice,
            'juggernaut' => $this->juggernaut,
            'pilingOn' => $this->pilingOn,
            'wrestle' => $this->wrestle,
            'sideStep' => $this->sideStep,
        ];
    }

    /** @param array<string, mixed> $data */
    public static function fromArray(array $data): self
    {
        $pole = static fn(mixed $p): array => ['x' => (int) ((array) $p)['x'], 'y' => (int) ((array) $p)['y']];

        return new self(
            array_values(array_map('boolval', (array) ($data['divingCatch'] ?? []))),
            array_values(array_map($pole, (array) ($data['bombTargets'] ?? []))),
            array_values(array_map(
                static fn(mixed $a): array => ['bribe' => (bool) ((array) $a)['bribe'], 'argue' => (bool) ((array) $a)['argue']],
                (array) ($data['referee'] ?? []),
            )),
            array_values(array_map('strval', (array) ($data['rerolls'] ?? []))),
            array_values(array_map('intval', (array) ($data['blockDice'] ?? []))),
            array_values(array_map('boolval', (array) ($data['juggernaut'] ?? []))),
            array_values(array_map('boolval', (array) ($data['pilingOn'] ?? []))),
            array_values(array_map('boolval', (array) ($data['wrestle'] ?? []))),
            array_values(array_map(static fn(mixed $p): ?array => $p === null ? null : $pole($p), (array) ($data['sideStep'] ?? []))),
        );
    }

    private function pridat(string $druh, mixed $odpoved): self
    {
        $pole = $this->toArray();
        $pole[$druh][] = $odpoved;

        return self::zPoli($pole);
    }

    /** @param array<string, list<mixed>> $pole */
    private static function zPoli(array $pole): self
    {
        return new self(...$pole);
    }
}
