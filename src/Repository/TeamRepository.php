<?php

declare(strict_types=1);

namespace App\Repository;

use App\Database;
use App\Entity\Team;
use PDO;

final class TeamRepository
{
    private PDO $pdo;

    public function __construct(?PDO $pdo = null)
    {
        $this->pdo = $pdo ?? Database::getConnection();
    }

    public function findById(int $id): ?Team
    {
        $stmt = $this->pdo->prepare(
            'SELECT t.*, r.name AS race_name, c.name AS coach_name
             FROM teams t
             JOIN races r ON t.race_id = r.id
             JOIN coaches c ON t.coach_id = c.id
             WHERE t.id = :id'
        );
        $stmt->execute(['id' => $id]);
        $row = $stmt->fetch();

        return $row !== false ? Team::fromRow($row) : null;
    }

    public function findByIdWithPlayers(int $id): ?Team
    {
        $team = $this->findById($id);
        if ($team === null) {
            return null;
        }

        $playerRepo = new PlayerRepository($this->pdo);
        $players = $playerRepo->findByTeamId($team->getId());

        return $team->withPlayers($players);
    }

    /**
     * @return list<Team>
     */
    public function findByCoachId(int $coachId): array
    {
        $stmt = $this->pdo->prepare(
            'SELECT t.*, r.name AS race_name, c.name AS coach_name
             FROM teams t
             JOIN races r ON t.race_id = r.id
             JOIN coaches c ON t.coach_id = c.id
             WHERE t.coach_id = :coach_id AND t.status = \'active\'
             ORDER BY t.name'
        );
        $stmt->execute(['coach_id' => $coachId]);

        return array_values(array_map(Team::fromRow(...), $stmt->fetchAll()));
    }

    /**
     * @return list<Team>
     */
    public function findAll(): array
    {
        $stmt = $this->pdo->prepare(
            'SELECT t.*, r.name AS race_name, c.name AS coach_name
             FROM teams t
             JOIN races r ON t.race_id = r.id
             JOIN coaches c ON t.coach_id = c.id
             WHERE t.status = \'active\'
             ORDER BY t.name'
        );
        $stmt->execute();

        return array_values(array_map(Team::fromRow(...), $stmt->fetchAll()));
    }

    /**
     * @param array{coach_id: int, race_id: int, name: string, treasury: int} $data
     */
    public function save(array $data): Team
    {
        $stmt = $this->pdo->prepare(
            'INSERT INTO teams (coach_id, race_id, name, treasury)
             VALUES (:coach_id, :race_id, :name, :treasury)
             RETURNING *'
        );
        $stmt->execute($data);

        /** @var array<string, mixed> $row */
        $row = $stmt->fetch();
        // Re-fetch with JOINed names
        return $this->findById((int) $row['id']) ?? Team::fromRow($row);
    }

    public function updateTreasury(int $id, int $treasury): bool
    {
        $stmt = $this->pdo->prepare(
            'UPDATE teams SET treasury = :treasury, updated_at = CURRENT_TIMESTAMP WHERE id = :id'
        );
        $stmt->execute(['id' => $id, 'treasury' => $treasury]);

        return $stmt->rowCount() > 0;
    }

    public function updateRerolls(int $id, int $rerolls): bool
    {
        $stmt = $this->pdo->prepare(
            'UPDATE teams SET rerolls = :rerolls, updated_at = CURRENT_TIMESTAMP WHERE id = :id'
        );
        $stmt->execute(['id' => $id, 'rerolls' => $rerolls]);

        return $stmt->rowCount() > 0;
    }

    public function updateApothecary(int $id, bool $hasApothecary): bool
    {
        $stmt = $this->pdo->prepare(
            'UPDATE teams SET has_apothecary = :has_apothecary, updated_at = CURRENT_TIMESTAMP WHERE id = :id'
        );
        $stmt->execute(['id' => $id, 'has_apothecary' => $hasApothecary ? 't' : 'f']);

        return $stmt->rowCount() > 0;
    }

    public function updateStatus(int $id, string $status): bool
    {
        $stmt = $this->pdo->prepare(
            'UPDATE teams SET status = :status, updated_at = CURRENT_TIMESTAMP WHERE id = :id'
        );
        $stmt->execute(['id' => $id, 'status' => $status]);

        return $stmt->rowCount() > 0;
    }

    public function delete(int $id): bool
    {
        $stmt = $this->pdo->prepare('DELETE FROM teams WHERE id = :id');
        $stmt->execute(['id' => $id]);

        return $stmt->rowCount() > 0;
    }

    public function nameExistsForCoach(int $coachId, string $name, ?int $excludeId = null): bool
    {
        $sql = 'SELECT COUNT(*) FROM teams WHERE coach_id = :coach_id AND name = :name AND status = \'active\'';
        $params = ['coach_id' => $coachId, 'name' => $name];

        if ($excludeId !== null) {
            $sql .= ' AND id != :exclude_id';
            $params['exclude_id'] = $excludeId;
        }

        $stmt = $this->pdo->prepare($sql);
        $stmt->execute($params);

        return (int) $stmt->fetchColumn() > 0;
    }
}
