<?php

declare(strict_types=1);

namespace App\DTO;

use App\Enum\BlockDiceFace;

/**
 * P308 (uživatel 20.11.2026: „ano“): AI blokuje hráče kouče-člověka a kostku vybírá obránce — `rules_bb2016.txt`
 * ř. 633-634: "The coach of the stronger player picks which block dice is used." Kouč volí jednu z `faces`
 * (`ActionType::RESOLVE_BLOCK_DIE`, `faceIndex`).
 *
 * Stav s touto otázkou je okamžik otázky (jen k zobrazení; P294 5a): po odpovědi se akce AI přehraje z výchozího
 * stavu (`replay->base`) se stejnými kostkami a odpověďmi (`ReplayContext`, `ReplayAnswers::$blockDice`).
 */
final class PendingBlockDieDTO
{
    /** @param list<BlockDiceFace> $faces */
    public function __construct(
        private readonly int $attackerId,
        private readonly int $defenderId,
        private readonly array $faces,
        private readonly bool $isFrenzy,
        private readonly ReplayContext $replay,
    ) {}

    public function getAttackerId(): int
    {
        return $this->attackerId;
    }

    public function getDefenderId(): int
    {
        return $this->defenderId;
    }

    /** @return list<BlockDiceFace> */
    public function getFaces(): array
    {
        return $this->faces;
    }

    public function isFrenzy(): bool
    {
        return $this->isFrenzy;
    }

    public function getReplay(): ReplayContext
    {
        return $this->replay;
    }

    public function isFaceIndex(mixed $faceIndex): bool
    {
        return is_int($faceIndex) && $faceIndex >= 0 && $faceIndex < count($this->faces);
    }

    /**
     * @param bool $sPrehranim false = pro klienta (P306 E1: přehrání potřebuje jen server)
     * @return array<string, mixed>
     */
    public function toArray(bool $sPrehranim = true): array
    {
        return [
            'attackerId' => $this->attackerId,
            'defenderId' => $this->defenderId,
            'faces' => array_map(static fn(BlockDiceFace $f): string => $f->value, $this->faces),
            'isFrenzy' => $this->isFrenzy,
        ] + ($sPrehranim ? ['replay' => $this->replay->toArray()] : []);
    }

    /** @param array<string, mixed> $data */
    public static function fromArray(array $data): self
    {
        return new self(
            (int) $data['attackerId'],
            (int) $data['defenderId'],
            array_values(array_map(static fn(mixed $f): BlockDiceFace => BlockDiceFace::from((string) $f), (array) $data['faces'])),
            (bool) $data['isFrenzy'],
            ReplayContext::fromArray((array) $data['replay']),
        );
    }
}
