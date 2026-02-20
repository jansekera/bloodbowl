<?php
declare(strict_types=1);

namespace App\DTO;

use App\Enum\GamePhase;
use App\Enum\TeamSide;
use App\Enum\Weather;
use App\ValueObject\Position;

final class GameState
{
    /**
     * @param array<int, MatchPlayerDTO> $players keyed by match player id
     */
    public function __construct(
        private readonly int $matchId,
        private int $half,
        private GamePhase $phase,
        private TeamSide $activeTeam,
        private TeamStateDTO $homeTeam,
        private TeamStateDTO $awayTeam,
        private array $players,
        private BallState $ball,
        private bool $turnoverPending,
        private ?TeamSide $kickingTeam,
        private ?TeamSide $aiTeam = null,
        private Weather $weather = Weather::NICE,
    ) {
    }

    /**
     * @param array<int, MatchPlayerDTO> $players
     */
    public static function create(
        int $matchId,
        TeamStateDTO $homeTeam,
        TeamStateDTO $awayTeam,
        array $players,
        TeamSide $receivingTeam,
    ): self {
        return new self(
            matchId: $matchId,
            half: 1,
            phase: GamePhase::SETUP,
            activeTeam: $receivingTeam,
            homeTeam: $homeTeam,
            awayTeam: $awayTeam,
            players: $players,
            ball: BallState::offPitch(),
            turnoverPending: false,
            kickingTeam: $receivingTeam->opponent(),
        );
    }

    public function getMatchId(): int { return $this->matchId; }
    public function getHalf(): int { return $this->half; }
    public function getPhase(): GamePhase { return $this->phase; }
    public function getActiveTeam(): TeamSide { return $this->activeTeam; }
    public function getHomeTeam(): TeamStateDTO { return $this->homeTeam; }
    public function getAwayTeam(): TeamStateDTO { return $this->awayTeam; }
    public function getBall(): BallState { return $this->ball; }
    public function isTurnoverPending(): bool { return $this->turnoverPending; }
    public function getKickingTeam(): ?TeamSide { return $this->kickingTeam; }
    public function getAiTeam(): ?TeamSide { return $this->aiTeam; }
    public function getWeather(): Weather { return $this->weather; }

    public function getTeamState(TeamSide $side): TeamStateDTO
    {
        return $side === TeamSide::HOME ? $this->homeTeam : $this->awayTeam;
    }

    /**
     * @return array<int, MatchPlayerDTO>
     */
    public function getPlayers(): array
    {
        return $this->players;
    }

    public function getPlayer(int $id): ?MatchPlayerDTO
    {
        return $this->players[$id] ?? null;
    }

    public function getPlayerAtPosition(Position $pos): ?MatchPlayerDTO
    {
        foreach ($this->players as $player) {
            $playerPos = $player->getPosition();
            if ($playerPos !== null && $playerPos->equals($pos)) {
                return $player;
            }
        }
        return null;
    }

    /**
     * @return list<MatchPlayerDTO>
     */
    public function getTeamPlayers(TeamSide $side): array
    {
        return array_values(array_filter(
            $this->players,
            fn(MatchPlayerDTO $p) => $p->getTeamSide() === $side,
        ));
    }

    /**
     * @return list<MatchPlayerDTO>
     */
    public function getPlayersOnPitch(TeamSide $side): array
    {
        return array_values(array_filter(
            $this->players,
            fn(MatchPlayerDTO $p) => $p->getTeamSide() === $side && $p->getState()->isOnPitch(),
        ));
    }

    // --- Wither methods ---

    public function withPhase(GamePhase $phase): self
    {
        $clone = clone $this;
        $clone->phase = $phase;
        return $clone;
    }

    public function withActiveTeam(TeamSide $side): self
    {
        $clone = clone $this;
        $clone->activeTeam = $side;
        return $clone;
    }

    public function withHalf(int $half): self
    {
        $clone = clone $this;
        $clone->half = $half;
        return $clone;
    }

    public function withBall(BallState $ball): self
    {
        $clone = clone $this;
        $clone->ball = $ball;
        return $clone;
    }

