<?php

declare(strict_types=1);

namespace App\Tests\Service;

use App\Database;
use App\Exception\NotFoundException;
use App\Exception\ValidationException;
use App\Repository\CoachRepository;
use App\Repository\PlayerRepository;
use App\Repository\RaceRepository;
use App\Repository\TeamRepository;
use App\Service\TeamService;
use App\Validation\RosterValidator;
use PDO;
use PHPUnit\Framework\TestCase;

final class TeamServiceTest extends TestCase
{
    private TeamService $service;
    private TeamRepository $teamRepository;
    private PlayerRepository $playerRepository;
    private RaceRepository $raceRepository;
    private PDO $pdo;
    private int $coachId;
    private int $raceId;

    protected function setUp(): void
    {
        $this->pdo = Database::getConnection();
        $this->teamRepository = new TeamRepository($this->pdo);
        $this->playerRepository = new PlayerRepository($this->pdo);
        $this->raceRepository = new RaceRepository($this->pdo);

        $this->service = new TeamService(
            $this->teamRepository,
            $this->playerRepository,
            $this->raceRepository,
            new RosterValidator($this->playerRepository),
        );

        // Clean up
        $this->pdo->exec("DELETE FROM teams WHERE name LIKE 'STest_%'");
        $this->pdo->exec("DELETE FROM coaches WHERE email LIKE '%@svctest.bb'");

        // Create test coach
        $coachRepo = new CoachRepository($this->pdo);
        $coach = $coachRepo->save('SVCCoach', 'coach@svctest.bb', password_hash('pw', PASSWORD_DEFAULT));
        $this->coachId = $coach->getId();

        // Get race (Human)
        $races = $this->raceRepository->findAll();
        $humanRace = null;
        foreach ($races as $race) {
            if ($race->getName() === 'Human') {
                $humanRace = $race;
                break;
            }
        }
        $this->raceId = $humanRace !== null ? $humanRace->getId() : $races[0]->getId();
    }

    public function testCreateTeam(): void
    {
        $team = $this->service->createTeam($this->coachId, $this->raceId, 'STest_Reavers');

        $this->assertSame('STest_Reavers', $team->getName());
        $this->assertSame($this->coachId, $team->getCoachId());
        $this->assertSame($this->raceId, $team->getRaceId());
        $this->assertSame(1000000, $team->getTreasury()->getGold());
    }

    public function testCreateTeamEmptyName(): void
    {
        $this->expectException(ValidationException::class);
        $this->service->createTeam($this->coachId, $this->raceId, '');
    }

    public function testCreateTeamInvalidRace(): void
    {
        $this->expectException(ValidationException::class);
        $this->service->createTeam($this->coachId, 99999, 'STest_Bad');
    }

    public function testCreateTeamDuplicateName(): void
    {
        $this->service->createTeam($this->coachId, $this->raceId, 'STest_Dupe');

        $this->expectException(ValidationException::class);
        $this->service->createTeam($this->coachId, $this->raceId, 'STest_Dupe');
    }

    public function testHirePlayer(): void
    {
        $team = $this->service->createTeam($this->coachId, $this->raceId, 'STest_Hire');
        $race = $this->raceRepository->findByIdWithPositionals($this->raceId);
        $this->assertNotNull($race);

        // Find lineman template
        $linemanId = null;
        foreach ($race->getPositionals() as $pos) {
            if ($pos->getName() === 'Lineman') {
                $linemanId = $pos->getId();
                break;
            }
        }
        $this->assertNotNull($linemanId);

        $player = $this->service->hirePlayer($team->getId(), $linemanId, 'Bob');

        $this->assertSame('Bob', $player->getName());
        $this->assertSame(1, $player->getNumber());
        $this->assertSame(6, $player->getStats()->getMovement());

        // Treasury should be reduced
        $updatedTeam = $this->teamRepository->findById($team->getId());
        $this->assertNotNull($updatedTeam);
        $this->assertSame(950000, $updatedTeam->getTreasury()->getGold()); // 1M - 50k
    }

    public function testHireBlitzerHasBlock(): void
    {
        $team = $this->service->createTeam($this->coachId, $this->raceId, 'STest_Blitz');
        $race = $this->raceRepository->findByIdWithPositionals($this->raceId);
        $this->assertNotNull($race);

        $blitzerId = null;
        foreach ($race->getPositionals() as $pos) {
            if ($pos->getName() === 'Blitzer') {
                $blitzerId = $pos->getId();
                break;
            }
        }
        $this->assertNotNull($blitzerId);

        $player = $this->service->hirePlayer($team->getId(), $blitzerId, 'Griff');

        $skillNames = array_map(fn($s) => $s->getName(), $player->getSkills());
        $this->assertContains('Block', $skillNames);
    }

