<?php

declare(strict_types=1);

namespace App\Tests\Repository;

use App\Database;
use App\Repository\CoachRepository;
use App\Repository\PlayerRepository;
use App\Repository\RaceRepository;
use App\Repository\TeamRepository;
use PDO;
use PHPUnit\Framework\TestCase;

final class PlayerRepositoryTest extends TestCase
{
    private PlayerRepository $repository;
    private PDO $pdo;
    private int $teamId;
    private int $templateId;

    protected function setUp(): void
    {
        $this->pdo = Database::getConnection();
        $this->repository = new PlayerRepository($this->pdo);

        // Clean up
        $this->pdo->exec("DELETE FROM teams WHERE name LIKE 'PTest_%'");
        $this->pdo->exec("DELETE FROM coaches WHERE email LIKE '%@playertest.bb'");

        // Create test coach + team
        $coachRepo = new CoachRepository($this->pdo);
        $coach = $coachRepo->save('PTestCoach', 'coach@playertest.bb', password_hash('pw', PASSWORD_DEFAULT));

        $raceRepo = new RaceRepository($this->pdo);
        $race = $raceRepo->findAllWithPositionals()[0]; // Human

        $teamRepo = new TeamRepository($this->pdo);
        $team = $teamRepo->save([
            'coach_id' => $coach->getId(),
            'race_id' => $race->getId(),
            'name' => 'PTest_Team',
            'treasury' => 1000000,
        ]);
        $this->teamId = $team->getId();

        // Get first positional template (Lineman)
        $this->templateId = $race->getPositionals()[0]->getId();
    }

    public function testSaveAndFindById(): void
    {
        $player = $this->repository->save([
            'team_id' => $this->teamId,
            'positional_template_id' => $this->templateId,
            'name' => 'Bob the Lineman',
            'number' => 1,
            'ma' => 6,
            'st' => 3,
            'ag' => 3,
            'av' => 8,
        ]);

        $this->assertGreaterThan(0, $player->getId());
        $this->assertSame('Bob the Lineman', $player->getName());
        $this->assertSame(1, $player->getNumber());
        $this->assertSame(6, $player->getStats()->getMovement());

        $found = $this->repository->findById($player->getId());
        $this->assertNotNull($found);
        $this->assertSame('Bob the Lineman', $found->getName());
        $this->assertNotNull($found->getPositionalName());
    }

    public function testFindByTeamId(): void
    {
        $this->repository->save([
            'team_id' => $this->teamId,
            'positional_template_id' => $this->templateId,
            'name' => 'Player1',
            'number' => 1,
            'ma' => 6, 'st' => 3, 'ag' => 3, 'av' => 8,
        ]);
        $this->repository->save([
            'team_id' => $this->teamId,
            'positional_template_id' => $this->templateId,
            'name' => 'Player2',
            'number' => 2,
            'ma' => 6, 'st' => 3, 'ag' => 3, 'av' => 8,
        ]);

        $players = $this->repository->findByTeamId($this->teamId);
        $this->assertCount(2, $players);
        // Should be ordered by number
        $this->assertSame(1, $players[0]->getNumber());
        $this->assertSame(2, $players[1]->getNumber());
    }

    public function testGetNextNumber(): void
    {
        $this->assertSame(1, $this->repository->getNextNumber($this->teamId));

        $this->repository->save([
            'team_id' => $this->teamId,
            'positional_template_id' => $this->templateId,
            'name' => 'First',
            'number' => 1,
            'ma' => 6, 'st' => 3, 'ag' => 3, 'av' => 8,
        ]);

        $this->assertSame(2, $this->repository->getNextNumber($this->teamId));
    }

    public function testCountByPositionalTemplate(): void
    {
        $this->assertSame(0, $this->repository->countByPositionalTemplate($this->teamId, $this->templateId));

        $this->repository->save([
            'team_id' => $this->teamId,
            'positional_template_id' => $this->templateId,
            'name' => 'Counter',
            'number' => 1,
            'ma' => 6, 'st' => 3, 'ag' => 3, 'av' => 8,
        ]);

        $this->assertSame(1, $this->repository->countByPositionalTemplate($this->teamId, $this->templateId));
    }

    public function testCountActive(): void
    {
        $this->assertSame(0, $this->repository->countActive($this->teamId));

        $player = $this->repository->save([
            'team_id' => $this->teamId,
            'positional_template_id' => $this->templateId,
            'name' => 'ActiveGuy',
            'number' => 1,
            'ma' => 6, 'st' => 3, 'ag' => 3, 'av' => 8,
        ]);

        $this->assertSame(1, $this->repository->countActive($this->teamId));

        $this->repository->updateStatus($player->getId(), 'retired');
        $this->assertSame(0, $this->repository->countActive($this->teamId));
    }

    public function testAddStartingSkills(): void
    {
        $player = $this->repository->save([
            'team_id' => $this->teamId,
            'positional_template_id' => $this->templateId,
            'name' => 'Skilled',
            'number' => 1,
            'ma' => 6, 'st' => 3, 'ag' => 3, 'av' => 8,
        ]);

        // Get Block skill ID
        $stmt = $this->pdo->prepare("SELECT id FROM skills WHERE name = 'Block'");
        $stmt->execute();
        $blockId = (int) $stmt->fetchColumn();

        $this->repository->addStartingSkills($player->getId(), [$blockId]);

        $found = $this->repository->findById($player->getId());
        $this->assertNotNull($found);
        $this->assertCount(1, $found->getSkills());
        $this->assertSame('Block', $found->getSkills()[0]->getName());
    }

    protected function tearDown(): void
    {
        $this->pdo->exec("DELETE FROM teams WHERE name LIKE 'PTest_%'");
        $this->pdo->exec("DELETE FROM coaches WHERE email LIKE '%@playertest.bb'");
    }
}
