<?php

declare(strict_types=1);

namespace App\Repository;

use App\Database;
use App\Entity\PositionalTemplate;
use App\Entity\Race;
use PDO;

final class RaceRepository
{
    private PDO $pdo;

    public function __construct(?PDO $pdo = null)
    {
        $this->pdo = $pdo ?? Database::getConnection();
    }

    /**
     * @return list<Race>
     */
    public function findAll(): array
    {
        $stmt = $this->pdo->prepare('SELECT * FROM races ORDER BY name');
        $stmt->execute();

        return array_values(array_map(Race::fromRow(...), $stmt->fetchAll()));
    }

    public function findById(int $id): ?Race
    {
        $stmt = $this->pdo->prepare('SELECT * FROM races WHERE id = :id');
        $stmt->execute(['id' => $id]);
        $row = $stmt->fetch();

        return $row !== false ? Race::fromRow($row) : null;
    }

    public function findByIdWithPositionals(int $id): ?Race
    {
        $race = $this->findById($id);
        if ($race === null) {
            return null;
        }

        $positionals = $this->getPositionalsForRace($race->getId());
        return $race->withPositionals($positionals);
    }

    /**
     * @return list<Race>
     */
    public function findAllWithPositionals(): array
    {
        $races = $this->findAll();
        $result = [];

        foreach ($races as $race) {
            $positionals = $this->getPositionalsForRace($race->getId());
            $result[] = $race->withPositionals($positionals);
        }

        return $result;
    }

    public function findPositionalTemplateById(int $id): ?PositionalTemplate
    {
        $stmt = $this->pdo->prepare(
            'SELECT * FROM positional_templates WHERE id = :id'
        );
        $stmt->execute(['id' => $id]);
        $row = $stmt->fetch();

        if ($row === false) {
            return null;
        }

        $template = PositionalTemplate::fromRow($row);
        $skillRepo = new SkillRepository($this->pdo);
        $skills = $skillRepo->findByPositionalTemplate($template->getId());

        return $template->withStartingSkills($skills);
    }

    /**
     * @return list<PositionalTemplate>
     */
    private function getPositionalsForRace(int $raceId): array
    {
        $stmt = $this->pdo->prepare(
            'SELECT * FROM positional_templates WHERE race_id = :race_id ORDER BY cost, name'
        );
        $stmt->execute(['race_id' => $raceId]);
        $templates = array_map(PositionalTemplate::fromRow(...), $stmt->fetchAll());

        // Load starting skills for each positional
        $skillRepo = new SkillRepository($this->pdo);
        $result = [];
        foreach ($templates as $template) {
            $skills = $skillRepo->findByPositionalTemplate($template->getId());
            $result[] = $template->withStartingSkills($skills);
        }

        return $result;
    }
}
