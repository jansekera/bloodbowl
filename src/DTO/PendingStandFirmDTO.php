<?php

declare(strict_types=1);

namespace App\DTO;

use App\ValueObject\Position;

/**
 * P226 (21.10.2026), parita s C++ P130 (`USE_STAND_FIRM`). Čekající otázka kouče-člověka, jestli
 * jeho hráč `playerId` použije Stand Firm (`rules_bb2016.txt` ř. 8510-8516: "may choose to not be
 * pushed back … If a player is pushed back into a player with using Stand Firm then neither player
 * moves"). Ptá se přímo blokovaného obránce i hráče v řetězovém odtlačení -- vždy jen toho, na
 * kterém odtlačení právě závisí.
 *
 * Otázka přeruší blok z akce Block/Blitz (`PendingBlockDTO` dál čeká) dřív, než se cokoli pohne
 * nebo hodí; po odpovědi se blok vyhodnotí znovu od zvolené kostky (`faceIndex`) se všemi
 * dosavadními odpověďmi (`answers`). `followUp` je volba útočníka poslaná s kostkou, `grab`
 * odpověď útočníka na Grab (P224: pole, false = ne, null = neptal se).
 */
final class PendingStandFirmDTO
{
    /** @param array<int, bool> $answers id hráče => použije Stand Firm */
    public function __construct(
        private readonly int $playerId,
        private readonly int $attackerId,
        private readonly int $defenderId,
        private readonly int $faceIndex,
        private readonly array $answers = [],
        private readonly ?bool $followUp = null,
        private readonly Position|false|null $grab = null,
    ) {}

    public function getPlayerId(): int
    {
        return $this->playerId;
    }

    public function getAttackerId(): int
    {
        return $this->attackerId;
    }

    public function getDefenderId(): int
    {
        return $this->defenderId;
    }

    public function getFaceIndex(): int
    {
        return $this->faceIndex;
    }

    /** @return array<int, bool> */
    public function getAnswers(): array
    {
        return $this->answers;
    }

    public function getFollowUp(): ?bool
    {
        return $this->followUp;
    }

    public function getGrab(): Position|false|null
    {
        return $this->grab;
    }

    /** @return array<string, mixed> */
    public function toArray(): array
    {
        $answers = [];
        foreach ($this->answers as $id => $use) {
            $answers[] = ['playerId' => $id, 'use' => $use];
        }

        return [
            'playerId' => $this->playerId,
            'attackerId' => $this->attackerId,
            'defenderId' => $this->defenderId,
            'faceIndex' => $this->faceIndex,
            'answers' => $answers,
            'followUp' => $this->followUp,
            'grab' => $this->grab instanceof Position ? ['x' => $this->grab->getX(), 'y' => $this->grab->getY()] : $this->grab,
        ];
    }

    /** @param array<string, mixed> $data */
    public static function fromArray(array $data): self
    {
        $answers = [];
        foreach ((array) ($data['answers'] ?? []) as $a) {
            $a = (array) $a;
            $answers[(int) $a['playerId']] = (bool) $a['use'];
        }
        $grab = $data['grab'] ?? null;
        if (is_array($grab) || is_object($grab)) {
            $grab = (array) $grab;
            $grab = new Position((int) $grab['x'], (int) $grab['y']);
        } elseif ($grab !== null) {
            $grab = false;
        }

        return new self(
            (int) $data['playerId'],
            (int) $data['attackerId'],
            (int) $data['defenderId'],
            (int) $data['faceIndex'],
            $answers,
            isset($data['followUp']) ? (bool) $data['followUp'] : null,
            $grab,
        );
    }
}
