<?php

declare(strict_types=1);

namespace App\Tests\Entity;

use App\Entity\PositionalTemplate;
use App\Entity\Race;
use App\ValueObject\PlayerStats;
use PHPUnit\Framework\TestCase;

final class RaceTest extends TestCase
{
    public function testFromRow(): void
    {
        $race = Race::fromRow([
            'id' => '1',
            'name' => 'Human',
            'reroll_cost' => '60000',
            'has_apothecary' => true,
        ]);

        $this->assertSame(1, $race->getId());
        $this->assertSame('Human', $race->getName());
        $this->assertSame(60000, $race->getRerollCost());
        $this->assertTrue($race->hasApothecary());
        $this->assertSame([], $race->getPositionals());
    }

    public function testWithPositionals(): void
    {
        $race = Race::fromRow([
            'id' => '1',
            'name' => 'Human',
            'reroll_cost' => '60000',
            'has_apothecary' => true,
        ]);

        $template = new PositionalTemplate(
            id: 1,
            raceId: 1,
            name: 'Lineman',
            maxCount: 16,
            cost: 50000,
            stats: new PlayerStats(6, 3, 3, 8),
        );

        $raceWithPos = $race->withPositionals([$template]);

        $this->assertCount(1, $raceWithPos->getPositionals());
        $this->assertSame('Lineman', $raceWithPos->getPositionals()[0]->getName());
        // Original unchanged
        $this->assertSame([], $race->getPositionals());
    }

    public function testToArray(): void
    {
        $race = Race::fromRow([
            'id' => '1',
            'name' => 'Orc',
            'reroll_cost' => '60000',
            'has_apothecary' => true,
        ]);

        $array = $race->toArray();

        $this->assertSame(1, $array['id']);
        $this->assertSame('Orc', $array['name']);
        $this->assertSame(60000, $array['reroll_cost']);
        $this->assertTrue($array['has_apothecary']);
        $this->assertSame([], $array['positionals']);
    }
}
