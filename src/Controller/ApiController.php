<?php

declare(strict_types=1);

namespace App\Controller;

use App\Repository\RaceRepository;
use App\Repository\SkillRepository;

final class ApiController
{
    public function __construct(
        private readonly RaceRepository $raceRepository,
        private readonly SkillRepository $skillRepository,
    ) {
    }

    public function getRaces(): void
    {
        $races = $this->raceRepository->findAllWithPositionals();

        $this->json([
            'data' => array_map(fn($race) => $race->toArray(), $races),
            '_links' => [
                'self' => ['href' => '/api/v1/races'],
            ],
        ]);
    }

    public function getRace(int $id): void
    {
        $race = $this->raceRepository->findByIdWithPositionals($id);

        if ($race === null) {
            $this->json(['error' => 'Race not found'], 404);
            return;
        }

        $this->json([
            'data' => $race->toArray(),
            '_links' => [
                'self' => ['href' => "/api/v1/races/{$id}"],
                'collection' => ['href' => '/api/v1/races'],
            ],
        ]);
    }

    public function getSkills(): void
    {
        $skills = $this->skillRepository->findAll();

        $this->json([
            'data' => array_map(fn($skill) => $skill->toArray(), $skills),
            '_links' => [
                'self' => ['href' => '/api/v1/skills'],
            ],
        ]);
    }

    public function getSkill(int $id): void
    {
        $skill = $this->skillRepository->findById($id);

        if ($skill === null) {
            $this->json(['error' => 'Skill not found'], 404);
            return;
        }

        $this->json([
            'data' => $skill->toArray(),
            '_links' => [
                'self' => ['href' => "/api/v1/skills/{$id}"],
                'collection' => ['href' => '/api/v1/skills'],
            ],
        ]);
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
