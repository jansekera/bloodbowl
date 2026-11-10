<?php

declare(strict_types=1);

namespace App\DTO;

/**
 * P292 (10.11.2026): odpovědi kouče-člověka z dřívějších přerušení TÉŽE akce, všech druhů otázek najednou. Akce se
 * po každé odpovědi přehraje od začátku se zapsanými hody (`ReplayDiceRoller`) a otázky dostanou odpovědi v pořadí.
 * Dřív si každá otázka nesla jen své odpovědi ⇒ akce se dvěma druhy otázek (přehoz přihrávky, pak Diving Catch)
 * by po druhé odpovědi znovu položila první otázku (review 10.11., P293 bod 4).
 */
final class ReplayAnswers
{
    /**
     * @param list<bool> $divingCatch Diving Catch ano / ne
     * @param list<array{x: int, y: int}> $bombTargets kam hodit chycenou bombu (P272)
     * @param list<array{bribe: bool, argue: bool}> $referee úplatek / protest u faulu (P291)
     * @param list<string> $rerolls přehoz hodu: 'pro' | 'team_reroll' | 'decline' (P292)
     */
    public function __construct(
        public readonly array $divingCatch = [],
        public readonly array $bombTargets = [],
        public readonly array $referee = [],
        public readonly array $rerolls = [],
    ) {}

    public function withDivingCatch(bool $use): self
    {
        return new self([...$this->divingCatch, $use], $this->bombTargets, $this->referee, $this->rerolls);
    }

    /** @param array{x: int, y: int} $target */
    public function withBombTarget(array $target): self
    {
        return new self($this->divingCatch, [...$this->bombTargets, $target], $this->referee, $this->rerolls);
    }

    /** @param array{bribe: bool, argue: bool} $choice */
    public function withReferee(array $choice): self
    {
        return new self($this->divingCatch, $this->bombTargets, [...$this->referee, $choice], $this->rerolls);
    }

    public function withReroll(string $choice): self
    {
        return new self($this->divingCatch, $this->bombTargets, $this->referee, [...$this->rerolls, $choice]);
    }

    /** @return array<string, mixed> */
    public function toArray(): array
    {
        return [
            'divingCatch' => $this->divingCatch,
            'bombTargets' => $this->bombTargets,
            'referee' => $this->referee,
            'rerolls' => $this->rerolls,
        ];
    }

    /** @param array<string, mixed> $data */
    public static function fromArray(array $data): self
    {
        return new self(
            array_values(array_map('boolval', (array) ($data['divingCatch'] ?? []))),
            array_values(array_map(
                static fn(mixed $p): array => ['x' => (int) ((array) $p)['x'], 'y' => (int) ((array) $p)['y']],
                (array) ($data['bombTargets'] ?? []),
            )),
            array_values(array_map(
                static fn(mixed $a): array => ['bribe' => (bool) ((array) $a)['bribe'], 'argue' => (bool) ((array) $a)['argue']],
                (array) ($data['referee'] ?? []),
            )),
            array_values(array_map('strval', (array) ($data['rerolls'] ?? []))),
        );
    }
}
