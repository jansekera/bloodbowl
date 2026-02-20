<?php
declare(strict_types=1);

namespace App\Repository;

use App\Database;
use App\DTO\GameEvent;
use PDO;

final class MatchEventRepository
{
    private PDO $pdo;

    public function __construct(?PDO $pdo = null)
    {
        $this->pdo = $pdo ?? Database::getConnection();
    }

    public function save(int $matchId, GameEvent $event): void
    {
        // Get next sequence number
        $stmt = $this->pdo->prepare(
            'SELECT COALESCE(MAX(sequence_number), 0) + 1 FROM match_events WHERE match_id = :match_id'
        );
        $stmt->execute(['match_id' => $matchId]);
        $sequence = (int) $stmt->fetchColumn();

        $stmt = $this->pdo->prepare(
            'INSERT INTO match_events (match_id, sequence_number, event_type, description, event_data)
             VALUES (:match_id, :sequence_number, :event_type, :description, :event_data)'
        );
        $stmt->execute([
            'match_id' => $matchId,
            'sequence_number' => $sequence,
            'event_type' => $event->getType(),
            'description' => $event->getDescription(),
            'event_data' => json_encode($event->getData()),
        ]);
    }

    /**
     * @param list<GameEvent> $events
     */
    public function saveAll(int $matchId, array $events): void
    {
        foreach ($events as $event) {
            $this->save($matchId, $event);
        }
    }

    /**
     * @return list<array<string, mixed>>
     */
    public function findByMatchId(int $matchId, int $limit = 50, int $offset = 0): array
    {
        $stmt = $this->pdo->prepare(
            'SELECT * FROM match_events
             WHERE match_id = :match_id
             ORDER BY sequence_number DESC
             LIMIT :limit OFFSET :offset'
        );
        $stmt->bindValue('match_id', $matchId, PDO::PARAM_INT);
        $stmt->bindValue('limit', $limit, PDO::PARAM_INT);
        $stmt->bindValue('offset', $offset, PDO::PARAM_INT);
        $stmt->execute();

        return array_values($stmt->fetchAll());
    }

    /**
     * @return list<GameEvent>
     */
    public function findAllGameEvents(int $matchId): array
    {
        $stmt = $this->pdo->prepare(
            'SELECT * FROM match_events
             WHERE match_id = :match_id
             ORDER BY sequence_number ASC'
        );
        $stmt->execute(['match_id' => $matchId]);

        $events = [];
        foreach ($stmt->fetchAll() as $row) {
            /** @var array<string, mixed> $data */
            $data = json_decode((string) $row['event_data'], true) ?? [];
            $events[] = new GameEvent(
                (string) $row['event_type'],
                (string) $row['description'],
                $data,
            );
        }

        return $events;
    }

    public function getEventCount(int $matchId): int
    {
        $stmt = $this->pdo->prepare(
            'SELECT COUNT(*) FROM match_events WHERE match_id = :match_id'
        );
        $stmt->execute(['match_id' => $matchId]);

        return (int) $stmt->fetchColumn();
    }
}