    public function withPlayer(MatchPlayerDTO $player): self
    {
        $clone = clone $this;
        $clone->players[$player->getId()] = $player;
        return $clone;
    }

    public function withHomeTeam(TeamStateDTO $team): self
    {
        $clone = clone $this;
        $clone->homeTeam = $team;
        return $clone;
    }

    public function withAwayTeam(TeamStateDTO $team): self
    {
        $clone = clone $this;
        $clone->awayTeam = $team;
        return $clone;
    }

    public function withTeamState(TeamSide $side, TeamStateDTO $team): self
    {
        return $side === TeamSide::HOME
            ? $this->withHomeTeam($team)
            : $this->withAwayTeam($team);
    }

    public function withTurnoverPending(bool $pending): self
    {
        $clone = clone $this;
        $clone->turnoverPending = $pending;
        return $clone;
    }

    public function withKickingTeam(?TeamSide $kickingTeam): self
    {
        $clone = clone $this;
        $clone->kickingTeam = $kickingTeam;
        return $clone;
    }

    public function withAiTeam(?TeamSide $aiTeam): self
    {
        $clone = clone $this;
        $clone->aiTeam = $aiTeam;
        return $clone;
    }

    public function withWeather(Weather $weather): self
    {
        $clone = clone $this;
        $clone->weather = $weather;
        return $clone;
    }

    /**
     * Reset all players' hasMoved/hasActed for a new turn.
     */
    public function resetPlayersForNewTurn(TeamSide $side): self
    {
        $clone = clone $this;
        foreach ($clone->players as $id => $player) {
            if ($player->getTeamSide() === $side) {
                $clone->players[$id] = $player
                    ->withHasMoved(false)
                    ->withHasActed(false)
                    ->withMovementRemaining($player->getStats()->getMovement())
                    ->withLostTacklezones(false)
                    ->withProUsedThisTurn(false);

                // Recover stunned players
                if ($player->getState() === \App\Enum\PlayerState::STUNNED) {
                    $clone->players[$id] = $clone->players[$id]->withState(\App\Enum\PlayerState::PRONE);
                }
            }
        }
        return $clone;
    }

    /**
     * @return array<string, mixed>
     */
    public function toArray(): array
    {
        $playersArray = [];
        foreach ($this->players as $player) {
            $playersArray[] = $player->toArray();
        }

        return [
            'matchId' => $this->matchId,
            'half' => $this->half,
            'phase' => $this->phase->value,
            'activeTeam' => $this->activeTeam->value,
            'homeTeam' => $this->homeTeam->toArray(),
            'awayTeam' => $this->awayTeam->toArray(),
            'players' => $playersArray,
            'ball' => $this->ball->toArray(),
            'turnoverPending' => $this->turnoverPending,
            'kickingTeam' => $this->kickingTeam?->value,
            'aiTeam' => $this->aiTeam?->value,
            'weather' => $this->weather->value,
        ];
    }

    /**
     * @param array<string, mixed> $data
     */
    public static function fromArray(array $data): self
    {
        $players = [];
        foreach ((array) $data['players'] as $playerData) {
            $player = MatchPlayerDTO::fromArray($playerData);
            $players[$player->getId()] = $player;
        }

        return new self(
            matchId: (int) $data['matchId'],
            half: (int) $data['half'],
            phase: GamePhase::from((string) $data['phase']),
            activeTeam: TeamSide::from((string) $data['activeTeam']),
            homeTeam: TeamStateDTO::fromArray((array) $data['homeTeam']),
            awayTeam: TeamStateDTO::fromArray((array) $data['awayTeam']),
            players: $players,
            ball: BallState::fromArray((array) $data['ball']),
            turnoverPending: (bool) $data['turnoverPending'],
            kickingTeam: isset($data['kickingTeam']) ? TeamSide::from((string) $data['kickingTeam']) : null,
            aiTeam: isset($data['aiTeam']) ? TeamSide::from((string) $data['aiTeam']) : null,
            weather: Weather::from((string) ($data['weather'] ?? 'nice')),
        );
    }
}
