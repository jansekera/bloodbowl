<?php
declare(strict_types=1);

namespace App\Repository;

use App\Database;
use PDO;

final class MatchPlayerRepository
{
    private PDO $pdo;

    public function __construct(?PDO $pdo = null)
    {
        $this->pdo = $pdo ?? Database::getConnection();
    }

    /**
     * @param array{match_id: int, player_id: int, team_side: string, name: string, number: int, positional_name: string, ma: int, st: int, ag: int, av: int, skills: string} $data
     * @return array<string, mixed>
     */
    public function save(array $data): array
    {
        $stmt = $this->pdo->prepare(
            'INSERT INTO match_players (match_id, player_id, team_side, name, number, positional_name, ma, st, ag, av, skills)
             VALUES (:match_id, :player_id, :team_side, :name, :number, :positional_name, :ma, :st, :ag, :av, :skills)
             RETURNING *'
        );
        $stmt->execute($data);

        /** @var array<string, mixed> */
        return $stmt->fetch();
    }

    /**
     * @return list<array<string, mixed>>
     */
    public function findByMatchId(int $matchId): array
    {
        $stmt = $this->pdo->prepare(
            'SELECT * FROM match_players WHERE match_id = :match_id ORDER BY team_side, number'
        );
        $stmt->execute(['match_id' => $matchId]);

        return array_values($stmt->fetchAll());
    }
}
