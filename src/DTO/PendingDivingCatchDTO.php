<?php

declare(strict_types=1);

namespace App\DTO;

use App\Enum\ActionType;
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
 * Stav s touto otázkou je stav PŘED akcí `action` (`params`): po odpovědi se akce přehraje se
 * zapsanými hody `dice` a odpověďmi `answers` + novou (viz `ReplayDiceRoller`).
 */
final class PendingDivingCatchDTO
{
    /**
     * @param array<string, mixed> $params
     * @param list<int> $dice
     * @param list<bool> $answers
     */
    public function __construct(
        private readonly int $playerId,
        private readonly Position $landing,
        private readonly ActionType $action,
        private readonly array $params,
        private readonly array $dice,
        private readonly array $answers,
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

    public function getAction(): ActionType
    {
        return $this->action;
    }

    /** @return array<string, mixed> */
    public function getParams(): array
    {
        return $this->params;
    }

    /** @return list<int> */
    public function getDice(): array
    {
        return $this->dice;
    }

    /** @return list<bool> */
    public function getAnswers(): array
    {
        return $this->answers;
    }

    public function getOtherCandidates(): int
    {
        return $this->otherCandidates;
    }

    public function getAttemptingId(): ?int
    {
        return $this->attemptingId;
    }

    /** @return array<string, mixed> */
    public function toArray(): array
    {
        return [
            'playerId' => $this->playerId,
            'landing' => ['x' => $this->landing->getX(), 'y' => $this->landing->getY()],
            'action' => $this->action->value,
            'params' => $this->params,
            'dice' => $this->dice,
            'answers' => $this->answers,
            'otherCandidates' => $this->otherCandidates,
            'attemptingId' => $this->attemptingId,
        ];
    }

    /** @param array<string, mixed> $data */
    public static function fromArray(array $data): self
    {
        $landing = (array) $data['landing'];

        return new self(
            (int) $data['playerId'],
            new Position((int) $landing['x'], (int) $landing['y']),
            ActionType::from((string) $data['action']),
            (array) ($data['params'] ?? []),
            array_values(array_map('intval', (array) ($data['dice'] ?? []))),
            array_values(array_map('boolval', (array) ($data['answers'] ?? []))),
            (int) ($data['otherCandidates'] ?? 0),
            isset($data['attemptingId']) ? (int) $data['attemptingId'] : null,
        );
    }
}
