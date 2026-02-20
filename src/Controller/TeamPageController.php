<?php

declare(strict_types=1);

namespace App\Controller;

use App\Exception\NotFoundException;
use App\Exception\ValidationException;
use App\Repository\RaceRepository;
use App\Repository\TeamRepository;
use App\Service\AuthService;
use App\Service\TeamService;
use Twig\Environment;

final class TeamPageController
{
    public function __construct(
        private readonly AuthService $authService,
        private readonly TeamService $teamService,
        private readonly TeamRepository $teamRepository,
        private readonly RaceRepository $raceRepository,
        private readonly Environment $twig,
    ) {
    }

    public function list(): void
    {
        $coach = $this->authService->requireAuth();
        $teams = $this->teamRepository->findByCoachId($coach->getId());

        echo $this->twig->render('teams/list.html.twig', [
            'coach' => $coach,
            'isLoggedIn' => true,
            'teams' => $teams,
        ]);
    }

    public function show(int $id): void
    {
        $coach = $this->authService->requireAuth();
        $team = $this->teamRepository->findByIdWithPlayers($id);

        if ($team === null || $team->getCoachId() !== $coach->getId()) {
            http_response_code(404);
            echo '404 Team not found';
            return;
        }

        $race = $this->raceRepository->findByIdWithPositionals($team->getRaceId());

        echo $this->twig->render('teams/show.html.twig', [
            'coach' => $coach,
            'isLoggedIn' => true,
            'team' => $team,
            'race' => $race,
        ]);
    }

    public function createForm(): void
    {
        $coach = $this->authService->requireAuth();
        $races = $this->raceRepository->findAllWithPositionals();

        echo $this->twig->render('teams/create.html.twig', [
            'coach' => $coach,
            'isLoggedIn' => true,
            'races' => $races,
        ]);
    }

    public function createAction(): void
    {
        $coach = $this->authService->requireAuth();

        $raceId = (int) ($_POST['race_id'] ?? 0);
        $name = (string) ($_POST['name'] ?? '');

        try {
            $team = $this->teamService->createTeam($coach->getId(), $raceId, $name);
            header("Location: /teams/{$team->getId()}");
        } catch (ValidationException $e) {
            $races = $this->raceRepository->findAllWithPositionals();
            echo $this->twig->render('teams/create.html.twig', [
                'coach' => $coach,
                'isLoggedIn' => true,
                'races' => $races,
                'errors' => $e->getErrors(),
                'formData' => ['name' => $name, 'race_id' => $raceId],
            ]);
        }
    }

    public function hirePlayerAction(int $teamId): void
    {
        $coach = $this->authService->requireAuth();
        $team = $this->teamRepository->findById($teamId);

        if ($team === null || $team->getCoachId() !== $coach->getId()) {
            http_response_code(404);
            echo '404 Team not found';
            return;
        }

        $templateId = (int) ($_POST['template_id'] ?? 0);
        $playerName = (string) ($_POST['player_name'] ?? '');

        try {
            $this->teamService->hirePlayer($teamId, $templateId, $playerName);
            header("Location: /teams/{$teamId}");
        } catch (ValidationException | NotFoundException $e) {
            $errorMsg = $e instanceof ValidationException
                ? implode(', ', $e->getErrors())
                : $e->getMessage();
            header("Location: /teams/{$teamId}?error=" . urlencode($errorMsg));
        }
    }

    public function firePlayerAction(int $teamId, int $playerId): void
    {
        $coach = $this->authService->requireAuth();
        $team = $this->teamRepository->findById($teamId);

        if ($team === null || $team->getCoachId() !== $coach->getId()) {
            http_response_code(404);
            echo '404 Team not found';
            return;
        }

        try {
            $this->teamService->firePlayer($teamId, $playerId);
        } catch (NotFoundException $e) {
            // Silently ignore
        }

        header("Location: /teams/{$teamId}");
    }

    public function buyRerollAction(int $teamId): void
    {
        $coach = $this->authService->requireAuth();
        $team = $this->teamRepository->findById($teamId);

        if ($team === null || $team->getCoachId() !== $coach->getId()) {
            http_response_code(404);
            echo '404 Team not found';
            return;
        }

        try {
            $this->teamService->buyReroll($teamId);
            header("Location: /teams/{$teamId}");
        } catch (ValidationException $e) {
            header("Location: /teams/{$teamId}?error=" . urlencode(implode(', ', $e->getErrors())));
        }
    }

    public function buyApothecaryAction(int $teamId): void
    {
        $coach = $this->authService->requireAuth();
        $team = $this->teamRepository->findById($teamId);

        if ($team === null || $team->getCoachId() !== $coach->getId()) {
            http_response_code(404);
            echo '404 Team not found';
            return;
        }

        try {
            $this->teamService->buyApothecary($teamId);
            header("Location: /teams/{$teamId}");
        } catch (ValidationException $e) {
            header("Location: /teams/{$teamId}?error=" . urlencode(implode(', ', $e->getErrors())));
        }
    }

    public function retireAction(int $teamId): void
    {
        $coach = $this->authService->requireAuth();
        $team = $this->teamRepository->findById($teamId);

        if ($team === null || $team->getCoachId() !== $coach->getId()) {
            http_response_code(404);
            echo '404 Team not found';
            return;
        }

        try {
            $this->teamService->retireTeam($teamId);
        } catch (NotFoundException $e) {
            // Silently ignore
        }

        header('Location: /teams');
    }
}
