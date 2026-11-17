<?php

declare(strict_types=1);

namespace App\DTO;

/**
 * P272 (uživatel 9.11.2026: „player must try to catch and if success throw — na ptaní mi z toho vyplývá — kam hodit
 * po chycení“). `rules_bb2016.txt` ř. 7959-7963: chycenou / zachycenou bombu hráč hned hodí dál. Chytání je povinné;
 * kouče-člověka hráče `playerId` se ptá, kam (i v tahu soupeře). Parita s C++ `CoachQuestion::BOMB_RETHROW_TARGET`.
 *
 * Stav s touto otázkou je okamžik otázky (rozpracovaná akce, jen k zobrazení; P294 5a): po odpovědi se akce přehraje
 * z výchozího stavu (`replay->base`) (`replay`: zapsané hody a cíle
 * + nový; viz `ReplayDiceRoller`, jako PendingDivingCatchDTO).
 */
final class PendingBombRethrowDTO
{
    /**
     * @param list<array{x: int, y: int, range: string}> $targets legální pole hodu z pole hráče
     * @param array{x: int, y: int}|null $suggested volba enginu
     */
    public function __construct(
        private readonly int $playerId,
        private readonly array $targets,
        private readonly ?array $suggested,
        private readonly ReplayContext $replay,
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

    /** P294 krok 4: akce, hody a odpovědi k přehrání */
    public function getReplay(): ReplayContext
    {
        return $this->replay;
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
            'replay' => $this->replay->toArray(),
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
            ReplayContext::fromArray((array) $data['replay']),
        );
    }
}
