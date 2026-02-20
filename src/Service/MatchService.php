<?php
declare(strict_types=1);

namespace App\Service;

use App\AI\AICoachInterface;
use App\AI\GameLogger;
use App\DTO\ActionResult;
use App\DTO\BallState;
use App\DTO\GameState;
use App\DTO\MatchPlayerDTO;
use App\DTO\TeamStateDTO;
use App\Engine\DiceRollerInterface;
use App\Engine\RulesEngine;
use App\Enum\ActionType;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use App\Enum\Weather;
use App\Exception\NotFoundException;
use App\Exception\ValidationException;
use App\Repository\MatchEventRepository;
use App\Repository\MatchPlayerRepository;
use App\Repository\MatchRepository;
use App\Repository\PlayerRepository;
use App\Repository\TeamRepository;
use App\ValueObject\PlayerStats;
use App\ValueObject\Position;

final class MatchService
{
    private readonly GameOrchestrator $orchestrator;
    private ?string $logDir = null;

    public function __construct(
        private readonly MatchRepository $matchRepo,
        private readonly MatchPlayerRepository $matchPlayerRepo,
        private readonly MatchEventRepository $matchEventRepo,
        private readonly TeamRepository $teamRepo,
        private readonly RulesEngine $rulesEngine,
        private readonly DiceRollerInterface $dice,
        private readonly AICoachInterface $aiCoach,
        private readonly SPPService $sppService = new SPPService(),
        private readonly ?PlayerRepository $playerRepo = null,
    ) {
        $this->orchestrator = new GameOrchestrator($rulesEngine, $dice, $aiCoach);
    }

    /**
     * Enable game logging for training data collection.
     */
    public function setLogDir(string $logDir): void
    {
        $this->logDir = $logDir;
    }

    /**
     * Create a new match between two teams.
     */
    public function createMatch(int $homeTeamId, int $awayTeamId, int $coachId, ?int $awayCoachId = null, bool $vsAi = false): GameState
    {
        $homeTeam = $this->teamRepo->findByIdWithPlayers($homeTeamId);
        if ($homeTeam === null) {
            throw new NotFoundException('Home team not found');
        }

        $awayTeam = $this->teamRepo->findByIdWithPlayers($awayTeamId);
        if ($awayTeam === null) {
            throw new NotFoundException('Away team not found');
        }

        if (count($homeTeam->getActivePlayers()) < 11) {
            throw new ValidationException(['Home team needs at least 11 active players']);
        }

        if (count($awayTeam->getActivePlayers()) < 11) {
            throw new ValidationException(['Away team needs at least 11 active players']);
        }

        // Create match record
        $matchRow = $this->matchRepo->create([
            'home_team_id' => $homeTeamId,
            'away_team_id' => $awayTeamId,
            'home_coach_id' => $coachId,
            'away_coach_id' => $awayCoachId,
        ]);
        $matchId = (int) $matchRow['id'];

        // Create match players and build game state
        $players = [];
        $matchPlayerId = 1;

        foreach ($homeTeam->getActivePlayers() as $player) {
            $mpRow = $this->matchPlayerRepo->save([
                'match_id' => $matchId,
                'player_id' => $player->getId(),
                'team_side' => 'home',
                'name' => $player->getName(),
                'number' => $player->getNumber(),
                'positional_name' => $player->getPositionalName() ?? 'Unknown',
                'ma' => $player->getStats()->getMovement(),
                'st' => $player->getStats()->getStrength(),
                'ag' => $player->getStats()->getAgility(),
                'av' => $player->getStats()->getArmour(),
                'skills' => (string) json_encode(array_map(fn($s) => $s->getName(), $player->getSkills())),
            ]);

            $mp = MatchPlayerDTO::create(
                id: (int) $mpRow['id'],
                playerId: $player->getId(),
                name: $player->getName(),
                number: $player->getNumber(),
                positionalName: $player->getPositionalName() ?? 'Unknown',
                stats: $player->getStats(),
                skills: array_map(fn($s) => SkillName::from($s->getName()), $player->getSkills()),
                teamSide: TeamSide::HOME,
                position: new Position(-1, -1), // Off pitch until setup
            );
            $mp = $mp->withPosition(null)->withState(\App\Enum\PlayerState::OFF_PITCH);
            $players[$mp->getId()] = $mp;
        }

        foreach ($awayTeam->getActivePlayers() as $player) {
            $mpRow = $this->matchPlayerRepo->save([
                'match_id' => $matchId,
                'player_id' => $player->getId(),
                'team_side' => 'away',
                'name' => $player->getName(),
                'number' => $player->getNumber(),
                'positional_name' => $player->getPositionalName() ?? 'Unknown',
                'ma' => $player->getStats()->getMovement(),
                'st' => $player->getStats()->getStrength(),
                'ag' => $player->getStats()->getAgility(),
                'av' => $player->getStats()->getArmour(),
                'skills' => (string) json_encode(array_map(fn($s) => $s->getName(), $player->getSkills())),
            ]);

            $mp = MatchPlayerDTO::create(
                id: (int) $mpRow['id'],
                playerId: $player->getId(),
                name: $player->getName(),
                number: $player->getNumber(),
                positionalName: $player->getPositionalName() ?? 'Unknown',
                stats: $player->getStats(),
                skills: array_map(fn($s) => SkillName::from($s->getName()), $player->getSkills()),
                teamSide: TeamSide::AWAY,
                position: new Position(-1, -1),
            );
            $mp = $mp->withPosition(null)->withState(\App\Enum\PlayerState::OFF_PITCH);
            $players[$mp->getId()] = $mp;
        }

        // Build team states
        $homeTeamState = TeamStateDTO::create(
            teamId: $homeTeamId,
            name: $homeTeam->getName(),
            raceName: $homeTeam->getRaceName() ?? 'Unknown',
            side: TeamSide::HOME,
            rerolls: $homeTeam->getRerolls(),
        );

        $awayTeamState = TeamStateDTO::create(
            teamId: $awayTeamId,
            name: $awayTeam->getName(),
            raceName: $awayTeam->getRaceName() ?? 'Unknown',
            side: TeamSide::AWAY,
            rerolls: $awayTeam->getRerolls(),
        );

        // Coin toss: home team receives first
        $gameState = GameState::create(
            matchId: $matchId,
            homeTeam: $homeTeamState,
            awayTeam: $awayTeamState,
            players: $players,
            receivingTeam: TeamSide::HOME,
        );

        // Roll initial weather (2D6)
        $weatherRoll = $this->dice->rollD6() + $this->dice->rollD6();
        $gameState = $gameState->withWeather(Weather::fromRoll($weatherRoll));

        if ($vsAi) {
            $gameState = $gameState->withAiTeam(TeamSide::AWAY);
        }

        // Save initial state
        $this->matchRepo->updateGameState($matchId, (string) json_encode($gameState->toArray()));
        $this->matchRepo->updateStatus($matchId, 'in_progress');

        return $gameState;
    }

