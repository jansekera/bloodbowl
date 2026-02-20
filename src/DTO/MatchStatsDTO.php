<?php

declare(strict_types=1);

namespace App\DTO;

final class MatchStatsDTO
{
    public function __construct(
        private readonly int $playerId,
        private int $touchdowns = 0,
        private int $completions = 0,
        private int $interceptions = 0,
        private int $casualties = 0,
        private bool $mvp = false,
    ) {
    }

    public function getPlayerId(): int { return $this->playerId; }
    public function getTouchdowns(): int { return $this->touchdowns; }
    public function getCompletions(): int { return $this->completions; }
    public function getInterceptions(): int { return $this->interceptions; }
    public function getCasualties(): int { return $this->casualties; }
    public function isMvp(): bool { return $this->mvp; }

    public function withTouchdown(): self
    {
        $clone = clone $this;
        $clone->touchdowns++;
        return $clone;
    }

    public function withCompletion(): self
    {
        $clone = clone $this;
        $clone->completions++;
        return $clone;
    }

    public function withInterception(): self
    {
        $clone = clone $this;
        $clone->interceptions++;
        return $clone;
    }

    public function withCasualty(): self
    {
        $clone = clone $this;
        $clone->casualties++;
        return $clone;
    }

    public function withMvp(): self
    {
        $clone = clone $this;
        $clone->mvp = true;
        return $clone;
    }

    public function getSpp(): int
    {
        return ($this->touchdowns * 3)
            + ($this->completions * 1)
            + ($this->interceptions * 2)
            + ($this->casualties * 2)
            + ($this->mvp ? 5 : 0);
    }

    /**
     * @return array<string, mixed>
     */
    public function toArray(): array
    {
        return [
            'playerId' => $this->playerId,
            'touchdowns' => $this->touchdowns,
            'completions' => $this->completions,
            'interceptions' => $this->interceptions,
            'casualties' => $this->casualties,
            'mvp' => $this->mvp,
            'spp' => $this->getSpp(),
        ];
    }
}
