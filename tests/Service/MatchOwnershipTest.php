<?php

declare(strict_types=1);

namespace App\Tests\Service;

use App\Container\Container;
use App\Container\ServiceProvider;
use App\Database;
use App\Enum\ActionType;
use App\Enum\TeamSide;
use App\Exception\ForbiddenException;
use App\Exception\NotFoundException;
use App\Exception\ValidationException;
use App\Repository\CoachRepository;
use App\Repository\MatchRepository;
use App\Repository\RaceRepository;
use App\Service\MatchService;
use App\Service\TeamService;
use PDO;
use PHPUnit\Framework\TestCase;

/**
 * P112(a, b) — security review 01.10.2026:
 *  (b) createMatch nekontroloval, čí jsou týmy ⇒ šlo hrát s cizí soupiskou a SPP se zapsaly cizím hráčům;
 *  (a) submitAction nekontroloval, kdo táhne ⇒ kdokoli přihlášený táhl v cizím zápase za kteroukoli stranu.
 * Zápas jednoho člověka (hot-seat, proti AI) zůstává jako dřív: domácí kouč smí táhnout za obě strany.
 */
final class MatchOwnershipTest extends TestCase
{
    private PDO $pdo;
    private MatchService $matches;
    private TeamService $teams;
    private int $owner;
    private int $other;

    protected function setUp(): void
    {
        $this->pdo = Database::getConnection();
        $this->cleanUp();

        $container = new Container();
        ServiceProvider::register($container);
        $this->matches = $container->get(MatchService::class);
        $this->teams = $container->get(TeamService::class);

        $coaches = new CoachRepository($this->pdo);
        $this->owner = $coaches->save('MOwner', 'owner@mtest.bb', password_hash('pw', PASSWORD_DEFAULT))->getId();
        $this->other = $coaches->save('MOther', 'other@mtest.bb', password_hash('pw', PASSWORD_DEFAULT))->getId();
    }

    protected function tearDown(): void
    {
        $this->cleanUp();
    }

    private function cleanUp(): void
    {
        $this->pdo->exec("DELETE FROM matches WHERE home_team_id IN (SELECT id FROM teams WHERE name LIKE 'MTest_%')
                                              OR away_team_id IN (SELECT id FROM teams WHERE name LIKE 'MTest_%')");
        $this->pdo->exec("DELETE FROM teams WHERE name LIKE 'MTest_%'");
        $this->pdo->exec("DELETE FROM coaches WHERE email LIKE '%@mtest.bb'");
    }

    /** Team of 11 linemen (the minimum to play). */
    private function team(int $coachId, string $name): int
    {
        $races = new RaceRepository($this->pdo);
        $human = array_values(array_filter($races->findAll(), fn($r) => $r->getName() === 'Human'))[0];
        $race = $races->findByIdWithPositionals($human->getId());
        $this->assertNotNull($race);
        $lineman = array_values(array_filter($race->getPositionals(), fn($p) => $p->getName() === 'Lineman'))[0];

        $team = $this->teams->createTeam($coachId, $human->getId(), 'MTest_' . $name);
        for ($i = 1; $i <= 11; $i++) {
            $this->teams->hirePlayer($team->getId(), $lineman->getId(), "L$i");
        }
        return $team->getId();
    }

    // ---- (b) who may create a match with which teams

    public function testOwnerCreatesHotSeatMatchWithOwnTeams(): void
    {
        $state = $this->matches->createMatch($this->team($this->owner, 'A'), $this->team($this->owner, 'B'), $this->owner);
        $this->assertGreaterThan(0, $state->getMatchId());
    }

    public function testOtherCoachCannotUseForeignHomeTeam(): void
    {
        $foreign = $this->team($this->owner, 'A');
        $own = $this->team($this->other, 'B');
        $this->expectException(NotFoundException::class);
        $this->matches->createMatch($foreign, $own, $this->other);
    }

    public function testOtherCoachCannotUseForeignAwayTeam(): void
    {
        $own = $this->team($this->other, 'A');
        $foreign = $this->team($this->owner, 'B');
        $this->expectException(NotFoundException::class);
        $this->matches->createMatch($own, $foreign, $this->other);
    }

    public function testRetiredTeamCannotPlay(): void
    {
        $a = $this->team($this->owner, 'A');
        $b = $this->team($this->owner, 'B');
        $this->teams->retireTeam($b);
        $this->expectException(ValidationException::class);
        $this->matches->createMatch($a, $b, $this->owner);
    }

    // ---- (a) who may submit an action

    public function testStrangerCannotActInAMatch(): void
    {
        $state = $this->matches->createMatch($this->team($this->owner, 'A'), $this->team($this->owner, 'B'), $this->owner);
        $this->expectException(ForbiddenException::class);
        $this->matches->submitAction($state->getMatchId(), ActionType::END_TURN, [], $this->other);
    }

    public function testHotSeatOwnerMayActForTheActiveSide(): void
    {
        $state = $this->matches->createMatch($this->team($this->owner, 'A'), $this->team($this->owner, 'B'), $this->owner);
        // Not forbidden — whatever the game does with END_TURN in this phase is not this test's business.
        $result = $this->matches->submitAction($state->getMatchId(), ActionType::END_TURN, [], $this->owner);
        $this->assertSame($state->getMatchId(), $result->getNewState()->getMatchId());
    }

    public function testInTwoCoachMatchEachCoachActsOnlyForOwnSide(): void
    {
        $state = $this->matches->createMatch(
            $this->team($this->owner, 'A'),
            $this->team($this->other, 'B'),
            $this->owner,
            awayCoachId: $this->other,
        );
        $notOnTurn = $state->getActiveTeam() === TeamSide::HOME ? $this->other : $this->owner;

        $this->expectException(ForbiddenException::class);
        $this->matches->submitAction($state->getMatchId(), ActionType::END_TURN, [], $notOnTurn);
    }
}
