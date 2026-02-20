<?php

declare(strict_types=1);

namespace App\Repository;

use App\Database;
use App\Entity\Player;
use App\Entity\Skill;
use PDO;

final class PlayerRepository
{
    private PDO $pdo;

    public function __construct(?PDO $pdo = null)
    {
        $this->pdo = $pdo ?? Database::getConnection();
    }

    public function findById(int $id): ?Player
    {
        $stmt = $this->pdo->prepare(
            'SELECT p.*, pt.name AS positional_name
             FROM players p
             JOIN positional_templates pt ON p.positional_template_id = pt.id
             WHERE p.id = :id'
        );
        $stmt->execute(['id' => $id]);
        $row = $stmt->fetch();

        if ($row === false) {
            return null;
        }

        $player = Player::fromRow($row);
        return $player->withSkills($this->getSkillsForPlayer($id));
    }

    /**
     * @return list<Player>
     */
    public function findByTeamId(int $teamId): array
    {
        $stmt = $this->pdo->prepare(
            'SELECT p.*, pt.name AS positional_name
             FROM players p
             JOIN positional_templates pt ON p.positional_template_id = pt.id
             WHERE p.team_id = :team_id
             ORDER BY p.number'
        );
        $stmt->execute(['team_id' => $teamId]);

        $players = [];
        foreach ($stmt->fetchAll() as $row) {
            $player = Player::fromRow($row);
            $players[] = $player->withSkills($this->getSkillsForPlayer($player->getId()));
        }

        return $players;
    }

    /**
     * @param array{team_id: int, positional_template_id: int, name: string, number: int, ma: int, st: int, ag: int, av: int} $data
     */
    public function save(array $data): Player
    {
        $stmt = $this->pdo->prepare(
            'INSERT INTO players (team_id, positional_template_id, name, number, ma, st, ag, av)
             VALUES (:team_id, :positional_template_id, :name, :number, :ma, :st, :ag, :av)
             RETURNING *'
        );
        $stmt->execute($data);

        /** @var array<string, mixed> $row */
        $row = $stmt->fetch();
        // Fetch with positional name
        return $this->findById((int) $row['id']) ?? Player::fromRow($row);
    }

    /**
     * @param list<int> $skillIds
     */
    public function addStartingSkills(int $playerId, array $skillIds): void
    {
        $stmt = $this->pdo->prepare(
            'INSERT INTO player_skills (player_id, skill_id, is_starting)
             VALUES (:player_id, :skill_id, TRUE)
             ON CONFLICT (player_id, skill_id) DO NOTHING'
        );

        foreach ($skillIds as $skillId) {
            $stmt->execute([
                'player_id' => $playerId,
                'skill_id' => $skillId,
            ]);
        }
    }

    public function addSkill(int $playerId, int $skillId): void
    {
        $stmt = $this->pdo->prepare(
            'INSERT INTO player_skills (player_id, skill_id, is_starting)
             VALUES (:player_id, :skill_id, FALSE)
             ON CONFLICT (player_id, skill_id) DO NOTHING'
        );
        $stmt->execute([
            'player_id' => $playerId,
            'skill_id' => $skillId,
        ]);
    }

    public function countNonStartingSkills(int $playerId): int
    {
        $stmt = $this->pdo->prepare(
            'SELECT COUNT(*) FROM player_skills
             WHERE player_id = :player_id AND is_starting = FALSE'
        );
        $stmt->execute(['player_id' => $playerId]);

        return (int) $stmt->fetchColumn();
    }

    public function updateStatus(int $id, string $status): bool
    {
        $stmt = $this->pdo->prepare('UPDATE players SET status = :status WHERE id = :id');
        $stmt->execute(['id' => $id, 'status' => $status]);

        return $stmt->rowCount() > 0;
    }

    public function updateSPP(int $id, int $spp, int $level): bool
    {
        $stmt = $this->pdo->prepare(
            'UPDATE players SET spp = :spp, level = :level WHERE id = :id'
        );
        $stmt->execute(['id' => $id, 'spp' => $spp, 'level' => $level]);

        return $stmt->rowCount() > 0;
    }

    public function delete(int $id): bool
    {
        $stmt = $this->pdo->prepare('DELETE FROM players WHERE id = :id');
        $stmt->execute(['id' => $id]);

        return $stmt->rowCount() > 0;
    }

    public function getNextNumber(int $teamId): int
    {
        $stmt = $this->pdo->prepare(
            'SELECT COALESCE(MAX(number), 0) + 1 FROM players WHERE team_id = :team_id'
        );
        $stmt->execute(['team_id' => $teamId]);

        return (int) $stmt->fetchColumn();
    }

    public function countByPositionalTemplate(int $teamId, int $templateId): int
    {
        $stmt = $this->pdo->prepare(
            'SELECT COUNT(*) FROM players
             WHERE team_id = :team_id
             AND positional_template_id = :template_id
             AND status = \'active\''
        );
        $stmt->execute([
            'team_id' => $teamId,
            'template_id' => $templateId,
        ]);

        return (int) $stmt->fetchColumn();
    }

    public function countActive(int $teamId): int
    {
        $stmt = $this->pdo->prepare(
            'SELECT COUNT(*) FROM players WHERE team_id = :team_id AND status = \'active\''
        );
        $stmt->execute(['team_id' => $teamId]);

        return (int) $stmt->fetchColumn();
    }

    /**
     * @return list<Skill>
     */
    private function getSkillsForPlayer(int $playerId): array
    {
        $stmt = $this->pdo->prepare(
            'SELECT s.* FROM skills s
             JOIN player_skills ps ON s.id = ps.skill_id
             WHERE ps.player_id = :player_id
             ORDER BY s.name'
        );
        $stmt->execute(['player_id' => $playerId]);

        return array_values(array_map(Skill::fromRow(...), $stmt->fetchAll()));
    }
}
