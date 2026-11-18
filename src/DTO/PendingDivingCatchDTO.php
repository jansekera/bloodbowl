<?php

declare(strict_types=1);

namespace App\DTO;

use App\ValueObject\Position;

/**
 * Diving Catch jako otázka kouči (uživatel 30.10.2026). `rules_bb2016.txt` ř. 8065-8071: hráč "can
 * attempt to catch any pass, kick off or crowd throw-in, but not bouncing ball, that would land in an
 * empty square in one of his tackle zones" — skill je volba (ř. 1820 "Skill use is not mandatory").
 * Ptá se kouče-člověka hráče `playerId` (i v tahu soupeře), když míč dopadá na `landing`. Varianta B
 * (uživatel 30.10.2026): víc kandidátů ⇒ otázka na každého, napřed strana na tahu (ř. 986-990);
 * `otherCandidates` = počet dalších kandidátů, `attemptingId` = kdo už skáče (null = nikdo). Dva
 * a víc pokusů se ruší (ř. 8069-8071).
 *
 * Stav s touto otázkou je okamžik otázky (rozpracovaná akce, jen k zobrazení; P294 5a): po odpovědi se akce přehraje
 * z výchozího stavu (`replay->base`) (`replay`: akce, zapsané hody
 * a odpovědi všech druhů + nová; viz `ReplayDiceRoller`).
 */
final class PendingDivingCatchDTO
{
    public function __construct(
        private readonly int $playerId,
        private readonly Position $landing,
        private readonly ReplayContext $replay,
        private readonly int $otherCandidates = 0,
        private readonly ?int $attemptingId = null,
    ) {}

    public function getPlayerId(): int
    {
        return $this->playerId;
    }

    public function getLanding(): Position
    {
        return $this->landing;
    }

    /** P294 krok 4: akce, hody a odpovědi k přehrání */
    public function getReplay(): ReplayContext
    {
        return $this->replay;
    }

    public function getOtherCandidates(): int
    {
        return $this->otherCandidates;
    }

    public function getAttemptingId(): ?int
    {
        return $this->attemptingId;
    }

    /**
     * @param bool $sPrehranim false = pro klienta (P306 E1: přehrání potřebuje jen server)
     * @return array<string, mixed>
     */
    public function toArray(bool $sPrehranim = true): array
    {
        return [
            'playerId' => $this->playerId,
            'landing' => ['x' => $this->landing->getX(), 'y' => $this->landing->getY()],
            'otherCandidates' => $this->otherCandidates,
            'attemptingId' => $this->attemptingId,
        ] + ($sPrehranim ? ['replay' => $this->replay->toArray()] : []);
    }

    /** @param array<string, mixed> $data */
    public static function fromArray(array $data): self
    {
        $landing = (array) $data['landing'];

        return new self(
            (int) $data['playerId'],
            new Position((int) $landing['x'], (int) $landing['y']),
            ReplayContext::fromArray((array) $data['replay']),
            (int) ($data['otherCandidates'] ?? 0),
            isset($data['attemptingId']) ? (int) $data['attemptingId'] : null,
        );
    }
}
