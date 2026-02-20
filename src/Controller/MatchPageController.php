<?php
declare(strict_types=1);

namespace App\Controller;

use App\Repository\MatchRepository;
use App\Repository\TeamRepository;
use App\Service\AuthService;
use App\Service\MatchService;
use Twig\Environment;

final class MatchPageController
{
    public function __construct(
        private readonly AuthService $authService,
        private readonly MatchService $matchService,
        private readonly TeamRepository $teamRepo,
        private readonly MatchRepository $matchRepo,
        private readonly Environment $twig,
    ) {
    }

    public function newMatch(): void
    {
        $coach = $this->authService->requireAuth();
        $teams = $this->teamRepo->findByCoachId($coach->getId());

        echo $this->twig->render('matches/new.html.twig', [
            'coach' => $coach,
            'isLoggedIn' => true,
            'teams' => $teams,
        ]);
    }

    public function createMatch(): void
    {
        $coach = $this->authService->requireAuth();

        $homeTeamId = (int) ($_POST['home_team_id'] ?? 0);
        $awayTeamId = (int) ($_POST['away_team_id'] ?? 0);
        $vsAi = isset($_POST['vs_ai']);

        try {
            $gameState = $this->matchService->createMatch($homeTeamId, $awayTeamId, $coach->getId(), vsAi: $vsAi);
            header("Location: /matches/{$gameState->getMatchId()}");
        } catch (\Exception $e) {
            $teams = $this->teamRepo->findByCoachId($coach->getId());
            echo $this->twig->render('matches/new.html.twig', [
                'coach' => $coach,
                'isLoggedIn' => true,
                'teams' => $teams,
                'error' => $e->getMessage(),
            ]);
        }
    }

    public function show(int $matchId): void
    {
        $coach = $this->authService->getCurrentCoach();
        $matchRow = $this->matchRepo->findById($matchId);

        if ($matchRow === null) {
            http_response_code(404);
            echo '404 Match not found';
            return;
        }

        echo $this->twig->render('matches/show.html.twig', [
            'coach' => $coach,
            'isLoggedIn' => $coach !== null,
            'matchId' => $matchId,
            'match' => $matchRow,
        ]);
    }

    public function list(): void
    {
        $coach = $this->authService->requireAuth();
        $matches = $this->matchRepo->findByCoachId($coach->getId());

        echo $this->twig->render('matches/list.html.twig', [
            'coach' => $coach,
            'isLoggedIn' => true,
            'matches' => $matches,
        ]);
    }
}
