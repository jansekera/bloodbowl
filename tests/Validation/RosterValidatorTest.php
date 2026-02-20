<?php

declare(strict_types=1);

namespace App\Tests\Validation;

use App\Database;
use App\Entity\PositionalTemplate;
use App\Repository\CoachRepository;
use App\Repository\PlayerRepository;
use App\Repository\RaceRepository;
use App\Repository\TeamRepository;
use App\Validation\RosterValidator;
use App\ValueObject\PlayerStats;
use PDO;
use PHPUnit\Framework\TestCase;

final class RosterValidatorTest extends TestCase
{
    private RosterValidator $validator;
    private PlayerRepository $playerRepository;
    private PDO $pdo;
    private int $teamId;
    private int $templateId;

    protected function setUp(): void
    {
        $this->pdo = Database::getConnection();
        $this->playerRepository = new PlayerRepository($this->pdo);
        $this->validator = new RosterValidator($this->playerRepository);

        // Clean up
        $this->pdo->exec("DELETE FROM teams WHERE name LIKE 'VTest_%'");
        $this->pdo->exec("DELETE FROM coaches WHERE email LIKE '%@validtest.bb'");

        // Create coach + team
        $coachRepo = new CoachRepository($this->pdo);
        $coach = $coachRepo->save('VCoach', 'coach@validtest.bb', password_hash('pw', PASSWORD_DEFAULT));

        $raceRepo = new RaceRepository($this->pdo);
        $race = $raceRepo->findAllWithPositionals()[0];

        $teamRepo = new TeamRepository($this->pdo);
        $team = $teamRepo->save([
            'coach_id' => $coach->getId(),
            'race_id' => $race->getId(),
            'name' => 'VTest_Team',
            'treasury' => 1000000,
        ]);
        $this->teamId = $team->getId();
        $this->templateId = $race->getPositionals()[0]->getId();
    }

    public function testValidateHirePlayerSuccess(): void
    {
        $template = new PositionalTemplate(
            id: $this->templateId,
            raceId: 1,
            name: 'Lineman',
            maxCount: 16,
            cost: 50000,
            stats: new PlayerStats(6, 3, 3, 8),
        );

        $errors = $this->validator->validateHirePlayer($this->teamId, $template, 1000000);

        $this->assertSame([], $errors);
    }

    public function testValidateHirePlayerInsufficientFunds(): void
    {
        $template = new PositionalTemplate(
            id: $this->templateId,
            raceId: 1,
            name: 'Lineman',
            maxCount: 16,
            cost: 50000,
            stats: new PlayerStats(6, 3, 3, 8),
        );

        $errors = $this->validator->validateHirePlayer($this->teamId, $template, 10000);

        $this->assertNotEmpty($errors);
        $this->assertStringContainsString('Insufficient funds', $errors[0]);
    }

    public function testValidateBuyRerollSuccess(): void
    {
        $errors = $this->validator->validateBuyReroll(60000, 100000);
        $this->assertSame([], $errors);
    }

    public function testValidateBuyRerollInsufficientFunds(): void
    {
        $errors = $this->validator->validateBuyReroll(60000, 50000);
        $this->assertNotEmpty($errors);
        $this->assertStringContainsString('Insufficient funds', $errors[0]);
    }

    public function testValidateBuyApothecarySuccess(): void
    {
        $errors = $this->validator->validateBuyApothecary(true, false, 100000);
        $this->assertSame([], $errors);
    }

    public function testValidateBuyApothecaryRaceCannotHire(): void
    {
        $errors = $this->validator->validateBuyApothecary(false, false, 100000);
        $this->assertNotEmpty($errors);
        $this->assertStringContainsString('cannot hire', $errors[0]);
    }

    public function testValidateBuyApothecaryAlreadyHas(): void
    {
        $errors = $this->validator->validateBuyApothecary(true, true, 100000);
        $this->assertNotEmpty($errors);
        $this->assertStringContainsString('already has', $errors[0]);
    }

    protected function tearDown(): void
    {
        $this->pdo->exec("DELETE FROM teams WHERE name LIKE 'VTest_%'");
        $this->pdo->exec("DELETE FROM coaches WHERE email LIKE '%@validtest.bb'");
    }
}
