<?php

declare(strict_types=1);

namespace App\DTO;

/**
 * P291 (uživatel 10.11.2026: „udělej ať se zeptá — člověk může některého hráče preferovat, ať zůstane“).
 * Rozhodčí vylučuje hráče kouče-člověka `playerId`; kouč volí úplatek (`rules_bb2016.txt` ř. 2871-2879) a protest
 * Argue the call (ř. 1091-1102). Ptá se jen na to, co jde (`canBribe`, `canArgue`).
 *
 * - Faul (`foul`): stav je okamžik otázky (jen k zobrazení, P294 5a); po odpovědi se akce přehraje z `replay->base` (`replay`, jako PendingDivingCatchDTO).
 * - Tajná zbraň na konci drive: `replay` je null, odpověď se vyhodnotí hned; `queue` = další hráči kouče
 *   s tajnou zbraní, na které rozhodčí ještě čeká.
 */
final class PendingRefereeDTO
{
    /**
     * @param list<int> $queue
     */
    public function __construct(
        private readonly int $playerId,
        private readonly bool $foul,
        private readonly bool $canBribe,
        private readonly bool $canArgue,
        private readonly ?ReplayContext $replay = null,
        private readonly array $queue = [],
    ) {}

    public function getPlayerId(): int
    {
        return $this->playerId;
    }

    public function isFoul(): bool
    {
        return $this->foul;
    }

    public function canBribe(): bool
    {
        return $this->canBribe;
    }

    public function canArgue(): bool
    {
        return $this->canArgue;
    }

    /** P294 krok 4: přehrání faulu; null = tajná zbraň na konci drive */
    public function getReplay(): ?ReplayContext
    {
        return $this->replay;
    }

    /** @return list<int> */
    public function getQueue(): array
    {
        return $this->queue;
    }

    /** @return array<string, mixed> */
    public function toArray(): array
    {
        return [
            'playerId' => $this->playerId,
            'foul' => $this->foul,
            'canBribe' => $this->canBribe,
            'canArgue' => $this->canArgue,
            'replay' => $this->replay?->toArray(),
            'queue' => $this->queue,
        ];
    }

    /** @param array<string, mixed> $data */
    public static function fromArray(array $data): self
    {
        return new self(
            (int) $data['playerId'],
            (bool) $data['foul'],
            (bool) $data['canBribe'],
            (bool) $data['canArgue'],
            isset($data['replay']) ? ReplayContext::fromArray((array) $data['replay']) : null,
            array_values(array_map('intval', (array) ($data['queue'] ?? []))),
        );
    }
}
