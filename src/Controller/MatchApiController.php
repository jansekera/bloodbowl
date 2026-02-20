<?php
declare(strict_types=1);

namespace App\Controller;

use App\Enum\ActionType;
use App\Exception\NotFoundException;
use App\Exception\ValidationException;
use App\Service\AuthService;
use App\Service\MatchService;

final class MatchApiController
{
    public function __construct(
        private readonly AuthService $authService,
        private readonly MatchService $matchService,
    ) {
    }

    public function create(): void
    {
        $coach = $this->authService->requireAuth();
        $body = $this->getJsonBody();

        $homeTeamId = (int) ($body['home_team_id'] ?? 0);
        $awayTeamId = (int) ($body['away_team_id'] ?? 0);

        try {
            $gameState = $this->matchService->createMatch(
                $homeTeamId,
                $awayTeamId,
                $coach->getId(),
            );

            $this->json([
                'data' => $gameState->toArray(),
                '_links' => [
                    'self' => ['href' => "/api/v1/matches/{$gameState->getMatchId()}"],
                    'state' => ['href' => "/api/v1/matches/{$gameState->getMatchId()}/state"],
                    'actions' => ['href' => "/api/v1/matches/{$gameState->getMatchId()}/actions", 'method' => 'POST'],
                ],
            ], 201);
        } catch (ValidationException $e) {
            $this->json(['errors' => $e->getErrors()], 422);
        } catch (NotFoundException $e) {
            $this->json(['error' => $e->getMessage()], 404);
        }
    }

    public function getState(int $matchId): void
    {
        try {
            $state = $this->matchService->getGameState($matchId);

            $this->json([
                'data' => $state->toArray(),
                '_links' => [
                    'self' => ['href' => "/api/v1/matches/{$matchId}/state"],
                    'actions' => ['href' => "/api/v1/matches/{$matchId}/actions", 'method' => 'POST'],
                    'events' => ['href' => "/api/v1/matches/{$matchId}/events"],
                ],
            ]);
        } catch (NotFoundException $e) {
            $this->json(['error' => $e->getMessage()], 404);
        }
    }

    public function submitAction(int $matchId): void
    {
        $this->authService->requireAuth();
        $body = $this->getJsonBody();

        $actionType = ActionType::tryFrom((string) ($body['action'] ?? ''));
        if ($actionType === null) {
            $this->json(['error' => 'Invalid action type'], 400);
            return;
        }

        /** @var array<string, mixed> */
        $params = (array) ($body['params'] ?? []);

        try {
            $result = $this->matchService->submitAction($matchId, $actionType, $params);

            $this->json([
                'data' => $result->toArray(),
                'state' => $result->getNewState()->toArray(),
                '_links' => [
                    'state' => ['href' => "/api/v1/matches/{$matchId}/state"],
                    'actions' => ['href' => "/api/v1/matches/{$matchId}/actions", 'method' => 'POST'],
                ],
            ]);
        } catch (ValidationException $e) {
            $this->json(['errors' => $e->getErrors()], 422);
        } catch (NotFoundException $e) {
            $this->json(['error' => $e->getMessage()], 404);
        } catch (\InvalidArgumentException $e) {
            $this->json(['error' => $e->getMessage()], 400);
        }
    }

    public function getValidMoves(int $matchId, int $playerId): void
    {
        try {
            $targets = $this->matchService->getValidMoveTargets($matchId, $playerId);

            $this->json([
                'data' => $targets,
                '_links' => [
                    'state' => ['href' => "/api/v1/matches/{$matchId}/state"],
                ],
            ]);
        } catch (NotFoundException $e) {
            $this->json(['error' => $e->getMessage()], 404);
        }
    }

    public function getAvailableActions(int $matchId): void
    {
        try {
            $actions = $this->matchService->getAvailableActions($matchId);

            $this->json([
                'data' => $actions,
                '_links' => [
                    'state' => ['href' => "/api/v1/matches/{$matchId}/state"],
                ],
            ]);
        } catch (NotFoundException $e) {
            $this->json(['error' => $e->getMessage()], 404);
        }
    }

    public function getBlockTargets(int $matchId, int $playerId): void
    {
        try {
            $targets = $this->matchService->getBlockTargets($matchId, $playerId);

            $this->json([
                'data' => $targets,
                '_links' => [
                    'state' => ['href' => "/api/v1/matches/{$matchId}/state"],
                ],
            ]);
        } catch (NotFoundException $e) {
            $this->json(['error' => $e->getMessage()], 404);
        }
    }

    public function getPassTargets(int $matchId, int $playerId): void
    {
        try {
            $targets = $this->matchService->getPassTargets($matchId, $playerId);

            $this->json([
                'data' => $targets,
                '_links' => [
                    'state' => ['href' => "/api/v1/matches/{$matchId}/state"],
                ],
            ]);
        } catch (NotFoundException $e) {
            $this->json(['error' => $e->getMessage()], 404);
        }
    }

    public function getHandOffTargets(int $matchId, int $playerId): void
    {
        try {
            $targets = $this->matchService->getHandOffTargets($matchId, $playerId);

            $this->json([
                'data' => $targets,
                '_links' => [
                    'state' => ['href' => "/api/v1/matches/{$matchId}/state"],
                ],
            ]);
        } catch (NotFoundException $e) {
            $this->json(['error' => $e->getMessage()], 404);
        }
    }

    public function getFoulTargets(int $matchId, int $playerId): void
    {
        try {
            $targets = $this->matchService->getFoulTargets($matchId, $playerId);

            $this->json([
                'data' => $targets,
                '_links' => [
                    'state' => ['href' => "/api/v1/matches/{$matchId}/state"],
                ],
            ]);
        } catch (NotFoundException $e) {
            $this->json(['error' => $e->getMessage()], 404);
        }
    }

    public function getEvents(int $matchId): void
    {
        try {
            $all = ($_GET['all'] ?? '') === '1';
            $events = $all
                ? $this->matchService->getAllEvents($matchId)
                : $this->matchService->getRecentEvents($matchId);

            $this->json([
                'data' => $events,
                '_links' => [
                    'state' => ['href' => "/api/v1/matches/{$matchId}/state"],
                ],
            ]);
        } catch (NotFoundException $e) {
            $this->json(['error' => $e->getMessage()], 404);
        }
    }

    /**
     * @return array<string, mixed>
     */
    private function getJsonBody(): array
    {
        $raw = file_get_contents('php://input');
        if ($raw === false || $raw === '') {
            return [];
        }

        /** @var array<string, mixed> */
        return json_decode($raw, true) ?? [];
    }

    /**
     * @param array<string, mixed> $data
     */
    private function json(array $data, int $statusCode = 200): void
    {
        http_response_code($statusCode);
        header('Content-Type: application/json');
        echo json_encode($data, JSON_PRETTY_PRINT | JSON_UNESCAPED_UNICODE);
    }
}
