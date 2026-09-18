<?php
declare(strict_types=1);

namespace App\DTO;

use App\Enum\BlockDiceFace;

final class PendingBlockDTO
{
    /**
     * @param list<BlockDiceFace> $faces
     */
    public function __construct(
        private readonly int $attackerId,
        private readonly int $defenderId,
        private readonly array $faces,
        private readonly bool $attackerChooses,
        private readonly bool $isBlitz,
        private readonly bool $isFrenzy,
        private readonly bool $proAvailable,
        private readonly bool $teamRerollAvailable,
        private readonly bool $rerollUsed = false,
        // ⭐ 18.09.2026: hod Pro padl 1-3 => kostky plati a tymovy prehoz smi
        //   prehodit uz jen HOD PRO (`rules_bb2016.txt` r. 8385-8387).
        private readonly bool $proFailed = false,
    ) {
    }

    public function getAttackerId(): int { return $this->attackerId; }
    public function getDefenderId(): int { return $this->defenderId; }
    /** @return list<BlockDiceFace> */
    public function getFaces(): array { return $this->faces; }
    public function isAttackerChooses(): bool { return $this->attackerChooses; }
    public function isBlitz(): bool { return $this->isBlitz; }
    public function isFrenzy(): bool { return $this->isFrenzy; }
    public function isProAvailable(): bool { return $this->proAvailable; }
    public function isTeamRerollAvailable(): bool { return $this->teamRerollAvailable; }
    public function isRerollUsed(): bool { return $this->rerollUsed; }
    public function isProFailed(): bool { return $this->proFailed; }

    /** @param list<BlockDiceFace> $faces */
    public function withFaces(array $faces): self
    {
        return new self(
            $this->attackerId, $this->defenderId, $faces,
            $this->attackerChooses, $this->isBlitz, $this->isFrenzy,
            $this->proAvailable, $this->teamRerollAvailable,
            $this->rerollUsed, $this->proFailed,
        );
    }

    public function withRerollUsed(): self
    {
        return new self(
            $this->attackerId, $this->defenderId, $this->faces,
            $this->attackerChooses, $this->isBlitz, $this->isFrenzy,
            false, false, true,
        );
    }

    public function withProFailed(): self
    {
        return new self(
            $this->attackerId, $this->defenderId, $this->faces,
            $this->attackerChooses, $this->isBlitz, $this->isFrenzy,
            false, $this->teamRerollAvailable,
            $this->rerollUsed, true,
        );
    }

    /**
     * ⛔ OPRAVA 18.09.2026: po tymovem prehozu uz ani Pro (r. 926: kostka se
     *   prehazuje nejvys jednou). Driv `proAvailable` zustal.
     */
    public function withTeamRerollUsed(): self
    {
        return $this->withRerollUsed();
    }

    /**
     * @return array<string, mixed>
     */
    public function toArray(): array
    {
        return [
            'attackerId' => $this->attackerId,
            'defenderId' => $this->defenderId,
            'faces' => array_map(fn(BlockDiceFace $f) => $f->value, $this->faces),
            'attackerChooses' => $this->attackerChooses,
            'isBlitz' => $this->isBlitz,
            'isFrenzy' => $this->isFrenzy,
            'proAvailable' => $this->proAvailable,
            'teamRerollAvailable' => $this->teamRerollAvailable,
            'rerollUsed' => $this->rerollUsed,
            'proFailed' => $this->proFailed,
        ];
    }

    /**
     * @param array<string, mixed> $data
     */
    public static function fromArray(array $data): self
    {
        return new self(
            attackerId: (int) $data['attackerId'],
            defenderId: (int) $data['defenderId'],
            faces: array_values(array_map(fn(string $f) => BlockDiceFace::from($f), (array) $data['faces'])),
            attackerChooses: (bool) $data['attackerChooses'],
            isBlitz: (bool) ($data['isBlitz'] ?? false),
            isFrenzy: (bool) ($data['isFrenzy'] ?? false),
            proAvailable: (bool) ($data['proAvailable'] ?? false),
            teamRerollAvailable: (bool) ($data['teamRerollAvailable'] ?? false),
            rerollUsed: (bool) ($data['rerollUsed'] ?? false),
            proFailed: (bool) ($data['proFailed'] ?? false),
        );
    }
}
