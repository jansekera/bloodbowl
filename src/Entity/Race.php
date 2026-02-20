<?php

declare(strict_types=1);

namespace App\Entity;

final class Race
{
    /**
     * @param list<PositionalTemplate> $positionals
     */
    public function __construct(
        private readonly int $id,
        private readonly string $name,
        private readonly int $rerollCost,
        private readonly bool $hasApothecary,
        private readonly array $positionals = [],
    ) {
    }

    /**
     * @param array<string, mixed> $row
     */
    public static function fromRow(array $row): self
    {
        return new self(
            id: (int) $row['id'],
            name: (string) $row['name'],
            rerollCost: (int) $row['reroll_cost'],
            hasApothecary: (bool) $row['has_apothecary'],
        );
    }

    /**
     * @param list<PositionalTemplate> $positionals
     */
    public function withPositionals(array $positionals): self
    {
        return new self(
            id: $this->id,
            name: $this->name,
            rerollCost: $this->rerollCost,
            hasApothecary: $this->hasApothecary,
            positionals: $positionals,
        );
    }

    public function getId(): int
    {
        return $this->id;
    }

    public function getName(): string
    {
        return $this->name;
    }

    public function getRerollCost(): int
    {
        return $this->rerollCost;
    }

    public function hasApothecary(): bool
    {
        return $this->hasApothecary;
    }

    /**
     * @return list<PositionalTemplate>
     */
    public function getPositionals(): array
    {
        return $this->positionals;
    }

    /**
     * @return array<string, mixed>
     */
    public function toArray(): array
    {
        return [
            'id' => $this->id,
            'name' => $this->name,
            'reroll_cost' => $this->rerollCost,
            'has_apothecary' => $this->hasApothecary,
            'positionals' => array_map(
                fn(PositionalTemplate $p) => $p->toArray(),
                $this->positionals,
            ),
        ];
    }
}