    /**
     * Get the current game state for a match.
     */
    public function getGameState(int $matchId): GameState
    {
        $matchRow = $this->matchRepo->findById($matchId);
        if ($matchRow === null) {
            throw new NotFoundException('Match not found');
        }

        if ($matchRow['game_state'] === null) {
            throw new \RuntimeException('Match has no game state');
        }

        /** @var array<string, mixed> */
        $data = json_decode((string) $matchRow['game_state'], true);
        return GameState::fromArray($data);
    }

    /**
     * Submit an action to the game engine.
     *
     * @param array<string, mixed> $params
     */
    public function submitAction(int $matchId, ActionType $action, array $params): ActionResult
    {
        $state = $this->getGameState($matchId);

        // Enable game logging for AI matches (for learning from human play)
        if ($this->logDir !== null && $state->getAiTeam() !== null) {
            $logPath = rtrim($this->logDir, '/') . "/match_{$matchId}.jsonl";
            $this->orchestrator->setGameLogger(new GameLogger($logPath));
        }

        $result = $this->orchestrator->executeAction($state, $action, $params);
        $newState = $result['state'];
        $allEvents = $result['events'];

        // Save events
        $this->matchEventRepo->saveAll($matchId, $allEvents);

        // Save updated state
        $this->matchRepo->updateGameState($matchId, (string) json_encode($newState->toArray()));

        // Update match status if game over
        if ($newState->getPhase() === \App\Enum\GamePhase::GAME_OVER) {
            $this->matchRepo->updateStatus($matchId, 'finished');
            $this->matchRepo->updateScore(
                $matchId,
                $newState->getHomeTeam()->getScore(),
                $newState->getAwayTeam()->getScore(),
            );
            $this->awardSPP($matchId, $newState);
        }

        return new ActionResult(
            $newState,
            $result['isSuccess'],
            $result['isTurnover'],
            $allEvents,
        );
    }

    /**
     * Get valid move targets for a player.
     *
     * @return list<array{x: int, y: int, dodges: int, gfis: int}>
     */
    public function getValidMoveTargets(int $matchId, int $playerId): array
    {
        $state = $this->getGameState($matchId);
        return $this->rulesEngine->getValidMoveTargets($state, $playerId);
    }

    /**
     * Get available actions for the current team.
     *
     * @return list<array{type: string, playerId?: int}>
     */
    public function getAvailableActions(int $matchId): array
    {
        $state = $this->getGameState($matchId);
        return $this->rulesEngine->getAvailableActions($state);
    }