    public function testHirePlayerEmptyName(): void
    {
        $team = $this->service->createTeam($this->coachId, $this->raceId, 'STest_NoName');
        $race = $this->raceRepository->findByIdWithPositionals($this->raceId);
        $this->assertNotNull($race);
        $templateId = $race->getPositionals()[0]->getId();

        $this->expectException(ValidationException::class);
        $this->service->hirePlayer($team->getId(), $templateId, '');
    }

    public function testHirePlayerInvalidTeam(): void
    {
        $race = $this->raceRepository->findByIdWithPositionals($this->raceId);
        $this->assertNotNull($race);
        $templateId = $race->getPositionals()[0]->getId();

        $this->expectException(NotFoundException::class);
        $this->service->hirePlayer(99999, $templateId, 'Nobody');
    }

    public function testFirePlayer(): void
    {
        $team = $this->service->createTeam($this->coachId, $this->raceId, 'STest_Fire');
        $race = $this->raceRepository->findByIdWithPositionals($this->raceId);
        $this->assertNotNull($race);
        $templateId = $race->getPositionals()[0]->getId();

        $player = $this->service->hirePlayer($team->getId(), $templateId, 'Fired');
        $this->assertTrue($player->isActive());

        $this->service->firePlayer($team->getId(), $player->getId());

        $fired = $this->playerRepository->findById($player->getId());
        $this->assertNotNull($fired);
        $this->assertFalse($fired->isActive());
    }

    public function testFirePlayerWrongTeam(): void
    {
        $team = $this->service->createTeam($this->coachId, $this->raceId, 'STest_WrongFire');
        $race = $this->raceRepository->findByIdWithPositionals($this->raceId);
        $this->assertNotNull($race);
        $templateId = $race->getPositionals()[0]->getId();

        $player = $this->service->hirePlayer($team->getId(), $templateId, 'Wrong');

        $this->expectException(NotFoundException::class);
        $this->service->firePlayer(99999, $player->getId());
    }

    public function testBuyReroll(): void
    {
        $team = $this->service->createTeam($this->coachId, $this->raceId, 'STest_Reroll');

        $this->service->buyReroll($team->getId());

        $updated = $this->teamRepository->findById($team->getId());
        $this->assertNotNull($updated);
        $this->assertSame(1, $updated->getRerolls());
        $this->assertSame(940000, $updated->getTreasury()->getGold()); // 1M - 60k
    }

    public function testBuyApothecary(): void
    {
        $team = $this->service->createTeam($this->coachId, $this->raceId, 'STest_Apo');
        $this->assertFalse($team->hasApothecary());

        $this->service->buyApothecary($team->getId());

        $updated = $this->teamRepository->findById($team->getId());
        $this->assertNotNull($updated);
        $this->assertTrue($updated->hasApothecary());
        $this->assertSame(950000, $updated->getTreasury()->getGold()); // 1M - 50k
    }

    public function testBuyApothecaryTwiceFails(): void
    {
        $team = $this->service->createTeam($this->coachId, $this->raceId, 'STest_ApoTwice');
        $this->service->buyApothecary($team->getId());

        $this->expectException(ValidationException::class);
        $this->service->buyApothecary($team->getId());
    }

    public function testRetireTeam(): void
    {
        $team = $this->service->createTeam($this->coachId, $this->raceId, 'STest_Retire');

        $this->service->retireTeam($team->getId());

        // Should not appear in active teams
        $teams = $this->teamRepository->findByCoachId($this->coachId);
        $ids = array_map(fn($t) => $t->getId(), $teams);
        $this->assertNotContains($team->getId(), $ids);
    }

    public function testHireMultiplePlayersNumbersIncrement(): void
    {
        $team = $this->service->createTeam($this->coachId, $this->raceId, 'STest_Numbers');
        $race = $this->raceRepository->findByIdWithPositionals($this->raceId);
        $this->assertNotNull($race);
        $templateId = $race->getPositionals()[0]->getId();

        $p1 = $this->service->hirePlayer($team->getId(), $templateId, 'First');
        $p2 = $this->service->hirePlayer($team->getId(), $templateId, 'Second');
        $p3 = $this->service->hirePlayer($team->getId(), $templateId, 'Third');

        $this->assertSame(1, $p1->getNumber());
        $this->assertSame(2, $p2->getNumber());
        $this->assertSame(3, $p3->getNumber());
    }

    protected function tearDown(): void
    {
        $this->pdo->exec("DELETE FROM teams WHERE name LIKE 'STest_%'");
        $this->pdo->exec("DELETE FROM coaches WHERE email LIKE '%@svctest.bb'");
    }
}
