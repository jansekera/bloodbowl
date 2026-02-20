<?php
declare(strict_types=1);

namespace App\DTO;

use App\ValueObject\Position;

final class BallState
{
    public function __construct(
        private readonly ?Position $position,
        private readonly bool $isHeld,
        private readonly ?int $carrierId,
    ) {
    }

    public static function onGround(Position $position): self
    {
        return new self($position, false, null);
    }

    public static function carried(Position $position, int $carrierId): self
    {
        return new self($position, true, $carrierId);
    }

    public static function offPitch(): self
    {
        return new self(null, false, null);
    }

    public function getPosition(): ?Position { return $this->position; }
    public function isHeld(): bool { return $this->isHeld; }
    public function getCarrierId(): ?int { return $this->carrierId; }

    public function isOnPitch(): bool
    {
        return $this->position !== null;
    }

    /**
     * @return array<string, mixed>
     */
    public function toArray(): array
    {
        return [
            'position' => $this->position?->toArray(),
            'isHeld' => $this->isHeld,
            'carrierId' => $this->carrierId,
        ];
    }

    /**
     * @param array<string, mixed> $data
     */
    public static function fromArray(array $data): self
    {
        $position = isset($data['position'])
            ? new Position((int) $data['position']['x'], (int) $data['position']['y'])
            : null;

        return new self(
            position: $position,
            isHeld: (bool) $data['isHeld'],
            carrierId: isset($data['carrierId']) ? (int) $data['carrierId'] : null,
        );
    }
}
