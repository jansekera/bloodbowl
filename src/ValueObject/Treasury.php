<?php

declare(strict_types=1);

namespace App\ValueObject;

use App\Exception\InsufficientFundsException;

final class Treasury
{
    public function __construct(
        private readonly int $gold,
    ) {
        if ($gold < 0) {
            throw new \InvalidArgumentException('Treasury cannot be negative');
        }
    }

    public function getGold(): int
    {
        return $this->gold;
    }

    public function canAfford(int $cost): bool
    {
        return $this->gold >= $cost;
    }

    public function spend(int $amount): self
    {
        if (!$this->canAfford($amount)) {
            throw new InsufficientFundsException(
                "Cannot spend {$amount}g, only {$this->gold}g available"
            );
        }

        return new self($this->gold - $amount);
    }

    public function add(int $amount): self
    {
        return new self($this->gold + $amount);
    }

    public function equals(self $other): bool
    {
        return $this->gold === $other->gold;
    }

    public function format(): string
    {
        return number_format($this->gold, 0, '.', ',') . 'g';
    }
}
