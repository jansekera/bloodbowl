<?php

declare(strict_types=1);

namespace App\DTO;

/**
 * P309 (uživatel 20.11.2026): Juggernaut jako volba. `rules_bb2016.txt` ř. 8191-8195: "If this player takes a Blitz
 * Action, then opposing players may not use their Fend, Stand Firm or Wrestle skills against blocks, and he may choose
 * to treat a 'Both Down' result as if a 'Pushed' result has been rolled instead." Ptá se kouče-člověka útočníka
 * `playerId`, který v blitzu proti `defenderId` zvolil (nebo dostal) Both Down.
 *
 * Stav s touto otázkou je okamžik otázky (jen k zobrazení); po odpovědi se akce přehraje z výchozího stavu
 * (`replay`, jako PendingDivingCatchDTO).
 */
final class PendingJuggernautDTO
{
    public function __construct(
        private readonly int $playerId,
        private readonly int $defenderId,
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
        ] + ($sPrehranim ? ['replay' => $this->replay->toArray()] : []);
    }

    /** @param array<string, mixed> $data */
    public static function fromArray(array $data): self
    {
        return new self(
            (int) $data['playerId'],
            (int) $data['defenderId'],
            ReplayContext::fromArray((array) $data['replay']),
        );
    }
}
