<?php

declare(strict_types=1);

namespace App\Controller;

use App\Exception\NotFoundException;
use App\Exception\ValidationException;
use App\Repository\TeamRepository;
use App\Service\AuthService;
use App\Service\TeamService;

final class TeamApiController
{
    public function __construct(
        private readonly AuthService $authService,
        private readonly TeamService $teamService,
        private readonly TeamRepository $teamRepository,
    ) {
    }

    public function list(): void
    {
        $coach = $this->authService->requireAuth();
        $teams = $this->teamRepository->findByCoachId($coach->getId());

        $this->json([
            'data' => array_map(fn($t) => $t->toArray(), $teams),
            '_links' => [
                'self' => ['href' => '/api/v1/teams'],
                'create' => ['href' => '/api/v1/teams', 'method' => 'POST'],
            ],
        ]);
    }

    public function show(int $id): void
    {
        $coach = $this->authService->requireAuth();
        $team = $this->teamRepository->findByIdWithPlayers($id);

        if ($team === null || $team->getCoachId() !== $coach->getId()) {
            $this->json(['error' => 'Team not found'], 404);
            return;
        }

        $this->json([
            'data' => $team->toArray(),
            '_links' => [
                'self' => ['href' => "/api/v1/teams/{$id}"],
                'collection' => ['href' => '/api/v1/teams'],
                'hire_player' => ['href' => "/api/v1/teams/{$id}/players", 'method' => 'POST'],
                'buy_reroll' => ['href' => "/api/v1/teams/{$id}/rerolls", 'method' => 'POST'],
            ],
        ]);
    }

    public function create(): void
    {
        $coach = $this->authService->requireAuth();
        $body = $this->getJsonBody();

        $raceId = (int) ($body['race_id'] ?? 0);
        $name = (string) ($body['name'] ?? '');

        try {
            $team = $this->teamService->createTeam($coach->getId(), $raceId, $name);
            $this->json([
                'data' => $team->toArray(),
                '_links' => [
                    'self' => ['href' => "/api/v1/teams/{$team->getId()}"],
                    'collection' => ['href' => '/api/v1/teams'],
                ],
            ], 201);
        } catch (ValidationException $e) {
            $this->json(['errors' => $e->getErrors()], 422);
        }
    }

    public function hirePlayer(int $teamId): void
    {
        $coach = $this->authService->requireAuth();
        $team = $this->teamRepository->findById($teamId);

        if ($team === null || $team->getCoachId() !== $coach->getId()) {
            $this->json(['error' => 'Team not found'], 404);
            return;
        }

        $body = $this->getJsonBody();
        $templateId = (int) ($body['positional_template_id'] ?? 0);
        $playerName = (string) ($body['name'] ?? '');

        try {
            $player = $this->teamService->hirePlayer($teamId, $templateId, $playerName);
            $this->json([
                'data' => $player->toArray(),
                '_links' => [
                    'self' => ['href' => "/api/v1/teams/{$teamId}/players/{$player->getId()}"],
                    'team' => ['href' => "/api/v1/teams/{$teamId}"],
                ],
            ], 201);
        } catch (ValidationException $e) {
            $this->json(['errors' => $e->getErrors()], 422);
        } catch (NotFoundException $e) {
            $this->json(['error' => $e->getMessage()], 404);
        }
    }

    public function firePlayer(int $teamId, int $playerId): void
    {
        $coach = $this->authService->requireAuth();
        $team = $this->teamRepository->findById($teamId);

        if ($team === null || $team->getCoachId() !== $coach->getId()) {
            $this->json(['error' => 'Team not found'], 404);
            return;
        }

        try {
            $this->teamService->firePlayer($teamId, $playerId);
            $this->json(['message' => 'Player retired'], 200);
        } catch (NotFoundException $e) {
            $this->json(['error' => $e->getMessage()], 404);
        }
    }

    public function buyReroll(int $teamId): void
    {
        $coach = $this->authService->requireAuth();
        $team = $this->teamRepository->findById($teamId);

        if ($team === null || $team->getCoachId() !== $coach->getId()) {
            $this->json(['error' => 'Team not found'], 404);
            return;
        }

        try {
            $this->teamService->buyReroll($teamId);
            $updatedTeam = $this->teamRepository->findById($teamId);
            $this->json([
                'data' => $updatedTeam?->toArray(),
                'message' => 'Reroll purchased',
            ]);
        } catch (ValidationException $e) {
            $this->json(['errors' => $e->getErrors()], 422);
        }
    }

    public function getAvailableSkills(int $playerId): void
    {
        $this->authService->requireAuth();

        try {
            $result = $this->teamService->getAvailableSkillsForPlayer($playerId);
            $this->json([
                'data' => $result,
                '_links' => [
                    'self' => ['href' => "/api/v1/players/{$playerId}/available-skills"],
                    'advance' => ['href' => "/api/v1/players/{$playerId}/advance", 'method' => 'POST'],
                ],
            ]);
        } catch (NotFoundException $e) {
            $this->json(['error' => $e->getMessage()], 404);
        }
    }

    public function advancePlayer(int $playerId): void
    {
        $this->authService->requireAuth();
        $body = $this->getJsonBody();
        $skillId = (int) ($body['skill_id'] ?? 0);

        try {
            $player = $this->teamService->advancePlayer($playerId, $skillId);
            $this->json([
                'data' => $player->toArray(),
                '_links' => [
                    'self' => ['href' => "/api/v1/players/{$playerId}"],
                    'available_skills' => ['href' => "/api/v1/players/{$playerId}/available-skills"],
                ],
            ]);
        } catch (NotFoundException $e) {
            $this->json(['error' => $e->getMessage()], 404);
        } catch (ValidationException $e) {
            $this->json(['errors' => $e->getErrors()], 422);
        }
    }

    public function buyApothecary(int $teamId): void
    {
        $coach = $this->authService->requireAuth();
        $team = $this->teamRepository->findById($teamId);

        if ($team === null || $team->getCoachId() !== $coach->getId()) {
            $this->json(['error' => 'Team not found'], 404);
            return;
        }

        try {
            $this->teamService->buyApothecary($teamId);
            $updatedTeam = $this->teamRepository->findById($teamId);
            $this->json([
                'data' => $updatedTeam?->toArray(),
                'message' => 'Apothecary hired',
            ]);
        } catch (ValidationException $e) {
            $this->json(['errors' => $e->getErrors()], 422);
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
