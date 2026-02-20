<?php

declare(strict_types=1);

namespace App\Tests\Service;

use App\Entity\Skill;
use App\Enum\SkillCategory;
use App\Service\SPPService;
use PHPUnit\Framework\TestCase;

final class SkillAdvancementTest extends TestCase
{
    private SPPService $spp;

    protected function setUp(): void
    {
        $this->spp = new SPPService();
    }

    // --- Level tests (verifying existing + new canAdvance) ---

    public function testGetLevelThresholds(): void
    {
        $this->assertSame(1, $this->spp->getLevel(0));
        $this->assertSame(2, $this->spp->getLevel(6));
        $this->assertSame(3, $this->spp->getLevel(16));
        $this->assertSame(4, $this->spp->getLevel(31));
    }

    public function testHasLeveledUpTransitions(): void
    {
        $this->assertTrue($this->spp->hasLeveledUp(5, 6));   // L1→L2
        $this->assertFalse($this->spp->hasLeveledUp(6, 15)); // L2→L2
    }

    public function testCanAdvanceWithPendingLevelUp(): void
    {
        // Player at 6 SPP = Level 2, with 0 non-starting skills → can advance (2 > 1+0)
        $this->assertTrue($this->spp->canAdvance(6, 0));
    }

    public function testCanAdvanceAlreadyUsed(): void
    {
        // Player at 6 SPP = Level 2, with 1 non-starting skill → cannot advance (2 > 1+1 is false)
        $this->assertFalse($this->spp->canAdvance(6, 1));
    }

    public function testCanAdvanceMultipleLevels(): void
    {
        // Player at 16 SPP = Level 3, with 1 non-starting skill → can advance (3 > 1+1)
        $this->assertTrue($this->spp->canAdvance(16, 1));
        // With 2 skills → cannot (3 > 1+2 is false)
        $this->assertFalse($this->spp->canAdvance(16, 2));
    }

    // --- getAvailableSkills tests ---

    public function testAvailableSkillsNormalAccess(): void
    {
        // Human Lineman: normal=G, double=ASP
        $allSkills = $this->buildSkillList();
        $owned = [];

        $result = $this->spp->getAvailableSkills(
            $owned,
            ['G'],      // normal access
            ['ASP'],    // double access
            $allSkills,
        );

        // Normal should contain General skills
        $normalNames = array_map(fn(Skill $s) => $s->getName(), $result['normal']);
        $this->assertContains('Block', $normalNames);
        $this->assertContains('Tackle', $normalNames);
        // Double should contain Agility, Strength, Passing (not General, that's normal)
        $doubleNames = array_map(fn(Skill $s) => $s->getName(), $result['double']);
        $this->assertContains('Dodge', $doubleNames);       // Agility
        $this->assertContains('Mighty Blow', $doubleNames);  // Strength
        $this->assertContains('Pass', $doubleNames);          // Passing
        $this->assertNotContains('Block', $doubleNames);      // General is normal, not double
    }

    public function testAvailableSkillsDoubleAccess(): void
    {
        // Human Blitzer: normal=GS, double=AP
        $allSkills = $this->buildSkillList();
        $owned = [];

        $result = $this->spp->getAvailableSkills(
            $owned,
            ['GS'],
            ['AP'],
            $allSkills,
        );

        $normalNames = array_map(fn(Skill $s) => $s->getName(), $result['normal']);
        $doubleNames = array_map(fn(Skill $s) => $s->getName(), $result['double']);

        // General + Strength in normal
        $this->assertContains('Block', $normalNames);
        $this->assertContains('Mighty Blow', $normalNames);
        // Agility + Passing in double
        $this->assertContains('Dodge', $doubleNames);
        $this->assertContains('Pass', $doubleNames);
    }

    public function testExcludesAlreadyOwnedSkills(): void
    {
        $allSkills = $this->buildSkillList();
        $owned = [
            $this->makeSkill(1, 'Block', SkillCategory::GENERAL),
        ];

        $result = $this->spp->getAvailableSkills(
            $owned,
            ['G'],
            ['ASP'],
            $allSkills,
        );

        $normalNames = array_map(fn(Skill $s) => $s->getName(), $result['normal']);
        $this->assertNotContains('Block', $normalNames);
        $this->assertContains('Tackle', $normalNames);
    }

    public function testExcludesExtraordinarySkills(): void
    {
        $allSkills = $this->buildSkillList();
        // Add extraordinary skill
        $allSkills[] = $this->makeSkill(100, 'Loner', SkillCategory::EXTRAORDINARY);

        $result = $this->spp->getAvailableSkills(
            [],
            ['G'],
            ['ASPM'],
            $allSkills,
        );

        $allAvailNames = array_merge(
            array_map(fn(Skill $s) => $s->getName(), $result['normal']),
            array_map(fn(Skill $s) => $s->getName(), $result['double']),
        );
        $this->assertNotContains('Loner', $allAvailNames);
    }

    public function testCategoryToCodeMapping(): void
    {
        $this->assertSame('G', SPPService::categoryToCode(SkillCategory::GENERAL));
        $this->assertSame('A', SPPService::categoryToCode(SkillCategory::AGILITY));
        $this->assertSame('S', SPPService::categoryToCode(SkillCategory::STRENGTH));
        $this->assertSame('P', SPPService::categoryToCode(SkillCategory::PASSING));
        $this->assertSame('M', SPPService::categoryToCode(SkillCategory::MUTATION));
        $this->assertSame('X', SPPService::categoryToCode(SkillCategory::EXTRAORDINARY));
    }

    public function testMutationAccess(): void
    {
        // Skaven Lineman: normal=G, double=ASPM
        $allSkills = $this->buildSkillList();
        // Add a mutation skill
        $allSkills[] = $this->makeSkill(50, 'Claw', SkillCategory::MUTATION);

        $result = $this->spp->getAvailableSkills(
            [],
            ['G'],
            ['ASPM'],
            $allSkills,
        );

        $doubleNames = array_map(fn(Skill $s) => $s->getName(), $result['double']);
        $this->assertContains('Claw', $doubleNames);
    }

    public function testNoAccessReturnsEmpty(): void
    {
        $allSkills = $this->buildSkillList();

        $result = $this->spp->getAvailableSkills(
            [],
            [],    // no normal
            [],    // no double
            $allSkills,
        );

        $this->assertEmpty($result['normal']);
        $this->assertEmpty($result['double']);
    }

    // --- Helper methods ---

    /**
     * @return list<Skill>
     */
    private function buildSkillList(): array
    {
        return [
            $this->makeSkill(1, 'Block', SkillCategory::GENERAL),
            $this->makeSkill(2, 'Tackle', SkillCategory::GENERAL),
            $this->makeSkill(3, 'Sure Hands', SkillCategory::GENERAL),
            $this->makeSkill(4, 'Frenzy', SkillCategory::GENERAL),
            $this->makeSkill(10, 'Dodge', SkillCategory::AGILITY),
            $this->makeSkill(11, 'Leap', SkillCategory::AGILITY),
            $this->makeSkill(20, 'Mighty Blow', SkillCategory::STRENGTH),
            $this->makeSkill(21, 'Guard', SkillCategory::STRENGTH),
            $this->makeSkill(30, 'Pass', SkillCategory::PASSING),
            $this->makeSkill(31, 'Safe Throw', SkillCategory::PASSING),
        ];
    }

    private function makeSkill(int $id, string $name, SkillCategory $category): Skill
    {
        return Skill::fromRow([
            'id' => (string) $id,
            'name' => $name,
            'category' => $category->value,
            'description' => $name . ' description',
        ]);
    }
}
