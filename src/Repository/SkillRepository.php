<?php

declare(strict_types=1);

namespace App\Repository;

use App\Database;
use App\Entity\Skill;
use PDO;

final class SkillRepository
{
    private PDO $pdo;

    public function __construct(?PDO $pdo = null)
    {
        $this->pdo = $pdo ?? Database::getConnection();
    }

    /**
     * @return list<Skill>
     */
    public function findAll(): array
    {
        $stmt = $this->pdo->prepare('SELECT * FROM skills ORDER BY category, name');
        $stmt->execute();

        return array_values(array_map(Skill::fromRow(...), $stmt->fetchAll()));
    }

    public function findById(int $id): ?Skill
    {
        $stmt = $this->pdo->prepare('SELECT * FROM skills WHERE id = :id');
        $stmt->execute(['id' => $id]);
        $row = $stmt->fetch();

        return $row !== false ? Skill::fromRow($row) : null;
    }

    public function findByName(string $name): ?Skill
    {
        $stmt = $this->pdo->prepare('SELECT * FROM skills WHERE name = :name');
        $stmt->execute(['name' => $name]);
        $row = $stmt->fetch();

        return $row !== false ? Skill::fromRow($row) : null;
    }

    /**
     * @param list<int> $ids
     * @return list<Skill>
     */
    public function findByIds(array $ids): array
    {
        if ($ids === []) {
            return [];
        }

        $placeholders = implode(',', array_fill(0, count($ids), '?'));
        $stmt = $this->pdo->prepare("SELECT * FROM skills WHERE id IN ({$placeholders}) ORDER BY name");
        $stmt->execute($ids);

        return array_values(array_map(Skill::fromRow(...), $stmt->fetchAll()));
    }

    /**
     * @return list<Skill>
     */
    public function findByCategory(string $category): array
    {
        $stmt = $this->pdo->prepare('SELECT * FROM skills WHERE category = :category ORDER BY name');
        $stmt->execute(['category' => $category]);

        return array_values(array_map(Skill::fromRow(...), $stmt->fetchAll()));
    }

    /**
     * @return list<Skill>
     */
    public function findByPositionalTemplate(int $templateId): array
    {
        $stmt = $this->pdo->prepare(
            'SELECT s.* FROM skills s
             JOIN positional_template_skills pts ON s.id = pts.skill_id
             WHERE pts.positional_template_id = :template_id
             ORDER BY s.name'
        );
        $stmt->execute(['template_id' => $templateId]);

        return array_values(array_map(Skill::fromRow(...), $stmt->fetchAll()));
    }
}
