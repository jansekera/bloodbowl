<?php

declare(strict_types=1);

namespace App\Tests\Enum;

use App\Enum\SkillName;
use PHPUnit\Framework\TestCase;

final class SkillNameTest extends TestCase
{
    public function testBackingValues(): void
    {
        $this->assertSame('Block', SkillName::Block->value);
        $this->assertSame('Dodge', SkillName::Dodge->value);
        $this->assertSame('Mighty Blow', SkillName::MightyBlow->value);
        $this->assertSame('Sure Hands', SkillName::SureHands->value);
        $this->assertSame('Side Step', SkillName::SideStep->value);
        $this->assertSame('Stand Firm', SkillName::StandFirm->value);
        $this->assertSame('Strip Ball', SkillName::StripBall->value);
    }

    public function testFromString(): void
    {
        $this->assertSame(SkillName::Block, SkillName::from('Block'));
        $this->assertSame(SkillName::MightyBlow, SkillName::from('Mighty Blow'));
        $this->assertSame(SkillName::SureHands, SkillName::from('Sure Hands'));
    }

    public function testTryFromInvalid(): void
    {
        $this->assertNull(SkillName::tryFrom('Nonexistent'));
    }

    public function testAllCasesExist(): void
    {
        $cases = SkillName::cases();
        $this->assertGreaterThanOrEqual(12, count($cases));

        $names = array_map(fn(SkillName $s) => $s->value, $cases);
        $this->assertContains('Block', $names);
        $this->assertContains('Catch', $names);
        $this->assertContains('Dodge', $names);
        $this->assertContains('Sure Feet', $names);
        $this->assertContains('Nerves of Steel', $names);
        $this->assertContains('Pro', $names);
    }
}
