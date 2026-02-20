<?php

declare(strict_types=1);

namespace App\Controller;

use App\Repository\RaceRepository;
use App\Service\AuthService;
use Twig\Environment;

final class PageController
{
    public function __construct(
        private readonly AuthService $authService,
        private readonly RaceRepository $raceRepository,
        private readonly Environment $twig,
    ) {
    }

    public function dashboard(): void
    {
        $coach = $this->authService->getCurrentCoach();

        echo $this->twig->render('dashboard.html.twig', [
            'coach' => $coach,
            'isLoggedIn' => $coach !== null,
        ]);
    }

    public function loginPage(): void
    {
        if ($this->authService->isLoggedIn()) {
            header('Location: /');
            return;
        }

        echo $this->twig->render('login.html.twig', [
            'isLoggedIn' => false,
        ]);
    }

    public function loginAction(): void
    {
        $email = $_POST['email'] ?? '';
        $password = $_POST['password'] ?? '';

        if ($this->authService->login($email, $password)) {
            header('Location: /');
        } else {
            echo $this->twig->render('login.html.twig', [
                'error' => 'Invalid email or password',
                'isLoggedIn' => false,
            ]);
        }
    }

    public function registerPage(): void
    {
        if ($this->authService->isLoggedIn()) {
            header('Location: /');
            return;
        }

        echo $this->twig->render('register.html.twig', [
            'isLoggedIn' => false,
        ]);
    }

    public function registerAction(): void
    {
        $name = $_POST['name'] ?? '';
        $email = $_POST['email'] ?? '';
        $password = $_POST['password'] ?? '';

        try {
            $this->authService->register($name, $email, $password);
            header('Location: /');
        } catch (\RuntimeException $e) {
            echo $this->twig->render('register.html.twig', [
                'error' => $e->getMessage(),
                'isLoggedIn' => false,
            ]);
        }
    }

    public function logout(): void
    {
        $this->authService->logout();
        header('Location: /login');
    }

    public function races(): void
    {
        $races = $this->raceRepository->findAllWithPositionals();

        echo $this->twig->render('races.html.twig', [
            'races' => $races,
            'coach' => $this->authService->getCurrentCoach(),
            'isLoggedIn' => $this->authService->isLoggedIn(),
        ]);
    }

    public function matchDemo(): void
    {
        echo $this->twig->render('match.html.twig', [
            'coach' => $this->authService->getCurrentCoach(),
            'isLoggedIn' => $this->authService->isLoggedIn(),
        ]);
    }
}
