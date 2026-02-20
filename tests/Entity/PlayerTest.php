<?php

declare(strict_types=1);

namespace App\Tests\Entity;

use App\Entity\Player;
use App\Entity\Skill;
use App\Enum\PlayerStatus;
use App\ValueObject\PlayerStats;
use PHPUnit\Framework\TestCase;

final class PlayerTest extends TestCase
{
    public function testFromRow(): void
    {
        $player = Player::fromRow([
            'id' => '1',
            'team_id' => '1',
            'positional_template_id' => '4',
            'name' => 'Griff Oberwald',
            'number' => '7',
            'ma' => '7',
            'st' => '3',
            'ag' => '3',
            'av' => '8',
            'spp' => '16',
            'level' => '3',
            'status' => 'active',
            'created_at' => '2025-01-01 00:00:00',
            'positional_name' => 'Blitzer',
        ]);

        $this->assertSame(1, $player->getId());
        $this->assertSame(1, $player->getTeamId());
        $this->assertSame(4, $player->getPositionalTemplateId());
        $this->assertSame('Griff Oberwald', $player->getName());
        $this->assertSame(7, $player->getNumber());
        $this->assertSame(7, $player->getStats()->getMovement());
        $this->assertSame(3, $player->getStats()->getStrength());
        $this->assertSame(16, $player->getSpp());
        $this->assertSame(3, $player->getLevel());
        $this->assertSame(PlayerStatus::ACTIVE, $player->getStatus());
        $this->assertSame('Blitzer', $player->getPositionalName());
        $this->assertTrue($player->isActive());
    }

    public function testWithSkills(): void
    {
        $player = Player::fromRow([
            'id' => '1', 'team_id' => '1', 'positional_template_id' => '1',
            'name' => 'Test', 'number' => '1', 'ma' => '6', 'st' => '3',
            'ag' => '3', 'av' => '8', 'spp' => '0', 'level' => '1',
            'status' => 'active', 'created_at' => '2025-01-01',
        ]);

        $skill = Skill::fromRow([
            'id' => '1', 'name' => 'Block', 'category' => 'General',
            'description' => 'Test',
        ]);

        $withSkills = $player->withSkills([$skill]);

        $this->assertCount(1, $withSkills->getSkills());
        $this->assertSame('Block', $withSkills->getSkills()[0]->getName());
        $this->assertSame([], $player->getSkills()); // immutable
    }

    public function testIsActiveForVariousStatuses(): void
    {
        $makePlayer = fn(string $status) => Player::fromRow([
            'id' => '1', 'team_id' => '1', 'positional_template_id' => '1',
            'name' => 'Test', 'number' => '1', 'ma' => '6', 'st' => '3',
            'ag' => '3', 'av' => '8', 'spp' => '0', 'level' => '1',
            'status' => $status, 'created_at' => '2025-01-01',
        ]);

        $this->assertTrue($makePlayer('active')->isActive());
        $this->assertFalse($makePlayer('injured')->isActive());
        $this->assertFalse($makePlayer('dead')->isActive());
        $this->assertFalse($makePlayer('retired')->isActive());
    }

    public function testToArray(): void
    {
        $player = Player::fromRow([
            'id' => '1', 'team_id' => '1', 'positional_template_id' => '4',
            'name' => 'Test', 'number' => '5', 'ma' => '7', 'st' => '3',
            'ag' => '3', 'av' => '8', 'spp' => '6', 'level' => '2',
            'status' => 'active', 'created_at' => '2025-01-01',
            'positional_name' => 'Blitzer',
        ]);

        $array = $player->toArray();

        $this->assertSame(1, $array['id']);
        $this->assertSame('Test', $array['name']);
        $this->assertSame(5, $array['number']);
        $this->assertSame('Blitzer', $array['positional_name']);
        $this->assertSame(['movement' => 7, 'strength' => 3, 'agility' => 3, 'armour' => 8], $array['stats']);
        $this->assertSame(6, $array['spp']);
        $this->assertSame('active', $array['status']);
    }
}
