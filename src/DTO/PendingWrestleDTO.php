<?php

declare(strict_types=1);

namespace App\DTO;

/**
 * P312 (uživatel 20.11.2026: „ano“): Wrestle jako volba kouče-člověka. `rules_bb2016.txt` ř. 8670-8676: "This player
 * may use Wrestle when he blocks or is blocked and a 'Both Down' result on the Block dice is chosen by either coach.
 * Instead of applying the 'Both Down' result, both players are wrestled to the ground." Ptá se kouče hráče
 * `playerId` (útočník nebo obránce s Wrestle) proti `opponentId`.
 *
 * Stav s touto otázkou je okamžik otázky (jen k zobrazení); po odpovědi se akce přehraje z výchozího stavu
 * (`replay`, jako PendingJuggernautDTO).
 */
final class PendingWrestleDTO
{
    public function __construct(
        private readonly int $playerId,
        private readonly int $opponentId,
        private readonly ReplayContext $replay,
    ) {}

    public function getPlayerId(): int
    {
        return $this->playerId;
    }

    public function getOpponentId(): int
    {
        return $this->opponentId;
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
            'opponentId' => $this->opponentId,
        ] + ($sPrehranim ? ['replay' => $this->replay->toArray()] : []);
    }

    /** @param array<string, mixed> $data */
    public static function fromArray(array $data): self
    {
        return new self(
            (int) $data['playerId'],
            (int) $data['opponentId'],
            ReplayContext::fromArray((array) $data['replay']),
        );
    }
}
