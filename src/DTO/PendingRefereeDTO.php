<?php

declare(strict_types=1);

namespace App\DTO;

use App\Enum\ActionType;

/**
 * P291 (uživatel 10.11.2026: „udělej ať se zeptá — člověk může některého hráče preferovat, ať zůstane“).
 * Rozhodčí vylučuje hráče kouče-člověka `playerId`; kouč volí úplatek (`rules_bb2016.txt` ř. 2871-2879) a protest
 * Argue the call (ř. 1091-1102). Ptá se jen na to, co jde (`canBribe`, `canArgue`).
 *
 * - Faul (`foul`): stav je stav PŘED akcí `action` (`params`); po odpovědi se akce přehraje se zapsanými hody
 *   `dice` a odpověďmi `answers` + novou (jako PendingDivingCatchDTO).
 * - Tajná zbraň na konci drive: `action` je null, odpověď se vyhodnotí hned; `queue` = další hráči kouče
 *   s tajnou zbraní, na které rozhodčí ještě čeká.
 */
final class PendingRefereeDTO
{
    /**
     * @param array<string, mixed> $params
     * @param list<int> $dice
     * @param list<array{bribe: bool, argue: bool}> $answers
     * @param list<int> $queue
     */
    public function __construct(
        private readonly int $playerId,
        private readonly bool $foul,
        private readonly bool $canBribe,
        private readonly bool $canArgue,
        private readonly ?ActionType $action = null,
        private readonly array $params = [],
        private readonly array $dice = [],
        private readonly array $answers = [],
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

    public function getAction(): ?ActionType
    {
        return $this->action;
    }

    /** @return array<string, mixed> */
    public function getParams(): array
    {
        return $this->params;
    }

    /** @return list<int> */
    public function getDice(): array
    {
        return $this->dice;
    }

    /** @return list<array{bribe: bool, argue: bool}> */
    public function getAnswers(): array
    {
        return $this->answers;
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
            'action' => $this->action?->value,
            'params' => $this->params,
            'dice' => $this->dice,
            'answers' => $this->answers,
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
            isset($data['action']) ? ActionType::from((string) $data['action']) : null,
            (array) ($data['params'] ?? []),
            array_values(array_map('intval', (array) ($data['dice'] ?? []))),
            array_values(array_map(
                static fn(mixed $a): array => ['bribe' => (bool) ((array) $a)['bribe'], 'argue' => (bool) ((array) $a)['argue']],
                (array) ($data['answers'] ?? []),
            )),
            array_values(array_map('intval', (array) ($data['queue'] ?? []))),
        );
    }
}
