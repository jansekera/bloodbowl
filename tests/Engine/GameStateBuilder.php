<?php
declare(strict_types=1);

namespace App\Tests\Engine;

use App\DTO\BallState;
use App\DTO\GameState;
use App\DTO\MatchPlayerDTO;
use App\DTO\TeamStateDTO;
use App\Enum\GamePhase;
use App\Enum\PlayerState;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use App\Enum\Weather;
use App\ValueObject\PlayerStats;
use App\ValueObject\Position;

/**
 * Helper to build GameState instances for testing.
 */
final class GameStateBuilder
{
    private int $matchId = 1;
    private int $half = 1;
    private GamePhase $phase = GamePhase::PLAY;
    private TeamSide $activeTeam = TeamSide::HOME;
    /** @var array<int, MatchPlayerDTO> */
    private array $players = [];
    private BallState $ball;
    private TeamStateDTO $homeTeam;
    private TeamStateDTO $awayTeam;
    private Weather $weather = Weather::NICE;
    private int $nextPlayerId = 1;

    public function __construct()
    {
        $this->ball = BallState::offPitch();
        $this->homeTeam = TeamStateDTO::create(1, 'Home', 'Human', TeamSide::HOME, 3);
        $this->awayTeam = TeamStateDTO::create(2, 'Away', 'Orc', TeamSide::AWAY, 2);
    }

    public function withPhase(GamePhase $phase): self
    {
        $this->phase = $phase;
        return $this;
    }

    public function withActiveTeam(TeamSide $side): self
    {
        $this->activeTeam = $side;
        return $this;
    }

    /**
     * Add a standing player at a position.
     *
     * @param list<SkillName> $skills
     */
    public function addPlayer(
        TeamSide $side,
        int $x,
        int $y,
        int $movement = 6,
        int $strength = 3,
        int $agility = 3,
        int $armour = 8,
        array $skills = [],
        ?int $id = null,
        ?string $raceName = null,
    ): self {
        $playerId = $id ?? $this->nextPlayerId++;
        if ($id !== null && $id >= $this->nextPlayerId) {
            $this->nextPlayerId = $id + 1;
        }
        $player = MatchPlayerDTO::create(
            id: $playerId,
            playerId: $playerId,
            name: "Player {$playerId}",
            number: $playerId,
            positionalName: 'Lineman',
            stats: new PlayerStats($movement, $strength, $agility, $armour),
            skills: $skills,
            teamSide: $side,
            position: new Position($x, $y),
            raceName: $raceName,
        );
        $this->players[$playerId] = $player;
        return $this;
    }

    /**
     * Add a prone player at a position.
     *
     * @param list<SkillName> $skills
     */
    public function addPronePlayer(
        TeamSide $side,
        int $x,
        int $y,
        int $movement = 6,
        int $strength = 3,
        int $agility = 3,
        int $armour = 8,
        array $skills = [],
        ?int $id = null,
    ): self {
        $this->addPlayer($side, $x, $y, movement: $movement, strength: $strength, agility: $agility, armour: $armour, skills: $skills, id: $id);
        $playerId = $id ?? ($this->nextPlayerId - 1);
        $this->players[$playerId] = $this->players[$playerId]->withState(PlayerState::PRONE);
        return $this;
    }

    /**
     * Add an off-pitch player (for setup phase testing).
     *
     * @param list<SkillName> $skills
     */
    public function addOffPitchPlayer(
        TeamSide $side,
        int $movement = 6,
        int $strength = 3,
        int $agility = 3,
        int $armour = 8,
        array $skills = [],
        ?int $id = null,
    ): self {
        $playerId = $id ?? $this->nextPlayerId++;
        if ($id !== null && $id >= $this->nextPlayerId) {
            $this->nextPlayerId = $id + 1;
        }
        $player = MatchPlayerDTO::create(
            id: $playerId,
            playerId: $playerId,
            name: "Player {$playerId}",
            number: $playerId,
            positionalName: 'Lineman',
            stats: new PlayerStats($movement, $strength, $agility, $armour),
            skills: $skills,
            teamSide: $side,
            position: new Position(0, 0),
        );
        $player = $player->withPosition(null)->withState(PlayerState::OFF_PITCH);
        $this->players[$playerId] = $player;
        return $this;
    }

    public function withHalf(int $half): self
    {
        $this->half = $half;
        return $this;
    }

    public function withHomeTeam(TeamStateDTO $team): self
    {
        $this->homeTeam = $team;
        return $this;
    }

    public function withAwayTeam(TeamStateDTO $team): self
    {
        $this->awayTeam = $team;
        return $this;
    }

    public function withBallOnGround(int $x, int $y): self
    {
        $this->ball = BallState::onGround(new Position($x, $y));
        return $this;
    }

    public function withBallCarried(int $carrierId): self
    {
        $player = $this->players[$carrierId] ?? null;
        if ($player && $player->getPosition()) {
            $this->ball = BallState::carried($player->getPosition(), $carrierId);
        }
        return $this;
    }

    public function withBallOffPitch(): self
    {
        $this->ball = BallState::offPitch();
        return $this;
    }

    public function withWeather(Weather $weather): self
    {
        $this->weather = $weather;
        return $this;
    }

    public function build(): GameState
    {
        return new GameState(
            matchId: $this->matchId,
            half: $this->half,
            phase: $this->phase,
            activeTeam: $this->activeTeam,
            homeTeam: $this->homeTeam,
            awayTeam: $this->awayTeam,
            players: $this->players,
            ball: $this->ball,
            turnoverPending: false,
            kickingTeam: TeamSide::AWAY,
            weather: $this->weather,
        );
    }
}
