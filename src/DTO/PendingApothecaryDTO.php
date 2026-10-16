<?php

declare(strict_types=1);

namespace App\DTO;

/**
 * P206 (b), 16.10.2026 (uživatel: „lékárník se má ptát hráče na vše“). Čekající volba kouče o lékárníkovi
 * po akci, ve které se jeho hráč zranil (`rules_bb2016.txt` ř. 1200-1210).
 *
 *  - stage 'offer': komu lékárníka použít (`offers`: hráči z této akce, KO i Casualty), nebo nikomu;
 *  - stage 'choose': u Casualty soupeř hodil znovu — kouč vybere jeden ze dvou výsledků (`options`).
 *
 * `turnover`: akce, ve které ke zranění došlo, skončila turnoverem — platí až po volbě.
 */
final class PendingApothecaryDTO
{
    /**
     * @param list<array{playerId: int, kind: string, result: ?string, x: ?int, y: ?int, regeneration: bool}> $offers
     * @param list<string> $options
     */
    public function __construct(
        private readonly array $offers,
        private readonly bool $turnover,
        private readonly string $stage = 'offer',
        private readonly ?int $chosenPlayerId = null,
        private readonly array $options = [],
    ) {}

    /** @return list<array{playerId: int, kind: string, result: ?string, x: ?int, y: ?int, regeneration: bool}> */
    public function getOffers(): array
    {
        return $this->offers;
    }

    public function isTurnover(): bool
    {
        return $this->turnover;
    }

    public function getStage(): string
    {
        return $this->stage;
    }

    public function getChosenPlayerId(): ?int
    {
        return $this->chosenPlayerId;
    }

    /** @return list<string> */
    public function getOptions(): array
    {
        return $this->options;
    }

    /** @return array{playerId: int, kind: string, result: ?string, x: ?int, y: ?int, regeneration: bool}|null */
    public function offerFor(int $playerId): ?array
    {
        foreach ($this->offers as $o) {
            if ($o['playerId'] === $playerId) {
                return $o;
            }
        }
        return null;
    }

    /** @param list<string> $options */
    public function withChoice(int $playerId, array $options): self
    {
        return new self($this->offers, $this->turnover, 'choose', $playerId, $options);
    }

    /** @return array<string, mixed> */
    public function toArray(): array
    {
        return [
            'offers' => $this->offers,
            'turnover' => $this->turnover,
            'stage' => $this->stage,
            'chosenPlayerId' => $this->chosenPlayerId,
            'options' => $this->options,
        ];
    }

    /** @param array<string, mixed> $data */
    public static function fromArray(array $data): self
    {
        $offers = [];
        foreach ((array) ($data['offers'] ?? []) as $o) {
            $o = (array) $o;
            $offers[] = [
                'playerId' => (int) $o['playerId'],
                'kind' => (string) $o['kind'],
                'result' => isset($o['result']) ? (string) $o['result'] : null,
                'x' => isset($o['x']) ? (int) $o['x'] : null,
                'y' => isset($o['y']) ? (int) $o['y'] : null,
                'regeneration' => (bool) ($o['regeneration'] ?? false),
            ];
        }
        return new self(
            $offers,
            (bool) ($data['turnover'] ?? false),
            (string) ($data['stage'] ?? 'offer'),
            isset($data['chosenPlayerId']) ? (int) $data['chosenPlayerId'] : null,
            array_values(array_map('strval', (array) ($data['options'] ?? []))),
        );
    }
}
