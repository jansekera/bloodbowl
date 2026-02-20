<?php
declare(strict_types=1);

namespace App\Tests\DTO;

use App\DTO\MatchPlayerDTO;
use App\Enum\PlayerState;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use App\ValueObject\PlayerStats;
use App\ValueObject\Position;
use PHPUnit\Framework\TestCase;

final class MatchPlayerDTOTest extends TestCase
{
    private function createPlayer(): MatchPlayerDTO
    {
        return MatchPlayerDTO::create(
            id: 1,
            playerId: 10,
            name: 'Test Player',
            number: 7,
            positionalName: 'Blitzer',
            stats: new PlayerStats(7, 3, 3, 8),
            skills: [SkillName::Block, SkillName::Dodge],
            teamSide: TeamSide::HOME,
            position: new Position(5, 5),
        );
    }

    public function testCreateSetsDefaults(): void
    {
        $player = $this->createPlayer();

        $this->assertSame(1, $player->getId());
        $this->assertSame(10, $player->getPlayerId());
        $this->assertSame('Test Player', $player->getName());
        $this->assertSame(7, $player->getNumber());
        $this->assertSame('Blitzer', $player->getPositionalName());
        $this->assertSame(PlayerState::STANDING, $player->getState());
        $this->assertFalse($player->hasMoved());
        $this->assertFalse($player->hasActed());
        $this->assertSame(7, $player->getMovementRemaining());
        $this->assertTrue($player->canAct());
        $this->assertTrue($player->canMove());
    }

    public function testHasSkill(): void
    {
        $player = $this->createPlayer();
        $this->assertTrue($player->hasSkill(SkillName::Block));
        $this->assertTrue($player->hasSkill(SkillName::Dodge));
        $this->assertFalse($player->hasSkill(SkillName::SureHands));
    }

    public function testWitherMethods(): void
    {
        $player = $this->createPlayer();

        $moved = $player->withHasMoved(true);
        $this->assertTrue($moved->hasMoved());
        $this->assertFalse($player->hasMoved()); // original unchanged

        $fallen = $player->withState(PlayerState::PRONE);
        $this->assertSame(PlayerState::PRONE, $fallen->getState());
        $this->assertFalse($fallen->canAct());

        $repositioned = $player->withPosition(new Position(10, 10));
        $this->assertSame(10, $repositioned->getPosition()?->getX());
    }

    public function testCanActWhenStandingAndNotActed(): void
    {
        $player = $this->createPlayer();
        $this->assertTrue($player->canAct());

        $acted = $player->withHasActed(true);
        $this->assertFalse($acted->canAct());

        $prone = $player->withState(PlayerState::PRONE);
        $this->assertFalse($prone->canAct());
    }

    public function testSerializationRoundTrip(): void
    {
        $player = $this->createPlayer();
        $array = $player->toArray();
        $restored = MatchPlayerDTO::fromArray($array);

        $this->assertSame($player->getId(), $restored->getId());
        $this->assertSame($player->getName(), $restored->getName());
        $this->assertSame($player->getNumber(), $restored->getNumber());
        $this->assertSame($player->getState(), $restored->getState());
        $this->assertSame($player->getTeamSide(), $restored->getTeamSide());
        $this->assertCount(2, $restored->getSkills());
    }
}
