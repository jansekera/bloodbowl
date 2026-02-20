<?php

declare(strict_types=1);

namespace App\Tests\Repository;

use App\Database;
use App\Repository\CoachRepository;
use PHPUnit\Framework\TestCase;

final class CoachRepositoryTest extends TestCase
{
    private CoachRepository $repository;

    protected function setUp(): void
    {
        $pdo = Database::getConnection();
        $this->repository = new CoachRepository($pdo);

        // Clean up test coaches
        $pdo->exec("DELETE FROM coaches WHERE email LIKE '%@test.bloodbowl%'");
    }

    public function testSaveAndFindById(): void
    {
        $hash = password_hash('password123', PASSWORD_DEFAULT);
        $coach = $this->repository->save('TestCoach', 'save@test.bloodbowl', $hash);

        $this->assertGreaterThan(0, $coach->getId());
        $this->assertSame('TestCoach', $coach->getName());
        $this->assertSame('save@test.bloodbowl', $coach->getEmail());

        $found = $this->repository->findById($coach->getId());
        $this->assertNotNull($found);
        $this->assertSame('TestCoach', $found->getName());
    }

    public function testFindByEmail(): void
    {
        $hash = password_hash('password123', PASSWORD_DEFAULT);
        $this->repository->save('EmailCoach', 'email@test.bloodbowl', $hash);

        $found = $this->repository->findByEmail('email@test.bloodbowl');
        $this->assertNotNull($found);
        $this->assertSame('EmailCoach', $found->getName());
    }

    public function testFindByEmailNotFound(): void
    {
        $found = $this->repository->findByEmail('nonexistent@test.bloodbowl');
        $this->assertNull($found);
    }

    public function testEmailExists(): void
    {
        $hash = password_hash('password123', PASSWORD_DEFAULT);
        $this->repository->save('ExistsCoach', 'exists@test.bloodbowl', $hash);

        $this->assertTrue($this->repository->emailExists('exists@test.bloodbowl'));
        $this->assertFalse($this->repository->emailExists('nope@test.bloodbowl'));
    }

    public function testFindAll(): void
    {
        $hash = password_hash('password123', PASSWORD_DEFAULT);
        $this->repository->save('AllCoach1', 'all1@test.bloodbowl', $hash);
        $this->repository->save('AllCoach2', 'all2@test.bloodbowl', $hash);

        $all = $this->repository->findAll();
        $this->assertGreaterThanOrEqual(2, count($all));
    }

    protected function tearDown(): void
    {
        $pdo = Database::getConnection();
        $pdo->exec("DELETE FROM coaches WHERE email LIKE '%@test.bloodbowl%'");
    }
}
