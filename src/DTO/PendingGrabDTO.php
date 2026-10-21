<?php

declare(strict_types=1);

namespace App\DTO;

use App\Enum\BlockDiceFace;
use App\ValueObject\Position;

/**
 * P224 (21.10.2026), parita s C++ P222 (`USE_GRAB`). Čekající otázka kouče-člověka, jestli útočník
 * použije Grab při odtlačení (`rules_bb2016.txt` ř. 8147-8154: "may choose any empty square
 * adjacent to his opponent"). Otázka přeruší vyhodnocení bloku těsně před odtlačením:
 *
 *  - kind 'block': blok z akce Block; čeká i `PendingBlockDTO`, `faceIndex`/`followUp` je už
 *    učiněná volba kostky a follow-upu, která se po odpovědi dokončí;
 *  - kind 'bnc': automatický blok Ball & Chain (ř. 7829-7831); `face` je zvolená strana kostky,
 *    `step` krok pohybu, ve kterém ke bloku došlo, a `newPos` pole, na kterém stál obránce.
 */
final class PendingGrabDTO
{
    /** @param list<Position> $squares volná pole vedle obránce, na která Grab smí odtlačit */
    public function __construct(
        private readonly string $kind,
        private readonly int $attackerId,
        private readonly int $defenderId,
        private readonly array $squares,
        private readonly int $faceIndex = 0,
        private readonly ?bool $followUp = null,
        private readonly ?BlockDiceFace $face = null,
        private readonly int $step = 0,
    ) {}

    public function getKind(): string
    {
        return $this->kind;
    }

    public function getAttackerId(): int
    {
        return $this->attackerId;
    }

    public function getDefenderId(): int
    {
        return $this->defenderId;
    }

    /** @return list<Position> */
    public function getSquares(): array
    {
        return $this->squares;
    }

    public function getFaceIndex(): int
    {
        return $this->faceIndex;
    }

    public function getFollowUp(): ?bool
    {
        return $this->followUp;
    }

    public function getFace(): ?BlockDiceFace
    {
        return $this->face;
    }

    public function getStep(): int
    {
        return $this->step;
    }

    public function offers(int $x, int $y): bool
    {
        foreach ($this->squares as $s) {
            if ($s->getX() === $x && $s->getY() === $y) {
                return true;
            }
        }

        return false;
    }

    /** @return array<string, mixed> */
    public function toArray(): array
    {
        return [
            'kind' => $this->kind,
            'attackerId' => $this->attackerId,
            'defenderId' => $this->defenderId,
            'squares' => array_map(static fn(Position $p) => ['x' => $p->getX(), 'y' => $p->getY()], $this->squares),
            'faceIndex' => $this->faceIndex,
            'followUp' => $this->followUp,
            'face' => $this->face?->value,
            'step' => $this->step,
        ];
    }

    /** @param array<string, mixed> $data */
    public static function fromArray(array $data): self
    {
        $squares = [];
        foreach ((array) ($data['squares'] ?? []) as $s) {
            $s = (array) $s;
            $squares[] = new Position((int) $s['x'], (int) $s['y']);
        }

        return new self(
            (string) $data['kind'],
            (int) $data['attackerId'],
            (int) $data['defenderId'],
            $squares,
            (int) ($data['faceIndex'] ?? 0),
            isset($data['followUp']) ? (bool) $data['followUp'] : null,
            isset($data['face']) ? BlockDiceFace::from((string) $data['face']) : null,
            (int) ($data['step'] ?? 0),
        );
    }
}
