<?php

declare(strict_types=1);

namespace App\Tests\Repository;

use App\Database;
use App\Repository\CoachRepository;
use App\Repository\RaceRepository;
use App\Repository\TeamRepository;
use PDO;
use PHPUnit\Framework\TestCase;

final class TeamRepositoryTest extends TestCase
{
    private TeamRepository $repository;
    private PDO $pdo;
    private int $coachId;
    private int $raceId;

    protected function setUp(): void
    {
        $this->pdo = Database::getConnection();
        $this->repository = new TeamRepository($this->pdo);

        // Clean up
        $this->pdo->exec("DELETE FROM teams WHERE name LIKE 'Test_%'");
        $this->pdo->exec("DELETE FROM coaches WHERE email LIKE '%@teamtest.bb'");

        // Create test coach
        $coachRepo = new CoachRepository($this->pdo);
        $coach = $coachRepo->save('TestCoach', 'coach@teamtest.bb', password_hash('pw', PASSWORD_DEFAULT));
        $this->coachId = $coach->getId();

        // Get a race
        $raceRepo = new RaceRepository($this->pdo);
        $races = $raceRepo->findAll();
        $this->raceId = $races[0]->getId();
    }

    public function testSaveAndFindById(): void
    {
        $team = $this->repository->save([
            'coach_id' => $this->coachId,
            'race_id' => $this->raceId,
            'name' => 'Test_Reavers',
            'treasury' => 1000000,
        ]);

        $this->assertGreaterThan(0, $team->getId());
        $this->assertSame('Test_Reavers', $team->getName());
        $this->assertSame(1000000, $team->getTreasury()->getGold());
        $this->assertSame(0, $team->getRerolls());
        $this->assertFalse($team->hasApothecary());

        $found = $this->repository->findById($team->getId());
        $this->assertNotNull($found);
        $this->assertSame('Test_Reavers', $found->getName());
        $this->assertNotNull($found->getRaceName());
        $this->assertNotNull($found->getCoachName());
    }

    public function testFindByCoachId(): void
    {
        $this->repository->save([
            'coach_id' => $this->coachId,
            'race_id' => $this->raceId,
            'name' => 'Test_TeamA',
            'treasury' => 1000000,
        ]);
        $this->repository->save([
            'coach_id' => $this->coachId,
            'race_id' => $this->raceId,
            'name' => 'Test_TeamB',
            'treasury' => 1000000,
        ]);

        $teams = $this->repository->findByCoachId($this->coachId);

        $this->assertGreaterThanOrEqual(2, count($teams));
    }

    public function testUpdateTreasury(): void
    {
        $team = $this->repository->save([
            'coach_id' => $this->coachId,
            'race_id' => $this->raceId,
            'name' => 'Test_Treasury',
            'treasury' => 1000000,
        ]);

        $this->repository->updateTreasury($team->getId(), 750000);

        $updated = $this->repository->findById($team->getId());
        $this->assertNotNull($updated);
        $this->assertSame(750000, $updated->getTreasury()->getGold());
    }

    public function testUpdateRerolls(): void
    {
        $team = $this->repository->save([
            'coach_id' => $this->coachId,
            'race_id' => $this->raceId,
            'name' => 'Test_Rerolls',
            'treasury' => 1000000,
        ]);

        $this->repository->updateRerolls($team->getId(), 2);

        $updated = $this->repository->findById($team->getId());
        $this->assertNotNull($updated);
        $this->assertSame(2, $updated->getRerolls());
    }

    public function testUpdateApothecary(): void
    {
        $team = $this->repository->save([
            'coach_id' => $this->coachId,
            'race_id' => $this->raceId,
            'name' => 'Test_Apo',
            'treasury' => 1000000,
        ]);

        $this->assertFalse($team->hasApothecary());
        $this->repository->updateApothecary($team->getId(), true);

        $updated = $this->repository->findById($team->getId());
        $this->assertNotNull($updated);
        $this->assertTrue($updated->hasApothecary());
    }

    public function testUpdateStatus(): void
    {
        $team = $this->repository->save([
            'coach_id' => $this->coachId,
            'race_id' => $this->raceId,
            'name' => 'Test_Status',
            'treasury' => 1000000,
        ]);

        $this->repository->updateStatus($team->getId(), 'retired');

        // Retired teams shouldn't show in findByCoachId
        $teams = $this->repository->findByCoachId($this->coachId);
        $ids = array_map(fn($t) => $t->getId(), $teams);
        $this->assertNotContains($team->getId(), $ids);
    }

    public function testNameExistsForCoach(): void
    {
        $this->repository->save([
            'coach_id' => $this->coachId,
            'race_id' => $this->raceId,
            'name' => 'Test_Unique',
            'treasury' => 1000000,
        ]);

        $this->assertTrue($this->repository->nameExistsForCoach($this->coachId, 'Test_Unique'));
        $this->assertFalse($this->repository->nameExistsForCoach($this->coachId, 'Test_NoExist'));
    }

    protected function tearDown(): void
    {
        $this->pdo->exec("DELETE FROM teams WHERE name LIKE 'Test_%'");
        $this->pdo->exec("DELETE FROM coaches WHERE email LIKE '%@teamtest.bb'");
    }
}
