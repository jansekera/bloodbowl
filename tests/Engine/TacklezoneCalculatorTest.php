<?php
declare(strict_types=1);

namespace App\Tests\Engine;

use App\Engine\TacklezoneCalculator;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use App\ValueObject\Position;
use PHPUnit\Framework\TestCase;

final class TacklezoneCalculatorTest extends TestCase
{
    private TacklezoneCalculator $calc;

    protected function setUp(): void
    {
        $this->calc = new TacklezoneCalculator();
    }

    public function testNoEnemiesNoTacklezones(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5)
            ->build();

        $count = $this->calc->countTacklezones($state, new Position(6, 6), TeamSide::HOME);
        $this->assertSame(0, $count);
    }

    public function testAdjacentEnemyHasOneTacklezone(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5)
            ->addPlayer(TeamSide::AWAY, 6, 5)
            ->build();

        $count = $this->calc->countTacklezones($state, new Position(5, 5), TeamSide::HOME);
        $this->assertSame(1, $count);
    }

    public function testMultipleAdjacentEnemies(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5)
            ->addPlayer(TeamSide::AWAY, 4, 4)
            ->addPlayer(TeamSide::AWAY, 6, 4)
            ->addPlayer(TeamSide::AWAY, 5, 4)
            ->build();

        $count = $this->calc->countTacklezones($state, new Position(5, 5), TeamSide::HOME);
        $this->assertSame(3, $count);
    }

    public function testProneEnemyDoesNotExertTacklezone(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5)
            ->addPronePlayer(TeamSide::AWAY, 6, 5)
            ->build();

        $count = $this->calc->countTacklezones($state, new Position(5, 5), TeamSide::HOME);
        $this->assertSame(0, $count);
    }

    public function testFarEnemyDoesNotExertTacklezone(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5)
            ->addPlayer(TeamSide::AWAY, 8, 5)
            ->build();

        $count = $this->calc->countTacklezones($state, new Position(5, 5), TeamSide::HOME);
        $this->assertSame(0, $count);
    }

    public function testIsInTacklezone(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5)
            ->build();

        $player = $state->requirePlayer(1);
        $this->assertTrue($this->calc->isInTacklezone($state, $player));
    }

    public function testIsNotInTacklezone(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, id: 1)
            ->addPlayer(TeamSide::AWAY, 10, 10)
            ->build();

        $player = $state->requirePlayer(1);
        $this->assertFalse($this->calc->isInTacklezone($state, $player));
    }

    public function testGetMarkingPlayers(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5)
            ->addPlayer(TeamSide::AWAY, 4, 4, id: 10)
            ->addPlayer(TeamSide::AWAY, 6, 6, id: 11)
            ->addPlayer(TeamSide::AWAY, 10, 10, id: 12) // far away
            ->build();

        $markers = $this->calc->getMarkingPlayers($state, new Position(5, 5), TeamSide::HOME);
        $this->assertCount(2, $markers);

        $markerIds = array_map(fn($p) => $p->getId(), $markers);
        $this->assertContains(10, $markerIds);
        $this->assertContains(11, $markerIds);
    }

    public function testDodgeTargetBasicCalculation(): void
    {
        // AG 3 player, 1 TZ at destination: 7 - 3 + max(0, 1-1) = 4+
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, agility: 3, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 6) // adjacent to destination (6,5)
            ->build();

        $player = $state->requirePlayer(1);
        $destination = new Position(6, 5);
        $target = $this->calc->calculateDodgeTarget($state, $player, $destination);
        $this->assertSame(4, $target); // 7 - 3 + max(0, 1-1) = 4
    }

    public function testDodgeTargetMinimum2(): void
    {
        // AG 6 player (hypothetical), 0 TZ = 7 - 6 = 1, clamped to 2
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, agility: 6, id: 1)
            ->build();

        $player = $state->requirePlayer(1);
        $target = $this->calc->calculateDodgeTarget($state, $player, new Position(6, 5));
        $this->assertSame(2, $target);
    }

    public function testDodgeTargetMaximum6(): void
    {
        // AG 1 player with many TZ: 7 - 1 + 2 = 8, clamped to 6
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, agility: 1, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 4)
            ->addPlayer(TeamSide::AWAY, 6, 6)
            ->addPlayer(TeamSide::AWAY, 7, 5)
            ->build();

        $player = $state->requirePlayer(1);
        $target = $this->calc->calculateDodgeTarget($state, $player, new Position(6, 5));
        $this->assertSame(6, $target);
    }

    public function testDodgeSkillReducesTarget(): void
    {
        // AG 3, 1 TZ: normally 7-3+0 = 4+, with Dodge: 3+
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, agility: 3, skills: [SkillName::Dodge], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 6) // adjacent to dest
            ->build();

        $player = $state->requirePlayer(1);
        $target = $this->calc->calculateDodgeTarget($state, $player, new Position(6, 5));
        // ⛔ OPRAVENO 14.09.2026. Puvodni ocekavani 3 zapisovalo DVOJI zapocteni
        //   skillu Dodge: cil se snizoval o 1 A JESTE se pri neuspechu hazelo
        //   znovu (`MoveHandler.php:250`). Pravidla davaji jen re-roll.
        //   Podle pravidel: AG3 => tabulka 4+, "+1 Making a Dodge roll",
        //   "-1 per opposing tackle zone on the square dodging to" (tady 1).
        //   => 4 - 1 + 1 = 4. Skill Dodge se v cili neprojevi vubec.
        $this->assertSame(4, $target);
    }
}
