<?php

declare(strict_types=1);

namespace App\Tests\Entity;

use App\Entity\Player;
use App\Entity\Team;
use App\Enum\PlayerStatus;
use App\Enum\TeamStatus;
use App\ValueObject\PlayerStats;
use App\ValueObject\Treasury;
use PHPUnit\Framework\TestCase;

final class TeamTest extends TestCase
{
    /**
     * @return array<string, mixed>
     */
    private function createTeamRow(): array
    {
        return [
            'id' => '1',
            'coach_id' => '1',
            'race_id' => '1',
            'name' => 'Reikland Reavers',
            'treasury' => '850000',
            'rerolls' => '3',
            'fan_factor' => '1',
            'has_apothecary' => true,
            'assistant_coaches' => '0',
            'cheerleaders' => '0',
            'status' => 'active',
            'created_at' => '2025-01-01 00:00:00',
            'updated_at' => '2025-01-01 00:00:00',
            'race_name' => 'Human',
            'coach_name' => 'TestCoach',
        ];
    }

    public function testFromRow(): void
    {
        $team = Team::fromRow($this->createTeamRow());

        $this->assertSame(1, $team->getId());
        $this->assertSame(1, $team->getCoachId());
        $this->assertSame(1, $team->getRaceId());
        $this->assertSame('Reikland Reavers', $team->getName());
        $this->assertSame(850000, $team->getTreasury()->getGold());
        $this->assertSame(3, $team->getRerolls());
        $this->assertSame(1, $team->getFanFactor());
        $this->assertTrue($team->hasApothecary());
        $this->assertSame(TeamStatus::ACTIVE, $team->getStatus());
        $this->assertSame('Human', $team->getRaceName());
        $this->assertSame('TestCoach', $team->getCoachName());
    }

    public function testWithPlayers(): void
    {
        $team = Team::fromRow($this->createTeamRow());

        $player = new Player(
            id: 1,
            teamId: 1,
            positionalTemplateId: 1,
            name: 'Bob',
            number: 1,
            stats: new PlayerStats(6, 3, 3, 8),
            spp: 0,
            level: 1,
            status: PlayerStatus::ACTIVE,
            createdAt: '2025-01-01 00:00:00',
            positionalName: 'Lineman',
        );

        $teamWithPlayers = $team->withPlayers([$player]);

        $this->assertCount(1, $teamWithPlayers->getPlayers());
        $this->assertSame(1, $teamWithPlayers->getPlayerCount());
        $this->assertSame([], $team->getPlayers()); // immutable
    }

    public function testGetActivePlayersExcludesInactive(): void
    {
        $team = Team::fromRow($this->createTeamRow());

        $active = new Player(
            id: 1, teamId: 1, positionalTemplateId: 1, name: 'Active',
            number: 1, stats: new PlayerStats(6, 3, 3, 8), spp: 0, level: 1,
            status: PlayerStatus::ACTIVE, createdAt: '2025-01-01',
        );
        $dead = new Player(
            id: 2, teamId: 1, positionalTemplateId: 1, name: 'Dead',
            number: 2, stats: new PlayerStats(6, 3, 3, 8), spp: 0, level: 1,
            status: PlayerStatus::DEAD, createdAt: '2025-01-01',
        );
        $retired = new Player(
            id: 3, teamId: 1, positionalTemplateId: 1, name: 'Retired',
            number: 3, stats: new PlayerStats(6, 3, 3, 8), spp: 0, level: 1,
            status: PlayerStatus::RETIRED, createdAt: '2025-01-01',
        );

        $teamWithPlayers = $team->withPlayers([$active, $dead, $retired]);

        $this->assertCount(3, $teamWithPlayers->getPlayers());
        $this->assertCount(1, $teamWithPlayers->getActivePlayers());
        $this->assertSame(1, $teamWithPlayers->getPlayerCount());
    }

    public function testToArray(): void
    {
        $team = Team::fromRow($this->createTeamRow());
        $array = $team->toArray();

        $this->assertSame(1, $array['id']);
        $this->assertSame('Reikland Reavers', $array['name']);
        $this->assertSame(850000, $array['treasury']);
        $this->assertSame(3, $array['rerolls']);
        $this->assertSame('Human', $array['race_name']);
        $this->assertSame('active', $array['status']);
    }
}
