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
        private readonly bool $brawlerAvailable,
        private readonly bool $proAvailable,
        private readonly bool $teamRerollAvailable,
        private readonly bool $rerollUsed = false,
    ) {
    }

    public function getAttackerId(): int { return $this->attackerId; }
    public function getDefenderId(): int { return $this->defenderId; }
    /** @return list<BlockDiceFace> */
    public function getFaces(): array { return $this->faces; }
    public function isAttackerChooses(): bool { return $this->attackerChooses; }
    public function isBlitz(): bool { return $this->isBlitz; }
    public function isFrenzy(): bool { return $this->isFrenzy; }
    public function isBrawlerAvailable(): bool { return $this->brawlerAvailable; }
    public function isProAvailable(): bool { return $this->proAvailable; }
    public function isTeamRerollAvailable(): bool { return $this->teamRerollAvailable; }
    public function isRerollUsed(): bool { return $this->rerollUsed; }

    /** @param list<BlockDiceFace> $faces */
    public function withFaces(array $faces): self
    {
        return new self(
            $this->attackerId, $this->defenderId, $faces,
            $this->attackerChooses, $this->isBlitz, $this->isFrenzy,
            $this->brawlerAvailable, $this->proAvailable, $this->teamRerollAvailable,
            $this->rerollUsed,
        );
    }

    public function withRerollUsed(): self
    {
        return new self(
            $this->attackerId, $this->defenderId, $this->faces,
            $this->attackerChooses, $this->isBlitz, $this->isFrenzy,
            false, false, false, true,
        );
    }

    public function withBrawlerUsed(): self
    {
        return new self(
            $this->attackerId, $this->defenderId, $this->faces,
            $this->attackerChooses, $this->isBlitz, $this->isFrenzy,
            false, $this->proAvailable, $this->teamRerollAvailable,
            $this->rerollUsed,
        );
    }

    public function withProUsed(): self
    {
        return new self(
            $this->attackerId, $this->defenderId, $this->faces,
            $this->attackerChooses, $this->isBlitz, $this->isFrenzy,
            $this->brawlerAvailable, false, $this->teamRerollAvailable,
            $this->rerollUsed,
        );
    }

    public function withTeamRerollUsed(): self
    {
        return new self(
            $this->attackerId, $this->defenderId, $this->faces,
            $this->attackerChooses, $this->isBlitz, $this->isFrenzy,
            $this->brawlerAvailable, $this->proAvailable, false,
            true,
        );
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
            'brawlerAvailable' => $this->brawlerAvailable,
            'proAvailable' => $this->proAvailable,
            'teamRerollAvailable' => $this->teamRerollAvailable,
            'rerollUsed' => $this->rerollUsed,
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
            brawlerAvailable: (bool) ($data['brawlerAvailable'] ?? false),
            proAvailable: (bool) ($data['proAvailable'] ?? false),
            teamRerollAvailable: (bool) ($data['teamRerollAvailable'] ?? false),
            rerollUsed: (bool) ($data['rerollUsed'] ?? false),
        );
    }
}
