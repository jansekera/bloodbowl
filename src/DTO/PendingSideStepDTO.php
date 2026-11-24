<?php

declare(strict_types=1);

namespace App\DTO;

use App\ValueObject\Position;

/**
 * P312 (uživatel 20.11.2026: „ano“): Side Step jako volba kouče-člověka odtlačeného hráče. `rules_bb2016.txt`
 * ř. 8474-8478: "This player is an expert at stepping neatly out of the way of an attacker. To represent this
 * ability, his coach may choose to move the player to any adjacent square, not just the three squares shown on the
 * Push Back diagram. Note that the player may not use this skill if there are no open squares on the pitch adjacent
 * to this player." `squares` = volná sousední pole (nabídka), `suggested` = volba AI; odpověď „ne“ = běžné odtlačení.
 *
 * Stav s touto otázkou je okamžik otázky (jen k zobrazení); po odpovědi se akce přehraje z výchozího stavu.
 */
final class PendingSideStepDTO
{
    /** @param list<Position> $squares */
    public function __construct(
        private readonly int $playerId,
        private readonly array $squares,
        private readonly Position $suggested,
        private readonly ReplayContext $replay,
    ) {}

    public function getPlayerId(): int
    {
        return $this->playerId;
    }

    /** @return list<Position> */
    public function getSquares(): array
    {
        return $this->squares;
    }

    public function getSuggested(): Position
    {
        return $this->suggested;
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
            'squares' => array_map(static fn(Position $p): array => ['x' => $p->getX(), 'y' => $p->getY()], $this->squares),
            'suggested' => ['x' => $this->suggested->getX(), 'y' => $this->suggested->getY()],
        ] + ($sPrehranim ? ['replay' => $this->replay->toArray()] : []);
    }

    /** @param array<string, mixed> $data */
    public static function fromArray(array $data): self
    {
        $pole = static fn(mixed $p): Position => new Position((int) ((array) $p)['x'], (int) ((array) $p)['y']);

        return new self(
            (int) $data['playerId'],
            array_values(array_map($pole, (array) $data['squares'])),
            $pole($data['suggested']),
            ReplayContext::fromArray((array) $data['replay']),
        );
    }
}