    /**
     * Get block targets for a specific player.
     *
     * @return list<array{playerId: int, name: string, x: int, y: int}>
     */
    public function getBlockTargets(int $matchId, int $playerId): array
    {
        $state = $this->getGameState($matchId);
        $player = $state->getPlayer($playerId);
        if ($player === null) {
            return [];
        }

        $targets = $this->rulesEngine->getBlockTargets($state, $player);
        $result = [];
        foreach ($targets as $target) {
            $pos = $target->getPosition();
            if ($pos !== null) {
                $result[] = [
                    'playerId' => $target->getId(),
                    'name' => $target->getName(),
                    'x' => $pos->getX(),
                    'y' => $pos->getY(),
                ];
            }
        }

        return $result;
    }

    /**
     * Get pass targets for a player.
     *
     * @return list<array{x: int, y: int, range: string}>
     */
    public function getPassTargets(int $matchId, int $playerId): array
    {
        $state = $this->getGameState($matchId);
        $player = $state->getPlayer($playerId);
        if ($player === null) {
            return [];
        }

        return $this->rulesEngine->getPassTargets($state, $player);
    }

    /**
     * Get hand-off targets for a player.
     *
     * @return list<array{playerId: int, name: string, x: int, y: int}>
     */
    public function getHandOffTargets(int $matchId, int $playerId): array
    {
        $state = $this->getGameState($matchId);
        $player = $state->getPlayer($playerId);
        if ($player === null) {
            return [];
        }

        $targets = $this->rulesEngine->getHandOffTargets($state, $player);
        $result = [];
        foreach ($targets as $target) {
            $pos = $target->getPosition();
            if ($pos !== null) {
                $result[] = [
                    'playerId' => $target->getId(),
                    'name' => $target->getName(),
                    'x' => $pos->getX(),
                    'y' => $pos->getY(),
                ];
            }
        }

        return $result;
    }

    /**
     * Get foul targets for a specific player.
     *
     * @return list<array{playerId: int, name: string, x: int, y: int}>
     */
    public function getFoulTargets(int $matchId, int $playerId): array
    {
        $state = $this->getGameState($matchId);
        $player = $state->getPlayer($playerId);
        if ($player === null) {
            return [];
        }

        $targets = $this->rulesEngine->getFoulTargets($state, $player);
        $result = [];
        foreach ($targets as $target) {
            $pos = $target->getPosition();
            if ($pos !== null) {
                $result[] = [
                    'playerId' => $target->getId(),
                    'name' => $target->getName(),
                    'x' => $pos->getX(),
                    'y' => $pos->getY(),
                ];
            }
        }

        return $result;
    }

    /**
     * Get recent events for a match.
     *
     * @return list<array<string, mixed>>
     */
    public function getRecentEvents(int $matchId, int $limit = 20): array
    {
        return $this->matchEventRepo->findByMatchId($matchId, $limit);
    }

    /**
     * Get all events for a match (for replay).
     *
     * @return list<array<string, mixed>>
     */
    public function getAllEvents(int $matchId): array
    {
        $events = $this->matchEventRepo->findAllGameEvents($matchId);
        return array_map(fn($e) => $e->toArray(), $events);
    }

    /**
     * Award SPP to players at end of match.
     */
    private function awardSPP(int $matchId, GameState $state): void
    {
        if ($this->playerRepo === null) {
            return;
        }

        // Collect all match events
        $events = $this->matchEventRepo->findAllGameEvents($matchId);

        // Build stats from events
        $stats = $this->sppService->collectStats($events);

        // Award MVP to one random player per team
        $homePlayers = [];
        $awayPlayers = [];
        foreach ($state->getPlayers() as $player) {
            if ($player->getTeamSide() === TeamSide::HOME) {
                $homePlayers[] = $player->getId();
            } else {
                $awayPlayers[] = $player->getId();
            }
        }

        $stats = $this->sppService->awardMvp($homePlayers, $stats, random_int(0, PHP_INT_MAX));
        $stats = $this->sppService->awardMvp($awayPlayers, $stats, random_int(0, PHP_INT_MAX));

        // Build map: match player ID → original player ID
        $matchPlayerToPlayer = [];
        foreach ($state->getPlayers() as $player) {
            $matchPlayerToPlayer[$player->getId()] = $player->getPlayerId();
        }

        // Update each player's SPP in the database
        foreach ($stats as $matchPlayerId => $matchStats) {
            $earnedSpp = $matchStats->getSpp();
            if ($earnedSpp <= 0) {
                continue;
            }

            $originalPlayerId = $matchPlayerToPlayer[$matchPlayerId] ?? null;
            if ($originalPlayerId === null) {
                continue;
            }

            $playerEntity = $this->playerRepo->findById($originalPlayerId);
            if ($playerEntity === null) {
                continue;
            }

            $newTotalSpp = $playerEntity->getSpp() + $earnedSpp;
            $newLevel = $this->sppService->getLevel($newTotalSpp);
            $this->playerRepo->updateSPP($originalPlayerId, $newTotalSpp, $newLevel);
        }
    }
}
