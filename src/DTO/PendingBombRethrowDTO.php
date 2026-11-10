<?php

declare(strict_types=1);

namespace App\DTO;

use App\Enum\ActionType;

/**
 * P272 (uživatel 9.11.2026: „player must try to catch and if success throw — na ptaní mi z toho vyplývá — kam hodit
 * po chycení“). `rules_bb2016.txt` ř. 7959-7963: chycenou / zachycenou bombu hráč hned hodí dál. Chytání je povinné;
 * kouče-člověka hráče `playerId` se ptá, kam (i v tahu soupeře). Parita s C++ `CoachQuestion::BOMB_RETHROW_TARGET`.
 *
 * Stav s touto otázkou je stav PŘED akcí `action` (`params`): po odpovědi se akce přehraje se zapsanými hody `dice`
 * a cíli `answers` + novým (viz `ReplayDiceRoller`, jako PendingDivingCatchDTO).
 */
final class PendingBombRethrowDTO
{
    /**
     * @param list<array{x: int, y: int, range: string}> $targets legální pole hodu z pole hráče
     * @param array{x: int, y: int}|null $suggested volba enginu
     * @param array<string, mixed> $params
     * @param list<int> $dice
     * @param list<array{x: int, y: int}> $answers cíle dřívějších hodů dál v téže akci
     */
    public function __construct(
        private readonly int $playerId,
        private readonly array $targets,
        private readonly ?array $suggested,
        private readonly ActionType $action,
        private readonly array $params,
        private readonly array $dice,
        private readonly array $answers,
    ) {}

    public function getPlayerId(): int
    {
        return $this->playerId;
    }

    /** @return list<array{x: int, y: int, range: string}> */
    public function getTargets(): array
    {
        return $this->targets;
    }

    /** @return array{x: int, y: int}|null */
    public function getSuggested(): ?array
    {
        return $this->suggested;
    }

    public function getAction(): ActionType
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

    /** @return list<array{x: int, y: int}> */
    public function getAnswers(): array
    {
        return $this->answers;
    }

    public function isTarget(int $x, int $y): bool
    {
        foreach ($this->targets as $t) {
            if ($t['x'] === $x && $t['y'] === $y) {
                return true;
            }
        }
        return false;
    }

    /** @return array<string, mixed> */
    public function toArray(): array
    {
        return [
            'playerId' => $this->playerId,
            'targets' => $this->targets,
            'suggested' => $this->suggested,
            'action' => $this->action->value,
            'params' => $this->params,
            'dice' => $this->dice,
            'answers' => $this->answers,
        ];
    }

    /** @param array<string, mixed> $data */
    public static function fromArray(array $data): self
    {
        $pole = static fn(mixed $p): array => ['x' => (int) ((array) $p)['x'], 'y' => (int) ((array) $p)['y']];
        $suggested = $data['suggested'] ?? null;

        return new self(
            (int) $data['playerId'],
            array_values(array_map(
                static fn(mixed $t): array => [...$pole($t), 'range' => (string) ((array) $t)['range']],
                (array) ($data['targets'] ?? []),
            )),
            $suggested === null ? null : $pole($suggested),
            ActionType::from((string) $data['action']),
            (array) ($data['params'] ?? []),
            array_values(array_map('intval', (array) ($data['dice'] ?? []))),
            array_values(array_map($pole, (array) ($data['answers'] ?? []))),
        );
    }
}
