<?php

declare(strict_types=1);

namespace App\Tests\Repository;

use App\Database;
use App\Repository\SkillRepository;
use PHPUnit\Framework\TestCase;

final class SkillRepositoryTest extends TestCase
{
    private SkillRepository $repository;

    protected function setUp(): void
    {
        $this->repository = new SkillRepository(Database::getConnection());
    }

    public function testFindAll(): void
    {
        $skills = $this->repository->findAll();

        $this->assertNotEmpty($skills);
        $this->assertGreaterThanOrEqual(30, count($skills));
    }

    public function testFindById(): void
    {
        $skills = $this->repository->findAll();
        $first = $skills[0];

        $found = $this->repository->findById($first->getId());

        $this->assertNotNull($found);
        $this->assertSame($first->getName(), $found->getName());
    }

    public function testFindByIdNotFound(): void
    {
        $found = $this->repository->findById(99999);
        $this->assertNull($found);
    }

    public function testFindByName(): void
    {
        $block = $this->repository->findByName('Block');

        $this->assertNotNull($block);
        $this->assertSame('Block', $block->getName());
        $this->assertSame('General', $block->getCategory()->value);
    }

    public function testFindByNameNotFound(): void
    {
        $found = $this->repository->findByName('NonexistentSkill');
        $this->assertNull($found);
    }

    public function testFindByCategory(): void
    {
        $generalSkills = $this->repository->findByCategory('General');

        $this->assertNotEmpty($generalSkills);
        foreach ($generalSkills as $skill) {
            $this->assertSame('General', $skill->getCategory()->value);
        }
    }

    public function testFindByIds(): void
    {
        $allSkills = $this->repository->findAll();
        $ids = [$allSkills[0]->getId(), $allSkills[1]->getId()];

        $found = $this->repository->findByIds($ids);

        $this->assertCount(2, $found);
    }

    public function testFindByIdsEmpty(): void
    {
        $found = $this->repository->findByIds([]);
        $this->assertSame([], $found);
    }
}
