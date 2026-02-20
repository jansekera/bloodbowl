<?php

declare(strict_types=1);

namespace App\Tests\Entity;

use App\Entity\Skill;
use App\Enum\SkillCategory;
use PHPUnit\Framework\TestCase;

final class SkillTest extends TestCase
{
    public function testFromRow(): void
    {
        $skill = Skill::fromRow([
            'id' => '1',
            'name' => 'Block',
            'category' => 'General',
            'description' => 'Prevents knockdown',
        ]);

        $this->assertSame(1, $skill->getId());
        $this->assertSame('Block', $skill->getName());
        $this->assertSame(SkillCategory::GENERAL, $skill->getCategory());
        $this->assertSame('Prevents knockdown', $skill->getDescription());
    }

    public function testToArray(): void
    {
        $skill = Skill::fromRow([
            'id' => '1',
            'name' => 'Dodge',
            'category' => 'Agility',
            'description' => 'Reroll dodge',
        ]);

        $array = $skill->toArray();

        $this->assertSame(1, $array['id']);
        $this->assertSame('Dodge', $array['name']);
        $this->assertSame('Agility', $array['category']);
        $this->assertSame('Reroll dodge', $array['description']);
    }
}
