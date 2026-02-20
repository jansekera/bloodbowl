<?php
declare(strict_types=1);

namespace App\Tests\Engine;

use App\Engine\StrengthCalculator;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use App\ValueObject\Position;
use PHPUnit\Framework\TestCase;

final class StrengthCalculatorTest extends TestCase
{
    private StrengthCalculator $calc;

    protected function setUp(): void
    {
        $this->calc = new StrengthCalculator();
    }

    public function testBaseStrengthWithNoAssists(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 3, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, strength: 3, id: 2)
            ->build();

        $player = $state->getPlayer(1);
        $this->assertNotNull($player);
        $eff = $this->calc->calculateEffectiveStrength($state, $player, new Position(6, 5));
        $this->assertSame(3, $eff);
    }

    public function testOneAssist(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 3, id: 1)
            ->addPlayer(TeamSide::HOME, 6, 4, strength: 3, id: 3) // adjacent to target at (6,5)
            ->addPlayer(TeamSide::AWAY, 6, 5, strength: 3, id: 2)
            ->build();

        $blocker = $state->getPlayer(1);
        $this->assertNotNull($blocker);
        $assists = $this->calc->countAssists($state, $blocker, new Position(6, 5));
        $this->assertSame(1, $assists);

        $player = $state->getPlayer(1);
        $this->assertNotNull($player);
        $eff = $this->calc->calculateEffectiveStrength($state, $player, new Position(6, 5));
        $this->assertSame(4, $eff);
    }

    public function testMultipleAssists(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 3, id: 1)
            ->addPlayer(TeamSide::HOME, 6, 4, strength: 3, id: 3) // assist
            ->addPlayer(TeamSide::HOME, 6, 6, strength: 3, id: 4) // assist
            ->addPlayer(TeamSide::AWAY, 6, 5, strength: 3, id: 2)
            ->build();

        $blocker = $state->getPlayer(1);
        $this->assertNotNull($blocker);
        $assists = $this->calc->countAssists($state, $blocker, new Position(6, 5));
        $this->assertSame(2, $assists);
    }

    public function testAssistBlockedByEnemyTacklezone(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 3, id: 1)
            ->addPlayer(TeamSide::HOME, 6, 4, strength: 3, id: 3) // would-be assist
            ->addPlayer(TeamSide::AWAY, 6, 5, strength: 3, id: 2) // target
            ->addPlayer(TeamSide::AWAY, 7, 4, strength: 3, id: 4) // marks assist player
            ->build();

        // Player 3 is in TZ of enemy 4, so cannot assist
        $blocker = $state->getPlayer(1);
        $this->assertNotNull($blocker);
        $assists = $this->calc->countAssists($state, $blocker, new Position(6, 5));
        $this->assertSame(0, $assists);
    }

    public function testGuardIgnoresEnemyTacklezone(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 3, id: 1)
            ->addPlayer(TeamSide::HOME, 6, 4, strength: 3, skills: [SkillName::Guard], id: 3) // assist with Guard
            ->addPlayer(TeamSide::AWAY, 6, 5, strength: 3, id: 2) // target
            ->addPlayer(TeamSide::AWAY, 7, 4, strength: 3, id: 4) // tries to block assist
            ->build();

        // Player 3 has Guard, so TZ doesn't block the assist
        $blocker = $state->getPlayer(1);
        $this->assertNotNull($blocker);
        $assists = $this->calc->countAssists($state, $blocker, new Position(6, 5));
        $this->assertSame(1, $assists);
    }

    public function testPronePlayerCannotAssist(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 3, id: 1)
            ->addPronePlayer(TeamSide::HOME, 6, 4, id: 3) // prone, can't assist
            ->addPlayer(TeamSide::AWAY, 6, 5, strength: 3, id: 2)
            ->build();

        $blocker = $state->getPlayer(1);
        $this->assertNotNull($blocker);
        $assists = $this->calc->countAssists($state, $blocker, new Position(6, 5));
        $this->assertSame(0, $assists);
    }

    public function testFarPlayerCannotAssist(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 3, id: 1)
            ->addPlayer(TeamSide::HOME, 8, 5, strength: 3, id: 3) // too far from target
            ->addPlayer(TeamSide::AWAY, 6, 5, strength: 3, id: 2)
            ->build();

        $blocker = $state->getPlayer(1);
        $this->assertNotNull($blocker);
        $assists = $this->calc->countAssists($state, $blocker, new Position(6, 5));
        $this->assertSame(0, $assists);
    }

    public function testBlockDiceEqualStrength(): void
    {
        $info = $this->calc->getBlockDiceInfo(3, 3);
        $this->assertSame(1, $info['count']);
        $this->assertTrue($info['attackerChooses']);
    }

    public function testBlockDiceAttackerStronger(): void
    {
        $info = $this->calc->getBlockDiceInfo(4, 3);
        $this->assertSame(2, $info['count']);
        $this->assertTrue($info['attackerChooses']);
    }

    public function testBlockDiceAttackerDoubleStrength(): void
    {
        $info = $this->calc->getBlockDiceInfo(6, 3);
        $this->assertSame(3, $info['count']);
        $this->assertTrue($info['attackerChooses']);
    }

    public function testBlockDiceDefenderStronger(): void
    {
        $info = $this->calc->getBlockDiceInfo(3, 4);
        $this->assertSame(2, $info['count']);
        $this->assertFalse($info['attackerChooses']);
    }

    public function testBlockDiceDefenderDoubleStrength(): void
    {
        $info = $this->calc->getBlockDiceInfo(3, 6);
        $this->assertSame(3, $info['count']);
        $this->assertFalse($info['attackerChooses']);
    }
}
