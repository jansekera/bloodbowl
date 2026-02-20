<?php
declare(strict_types=1);

namespace App\Tests\DTO;

use App\DTO\TeamStateDTO;
use App\Enum\TeamSide;
use PHPUnit\Framework\TestCase;

final class TeamStateDTOTest extends TestCase
{
    public function testCreateSetsDefaults(): void
    {
        $team = TeamStateDTO::create(1, 'Test', 'Human', TeamSide::HOME, 3);

        $this->assertSame(0, $team->getScore());
        $this->assertSame(3, $team->getRerolls());
        $this->assertFalse($team->isRerollUsedThisTurn());
        $this->assertSame(1, $team->getTurnNumber());
        $this->assertTrue($team->canUseReroll());
    }

    public function testWithRerollUsed(): void
    {
        $team = TeamStateDTO::create(1, 'Test', 'Human', TeamSide::HOME, 3);
        $used = $team->withRerollUsed();

        $this->assertSame(2, $used->getRerolls());
        $this->assertTrue($used->isRerollUsedThisTurn());
        $this->assertFalse($used->canUseReroll());
    }

    public function testCannotUseRerollWithZeroRerolls(): void
    {
        $team = TeamStateDTO::create(1, 'Test', 'Human', TeamSide::HOME, 0);
        $this->assertFalse($team->canUseReroll());
    }

    public function testResetForNewTurn(): void
    {
        $team = TeamStateDTO::create(1, 'Test', 'Human', TeamSide::HOME, 3);
        $used = $team->withRerollUsed()->withBlitzUsed()->withPassUsed();
        $reset = $used->resetForNewTurn();

        $this->assertFalse($reset->isRerollUsedThisTurn());
        $this->assertFalse($reset->isBlitzUsedThisTurn());
        $this->assertFalse($reset->isPassUsedThisTurn());
        $this->assertSame(2, $reset->getTurnNumber());
        $this->assertSame(2, $reset->getRerolls()); // rerolls don't reset
    }

    public function testSerializationRoundTrip(): void
    {
        $team = TeamStateDTO::create(1, 'Test', 'Human', TeamSide::HOME, 3);
        $team = $team->withScore(2)->withBlitzUsed();
        $array = $team->toArray();
        $restored = TeamStateDTO::fromArray($array);

        $this->assertSame($team->getTeamId(), $restored->getTeamId());
        $this->assertSame($team->getScore(), $restored->getScore());
        $this->assertSame($team->isBlitzUsedThisTurn(), $restored->isBlitzUsedThisTurn());
    }
}
