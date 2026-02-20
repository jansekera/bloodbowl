<?php
declare(strict_types=1);

namespace App\Repository;

use App\Database;
use PDO;

final class MatchRepository
{
    private PDO $pdo;

    public function __construct(?PDO $pdo = null)
    {
        $this->pdo = $pdo ?? Database::getConnection();
    }

    /**
     * @param array{home_team_id: int, away_team_id: int, home_coach_id: int, away_coach_id: ?int} $data
     * @return array<string, mixed>
     */
    public function create(array $data): array
    {
        $stmt = $this->pdo->prepare(
            'INSERT INTO matches (home_team_id, away_team_id, home_coach_id, away_coach_id)
             VALUES (:home_team_id, :away_team_id, :home_coach_id, :away_coach_id)
             RETURNING *'
        );
        $stmt->execute($data);

        /** @var array<string, mixed> */
        return $stmt->fetch();
    }

    /**
     * @return array<string, mixed>|null
     */
    public function findById(int $id): ?array
    {
        $stmt = $this->pdo->prepare(
            'SELECT m.*,
                    ht.name AS home_team_name, hr.name AS home_race_name,
                    at.name AS away_team_name, ar.name AS away_race_name,
                    hc.name AS home_coach_name, ac.name AS away_coach_name
             FROM matches m
             JOIN teams ht ON m.home_team_id = ht.id
             JOIN races hr ON ht.race_id = hr.id
             JOIN teams at ON m.away_team_id = at.id
             JOIN races ar ON at.race_id = ar.id
             JOIN coaches hc ON m.home_coach_id = hc.id
             LEFT JOIN coaches ac ON m.away_coach_id = ac.id
             WHERE m.id = :id'
        );
        $stmt->execute(['id' => $id]);
        $row = $stmt->fetch();

        return $row !== false ? $row : null;
    }

    /**
     * @return list<array<string, mixed>>
     */
    public function findByCoachId(int $coachId): array
    {
        $stmt = $this->pdo->prepare(
            'SELECT m.*,
                    ht.name AS home_team_name, at.name AS away_team_name
             FROM matches m
             JOIN teams ht ON m.home_team_id = ht.id
             JOIN teams at ON m.away_team_id = at.id
             WHERE m.home_coach_id = :coach_id OR m.away_coach_id = :coach_id
             ORDER BY m.created_at DESC'
        );
        $stmt->execute(['coach_id' => $coachId]);

        return array_values($stmt->fetchAll());
    }

    public function updateGameState(int $matchId, string $gameStateJson): void
    {
        $stmt = $this->pdo->prepare(
            'UPDATE matches SET game_state = :game_state, updated_at = CURRENT_TIMESTAMP
             WHERE id = :id'
        );
        $stmt->execute(['id' => $matchId, 'game_state' => $gameStateJson]);
    }

    public function updateStatus(int $matchId, string $status): void
    {
        $sql = 'UPDATE matches SET status = :status, updated_at = CURRENT_TIMESTAMP';
        $params = ['id' => $matchId, 'status' => $status];

        if ($status === 'finished') {
            $sql .= ', finished_at = CURRENT_TIMESTAMP';
        }

        $sql .= ' WHERE id = :id';

        $stmt = $this->pdo->prepare($sql);
        $stmt->execute($params);
    }

    public function updateScore(int $matchId, int $homeScore, int $awayScore): void
    {
        $stmt = $this->pdo->prepare(
            'UPDATE matches SET home_score = :home_score, away_score = :away_score,
                    updated_at = CURRENT_TIMESTAMP
             WHERE id = :id'
        );
        $stmt->execute([
            'id' => $matchId,
            'home_score' => $homeScore,
            'away_score' => $awayScore,
        ]);
    }
}
