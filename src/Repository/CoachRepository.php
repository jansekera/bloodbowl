<?php

declare(strict_types=1);

namespace App\Repository;

use App\Database;
use App\Entity\Coach;
use PDO;

final class CoachRepository
{
    private PDO $pdo;

    public function __construct(?PDO $pdo = null)
    {
        $this->pdo = $pdo ?? Database::getConnection();
    }

    public function findById(int $id): ?Coach
    {
        $stmt = $this->pdo->prepare('SELECT * FROM coaches WHERE id = :id');
        $stmt->execute(['id' => $id]);
        $row = $stmt->fetch();

        return $row !== false ? Coach::fromRow($row) : null;
    }

    public function findByEmail(string $email): ?Coach
    {
        $stmt = $this->pdo->prepare('SELECT * FROM coaches WHERE email = :email');
        $stmt->execute(['email' => $email]);
        $row = $stmt->fetch();

        return $row !== false ? Coach::fromRow($row) : null;
    }

    /**
     * @return list<Coach>
     */
    public function findAll(): array
    {
        $stmt = $this->pdo->prepare('SELECT * FROM coaches ORDER BY name');
        $stmt->execute();

        return array_values(array_map(Coach::fromRow(...), $stmt->fetchAll()));
    }

    public function save(string $name, string $email, string $passwordHash): Coach
    {
        $stmt = $this->pdo->prepare(
            'INSERT INTO coaches (name, email, password_hash)
             VALUES (:name, :email, :password_hash)
             RETURNING *'
        );
        $stmt->execute([
            'name' => $name,
            'email' => $email,
            'password_hash' => $passwordHash,
        ]);

        /** @var array<string, mixed> $row */
        $row = $stmt->fetch();
        return Coach::fromRow($row);
    }

    public function emailExists(string $email): bool
    {
        $stmt = $this->pdo->prepare('SELECT COUNT(*) FROM coaches WHERE email = :email');
        $stmt->execute(['email' => $email]);

        return (int) $stmt->fetchColumn() > 0;
    }
}
