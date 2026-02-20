<?php

declare(strict_types=1);

namespace App\Tests\Enum;

use App\Enum\SkillCategory;
use PHPUnit\Framework\TestCase;

final class SkillCategoryTest extends TestCase
{
    public function testAllCategories(): void
    {
        $cases = SkillCategory::cases();

        $this->assertCount(6, $cases);
        $this->assertSame('General', SkillCategory::GENERAL->value);
        $this->assertSame('Agility', SkillCategory::AGILITY->value);
        $this->assertSame('Strength', SkillCategory::STRENGTH->value);
        $this->assertSame('Passing', SkillCategory::PASSING->value);
        $this->assertSame('Mutation', SkillCategory::MUTATION->value);
        $this->assertSame('Extraordinary', SkillCategory::EXTRAORDINARY->value);
    }

    public function testFromString(): void
    {
        $this->assertSame(SkillCategory::GENERAL, SkillCategory::from('General'));
        $this->assertSame(SkillCategory::AGILITY, SkillCategory::from('Agility'));
    }

    public function testFromInvalidThrows(): void
    {
        $this->expectException(\ValueError::class);
        SkillCategory::from('Invalid');
    }
}
