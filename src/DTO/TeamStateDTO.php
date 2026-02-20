<?php
declare(strict_types=1);

namespace App\DTO;

use App\Enum\TeamSide;

final class TeamStateDTO
{
    public function __construct(
        private readonly int $teamId,
        private readonly string $name,
        private readonly string $raceName,
        private readonly TeamSide $side,
        private int $score,
        private int $rerolls,
        private bool $rerollUsedThisTurn,
        private int $turnNumber,
        private bool $blitzUsedThisTurn,
        private bool $passUsedThisTurn,
        private bool $foulUsedThisTurn,
        private bool $hasApothecary = true,
        private bool $apothecaryUsed = false,
    ) {
    }

    public static function create(
        int $teamId,
        string $name,
        string $raceName,
        TeamSide $side,
        int $rerolls,
        bool $hasApothecary = true,
    ): self {
        return new self(
            teamId: $teamId,
            name: $name,
            raceName: $raceName,
            side: $side,
            score: 0,
            rerolls: $rerolls,
            rerollUsedThisTurn: false,
            turnNumber: 1,
            blitzUsedThisTurn: false,
            passUsedThisTurn: false,
            foulUsedThisTurn: false,
            hasApothecary: $hasApothecary,
            apothecaryUsed: false,
        );
    }

    public function getTeamId(): int { return $this->teamId; }
    public function getName(): string { return $this->name; }
    public function getRaceName(): string { return $this->raceName; }
    public function getSide(): TeamSide { return $this->side; }
    public function getScore(): int { return $this->score; }
    public function getRerolls(): int { return $this->rerolls; }
    public function isRerollUsedThisTurn(): bool { return $this->rerollUsedThisTurn; }
    public function getTurnNumber(): int { return $this->turnNumber; }
    public function isBlitzUsedThisTurn(): bool { return $this->blitzUsedThisTurn; }
    public function isPassUsedThisTurn(): bool { return $this->passUsedThisTurn; }
    public function isFoulUsedThisTurn(): bool { return $this->foulUsedThisTurn; }
    public function hasApothecary(): bool { return $this->hasApothecary; }
    public function isApothecaryUsed(): bool { return $this->apothecaryUsed; }

    public function canUseApothecary(): bool
    {
        return $this->hasApothecary && !$this->apothecaryUsed;
    }

    public function withApothecaryUsed(): self
    {
        $clone = clone $this;
        $clone->apothecaryUsed = true;
        return $clone;
    }

    public function canUseReroll(): bool
    {
        return $this->rerolls > 0 && !$this->rerollUsedThisTurn;
    }

    public function withScore(int $score): self
    {
        $clone = clone $this;
        $clone->score = $score;
        return $clone;
    }

    public function withRerolls(int $rerolls): self
    {
        $clone = clone $this;
        $clone->rerolls = $rerolls;
        return $clone;
    }

    public function withRerollUsed(): self
    {
        $clone = clone $this;
        $clone->rerollUsedThisTurn = true;
        $clone->rerolls--;
        return $clone;
    }

    public function withTurnNumber(int $turnNumber): self
    {
        $clone = clone $this;
        $clone->turnNumber = $turnNumber;
        return $clone;
    }

    public function withBlitzUsed(): self
    {
        $clone = clone $this;
        $clone->blitzUsedThisTurn = true;
        return $clone;
    }

    public function withPassUsed(): self
    {
        $clone = clone $this;
        $clone->passUsedThisTurn = true;
        return $clone;
    }

    public function withFoulUsed(): self
    {
        $clone = clone $this;
        $clone->foulUsedThisTurn = true;
        return $clone;
    }

    public function resetForNewTurn(): self
    {
        $clone = clone $this;
        $clone->rerollUsedThisTurn = false;
        $clone->blitzUsedThisTurn = false;
        $clone->passUsedThisTurn = false;
        $clone->foulUsedThisTurn = false;
        $clone->turnNumber++;
        return $clone;
    }

    /**
     * @return array<string, mixed>
     */
    public function toArray(): array
    {
        return [
            'teamId' => $this->teamId,
            'name' => $this->name,
            'raceName' => $this->raceName,
            'side' => $this->side->value,
            'score' => $this->score,
            'rerolls' => $this->rerolls,
            'rerollUsedThisTurn' => $this->rerollUsedThisTurn,
            'turnNumber' => $this->turnNumber,
            'blitzUsedThisTurn' => $this->blitzUsedThisTurn,
            'passUsedThisTurn' => $this->passUsedThisTurn,
            'foulUsedThisTurn' => $this->foulUsedThisTurn,
            'hasApothecary' => $this->hasApothecary,
            'apothecaryUsed' => $this->apothecaryUsed,
        ];
    }

    /**
     * @param array<string, mixed> $data
     */
    public static function fromArray(array $data): self
    {
        return new self(
            teamId: (int) $data['teamId'],
            name: (string) $data['name'],
            raceName: (string) $data['raceName'],
            side: TeamSide::from((string) $data['side']),
            score: (int) $data['score'],
            rerolls: (int) $data['rerolls'],
            rerollUsedThisTurn: (bool) $data['rerollUsedThisTurn'],
            turnNumber: (int) $data['turnNumber'],
            blitzUsedThisTurn: (bool) ($data['blitzUsedThisTurn'] ?? false),
            passUsedThisTurn: (bool) ($data['passUsedThisTurn'] ?? false),
            foulUsedThisTurn: (bool) ($data['foulUsedThisTurn'] ?? false),
            hasApothecary: (bool) ($data['hasApothecary'] ?? true),
            apothecaryUsed: (bool) ($data['apothecaryUsed'] ?? false),
        );
    }
}
