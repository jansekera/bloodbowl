<?php
declare(strict_types=1);

namespace App\Tests\Engine;

use App\Engine\TacklezoneCalculator;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use App\ValueObject\Position;
use PHPUnit\Framework\TestCase;

final class TitchyTest extends TestCase
{
    private TacklezoneCalculator $calc;

    protected function setUp(): void
    {
        $this->calc = new TacklezoneCalculator();
    }

    public function testTitchyReducesDodgeTarget(): void
    {
        // Player with Titchy dodging, 1 enemy TZ at destination
        // AG3: base = 7-3 = 4, +0 TZ (first free), Titchy -1 = 3+
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, agility: 3, skills: [SkillName::Titchy], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 6, id: 10)
            ->build();

        $target = $this->calc->calculateDodgeTarget(
            $state,
            $state->getPlayer(1),
            new Position(6, 5), // destination with 1 TZ from enemy at (6,6)
            new Position(5, 5), // source
        );

        // 7-3 = 4, first TZ free (0 extra), Titchy -1 = 3
        $this->assertSame(3, $target);
    }

    public function testTitchyAndStuntyStack(): void
    {
        // Player with both Titchy + Stunty = -2 total
        // AG3: 7-3 = 4, +0 TZ, Stunty -1, Titchy -1 = 2+ (clamped)
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, agility: 3, skills: [SkillName::Titchy, SkillName::Stunty], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 6, id: 10)
            ->build();

        $target = $this->calc->calculateDodgeTarget(
            $state,
            $state->getPlayer(1),
            new Position(6, 5),
            new Position(5, 5),
        );

        // 7-3 = 4, Stunty -1, Titchy -1 = 2 (clamped at 2)
        $this->assertSame(2, $target);
    }

    public function testTitchyEnemyMakesDodgingAwayEasier(): void
    {
        // Enemy with Titchy: TZ still counted but -1 to dodge target at destination
        // AG3 player dodging to square with 1 Titchy enemy TZ
        // Base: 7-3 = 4, +0 TZ (first free), enemy Titchy -1 = 3+
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, agility: 3, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 6, skills: [SkillName::Titchy], id: 10)
            ->build();

        $target = $this->calc->calculateDodgeTarget(
            $state,
            $state->getPlayer(1),
            new Position(6, 5), // destination with 1 TZ from Titchy enemy
            new Position(5, 5),
        );

        // 7-3 = 4, first TZ free, Titchy enemy -1 = 3
        $this->assertSame(3, $target);
    }

    public function testTitchyEnemyWithMultipleTZs(): void
    {
        // 2 enemies at destination: 1 normal + 1 Titchy
        // AG3: 7-3 = 4, +1 TZ (2nd TZ = +1), Titchy enemy -1 = 4+
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, agility: 3, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 4, id: 10) // normal enemy
            ->addPlayer(TeamSide::AWAY, 6, 6, skills: [SkillName::Titchy], id: 11) // Titchy enemy
            ->build();

        $target = $this->calc->calculateDodgeTarget(
            $state,
            $state->getPlayer(1),
            new Position(6, 5), // both enemies adjacent
            new Position(5, 5),
        );

        // 7-3 = 4, 2 TZs → +1, Titchy enemy -1 = 4
        $this->assertSame(4, $target);
    }

    public function testWithoutTitchyHigherTarget(): void
    {
        // Same scenario without Titchy — verify target is 1 higher
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, agility: 3, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 6, id: 10)
            ->build();

        $target = $this->calc->calculateDodgeTarget(
            $state,
            $state->getPlayer(1),
            new Position(6, 5),
            new Position(5, 5),
        );

        // 7-3 = 4, first TZ free → 4+
        $this->assertSame(4, $target);
    }
}
