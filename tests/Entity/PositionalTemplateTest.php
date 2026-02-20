<?php

declare(strict_types=1);

namespace App\Tests\Entity;

use App\Entity\PositionalTemplate;
use App\Entity\Skill;
use App\ValueObject\PlayerStats;
use PHPUnit\Framework\TestCase;

final class PositionalTemplateTest extends TestCase
{
    public function testFromRow(): void
    {
        $template = PositionalTemplate::fromRow([
            'id' => '1',
            'race_id' => '1',
            'name' => 'Blitzer',
            'max_count' => '4',
            'cost' => '90000',
            'ma' => '7',
            'st' => '3',
            'ag' => '3',
            'av' => '8',
            'normal_access' => 'G,S',
            'double_access' => 'A,P',
        ]);

        $this->assertSame(1, $template->getId());
        $this->assertSame(1, $template->getRaceId());
        $this->assertSame('Blitzer', $template->getName());
        $this->assertSame(4, $template->getMaxCount());
        $this->assertSame(90000, $template->getCost());
        $this->assertSame(7, $template->getStats()->getMovement());
        $this->assertSame(3, $template->getStats()->getStrength());
        $this->assertSame(3, $template->getStats()->getAgility());
        $this->assertSame(8, $template->getStats()->getArmour());
        $this->assertSame(['G', 'S'], $template->getNormalAccess());
        $this->assertSame(['A', 'P'], $template->getDoubleAccess());
    }

    public function testWithStartingSkills(): void
    {
        $template = new PositionalTemplate(
            id: 1,
            raceId: 1,
            name: 'Blitzer',
            maxCount: 4,
            cost: 90000,
            stats: new PlayerStats(7, 3, 3, 8),
        );

        $skill = Skill::fromRow([
            'id' => '1',
            'name' => 'Block',
            'category' => 'General',
            'description' => 'Prevents knockdown',
        ]);

        $withSkills = $template->withStartingSkills([$skill]);

        $this->assertCount(1, $withSkills->getStartingSkills());
        $this->assertSame('Block', $withSkills->getStartingSkills()[0]->getName());
        // Original unchanged
        $this->assertSame([], $template->getStartingSkills());
    }

    public function testToArray(): void
    {
        $template = new PositionalTemplate(
            id: 1,
            raceId: 1,
            name: 'Lineman',
            maxCount: 16,
            cost: 50000,
            stats: new PlayerStats(6, 3, 3, 8),
            normalAccess: ['G'],
            doubleAccess: ['A', 'S', 'P'],
        );

        $array = $template->toArray();

        $this->assertSame(1, $array['id']);
        $this->assertSame('Lineman', $array['name']);
        $this->assertSame(50000, $array['cost']);
        $this->assertSame(16, $array['max_count']);
        $this->assertSame(['movement' => 6, 'strength' => 3, 'agility' => 3, 'armour' => 8], $array['stats']);
    }
}
