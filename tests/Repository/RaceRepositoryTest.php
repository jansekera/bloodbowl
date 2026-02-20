<?php

declare(strict_types=1);

namespace App\Tests\Repository;

use App\Database;
use App\Repository\RaceRepository;
use PHPUnit\Framework\TestCase;

final class RaceRepositoryTest extends TestCase
{
    private RaceRepository $repository;

    protected function setUp(): void
    {
        $this->repository = new RaceRepository(Database::getConnection());
    }

    public function testFindAll(): void
    {
        $races = $this->repository->findAll();

        $this->assertCount(26, $races);
        $names = array_map(fn($r) => $r->getName(), $races);
        $this->assertContains('Human', $names);
        $this->assertContains('Orc', $names);
        $this->assertContains('Skaven', $names);
        $this->assertContains('Dwarf', $names);
        $this->assertContains('Wood Elf', $names);
        $this->assertContains('Chaos', $names);
        $this->assertContains('Undead', $names);
        $this->assertContains('Lizardmen', $names);
        $this->assertContains('Dark Elf', $names);
    }

    public function testFindById(): void
    {
        $races = $this->repository->findAll();
        $first = $races[0];

        $found = $this->repository->findById($first->getId());

        $this->assertNotNull($found);
        $this->assertSame($first->getName(), $found->getName());
    }

    public function testFindByIdNotFound(): void
    {
        $found = $this->repository->findById(99999);
        $this->assertNull($found);
    }

    public function testFindByIdWithPositionals(): void
    {
        $races = $this->repository->findAll();
        $first = $races[0];

        $race = $this->repository->findByIdWithPositionals($first->getId());

        $this->assertNotNull($race);
        $this->assertNotEmpty($race->getPositionals());

        foreach ($race->getPositionals() as $pos) {
            $this->assertSame($race->getId(), $pos->getRaceId());
            $this->assertGreaterThan(0, $pos->getCost());
            $this->assertGreaterThan(0, $pos->getStats()->getMovement());
        }
    }

    public function testFindAllWithPositionals(): void
    {
        $races = $this->repository->findAllWithPositionals();

        $this->assertCount(26, $races);

        foreach ($races as $race) {
            $this->assertNotEmpty($race->getPositionals(), "{$race->getName()} should have positionals");
        }
    }

    public function testHumanPositionals(): void
    {
        $races = $this->repository->findAllWithPositionals();
        $human = null;
        foreach ($races as $race) {
            if ($race->getName() === 'Human') {
                $human = $race;
                break;
            }
        }

        $this->assertNotNull($human);
        $this->assertSame(60000, $human->getRerollCost());
        $this->assertTrue($human->hasApothecary());

        $positionalNames = array_map(fn($p) => $p->getName(), $human->getPositionals());
        $this->assertContains('Lineman', $positionalNames);
        $this->assertContains('Blitzer', $positionalNames);
        $this->assertContains('Catcher', $positionalNames);
        $this->assertContains('Thrower', $positionalNames);
        $this->assertContains('Ogre', $positionalNames);
    }

    public function testBlitzerHasBlockSkill(): void
    {
        $races = $this->repository->findAllWithPositionals();
        $human = null;
        foreach ($races as $race) {
            if ($race->getName() === 'Human') {
                $human = $race;
                break;
            }
        }

        $this->assertNotNull($human);

        $blitzer = null;
        foreach ($human->getPositionals() as $pos) {
            if ($pos->getName() === 'Blitzer') {
                $blitzer = $pos;
                break;
            }
        }

        $this->assertNotNull($blitzer);
        $skillNames = array_map(fn($s) => $s->getName(), $blitzer->getStartingSkills());
        $this->assertContains('Block', $skillNames);
    }
}
