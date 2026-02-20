<?php

declare(strict_types=1);

namespace App\Service;

use App\Entity\Coach;
use App\Repository\CoachRepository;

final class AuthService
{
    private const SESSION_KEY = '_auth_coach_id';

    public function __construct(private readonly CoachRepository $coachRepository)
    {
        if (session_status() === PHP_SESSION_NONE && php_sapi_name() !== 'cli') {
            session_start();
        }
    }

    public function register(string $name, string $email, string $password): Coach
    {
        if ($this->coachRepository->emailExists($email)) {
            throw new \RuntimeException('Email already registered');
        }

        $passwordHash = password_hash($password, PASSWORD_DEFAULT);
        $coach = $this->coachRepository->save($name, $email, $passwordHash);

        $_SESSION[self::SESSION_KEY] = $coach->getId();

        return $coach;
    }

    public function login(string $email, string $password): bool
    {
        $coach = $this->coachRepository->findByEmail($email);
        if ($coach === null || !password_verify($password, $coach->getPasswordHash())) {
            return false;
        }

        $_SESSION[self::SESSION_KEY] = $coach->getId();
        return true;
    }

    public function logout(): void
    {
        unset($_SESSION[self::SESSION_KEY]);
    }

    public function getCurrentCoach(): ?Coach
    {
        $id = $_SESSION[self::SESSION_KEY] ?? null;
        if ($id === null) {
            return null;
        }

        return $this->coachRepository->findById((int) $id);
    }

    public function isLoggedIn(): bool
    {
        return $this->getCurrentCoach() !== null;
    }

    public function requireAuth(): Coach
    {
        $coach = $this->getCurrentCoach();
        if ($coach === null) {
            throw new \RuntimeException('Authentication required');
        }

        return $coach;
    }
}
