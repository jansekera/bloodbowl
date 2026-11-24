<?php

declare(strict_types=1);

namespace App\DTO;

/**
 * P310 (uživatel 24.11.2026: „3 ano“): Piling On jako volba kouče-člověka útočníka. `rules_bb2016.txt` ř. 8361-8371:
 * "You can use a team re-roll to re-roll the Armour roll or Injury roll. Then the Piling On player is Placed Prone".
 * `armour` = brnění sraženého `defenderId` vydrželo (přehodit brnění), jinak padlo jen Stunned (přehodit zranění).
 *
 * Stav s touto otázkou je okamžik otázky (jen k zobrazení); po odpovědi se akce přehraje z výchozího stavu
 * (`replay`, jako PendingJuggernautDTO).
 */
final class PendingPilingOnDTO
{
    public function __construct(
        private readonly int $playerId,
        private readonly int $defenderId,
        private readonly bool $armour,
        private readonly ReplayContext $replay,
    ) {}

    public function getPlayerId(): int
    {
        return $this->playerId;
    }

    public function getDefenderId(): int
    {
        return $this->defenderId;
    }

    public function isArmour(): bool
    {
        return $this->armour;
    }

    public function getReplay(): ReplayContext
    {
        return $this->replay;
    }

    /**
     * @param bool $sPrehranim false = pro klienta (P306 E1: přehrání potřebuje jen server)
     * @return array<string, mixed>
     */
    public function toArray(bool $sPrehranim = true): array
    {
        return [
            'playerId' => $this->playerId,
            'defenderId' => $this->defenderId,
            'armour' => $this->armour,
        ] + ($sPrehranim ? ['replay' => $this->replay->toArray()] : []);
    }

    /** @param array<string, mixed> $data */
    public static function fromArray(array $data): self
    {
        return new self(
            (int) $data['playerId'],
            (int) $data['defenderId'],
            (bool) $data['armour'],
            ReplayContext::fromArray((array) $data['replay']),
        );
    }
}
