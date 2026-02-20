<?php
declare(strict_types=1);

namespace App\Tests\CLI;

use App\Enum\SkillName;
use App\Enum\TeamSide;
use PHPUnit\Framework\TestCase;

require_once __DIR__ . '/../../cli/developed_rosters.php';

final class DevelopedRostersTest extends TestCase
{
    /**
     * @dataProvider allRacesProvider
     */
    public function testDevelopedRosterReturns11Players(string $race): void
    {
        $players = getDevelopedRaceRoster(TeamSide::HOME, $race);
        $this->assertCount(11, $players, "Race {$race} should have 11 players");
    }

    /**
     * @dataProvider allRacesProvider
     */
    public function testDevelopedRosterHasMoreOrEqualSkillsThanBase(string $race): void
    {
        $basePlayers = getRaceRoster(TeamSide::HOME, $race);
        $devPlayers = getDevelopedRaceRoster(TeamSide::HOME, $race);

        foreach ($basePlayers as $id => $basePlayer) {
            $devPlayer = $devPlayers[$id];
            $this->assertGreaterThanOrEqual(
                count($basePlayer->getSkills()),
                count($devPlayer->getSkills()),
                "Developed {$race} player {$basePlayer->getPositionalName()} should have >= base skills"
            );
        }
    }

    public function testHumanBlitzerHasExtraSkill(): void
    {
        $players = getDevelopedRaceRoster(TeamSide::HOME, 'Human');
        $blitzers = array_filter(
            $players,
            fn($p) => $p->getPositionalName() === 'Blitzer'
        );
        $this->assertCount(4, $blitzers);

        foreach ($blitzers as $blitzer) {
            // All blitzers have base Block
            $this->assertTrue($blitzer->hasSkill(SkillName::Block));
            // Each has one extra: Guard, MightyBlow, or Tackle
            $extraSkills = array_diff(
                array_map(fn($s) => $s->value, $blitzer->getSkills()),
                [SkillName::Block->value]
            );
            $this->assertCount(1, $extraSkills);
            $extra = SkillName::from(array_values($extraSkills)[0]);
            $this->assertContains($extra, [SkillName::Guard, SkillName::MightyBlow, SkillName::Tackle]);
        }
    }

    public function testWoodElfWardancerHasExtraSkill(): void
    {
        $players = getDevelopedRaceRoster(TeamSide::HOME, 'Wood Elf');
        $wardancers = array_filter(
            $players,
            fn($p) => $p->getPositionalName() === 'Wardancer'
        );
        $this->assertCount(2, $wardancers);

        foreach ($wardancers as $wd) {
            // Base: Block + Dodge
            $this->assertTrue($wd->hasSkill(SkillName::Block));
            $this->assertTrue($wd->hasSkill(SkillName::Dodge));
            // +1 extra: StripBall or Tackle
            $this->assertCount(3, $wd->getSkills());
            $hasExtra = $wd->hasSkill(SkillName::StripBall) || $wd->hasSkill(SkillName::Tackle);
            $this->assertTrue($hasExtra, 'Wardancer should have StripBall or Tackle');
        }
    }

    public function testOrcBlackOrcsAllGetBlock(): void
    {
        $players = getDevelopedRaceRoster(TeamSide::HOME, 'Orc');
        $blackOrcs = array_filter(
            $players,
            fn($p) => $p->getPositionalName() === 'Black Orc'
        );
        $this->assertCount(4, $blackOrcs);

        foreach ($blackOrcs as $bo) {
            $this->assertTrue($bo->hasSkill(SkillName::Block), 'Black Orc should have Block');
        }
    }

    public function testLizardmenSaurusAllGetBlock(): void
    {
        $players = getDevelopedRaceRoster(TeamSide::HOME, 'Lizardmen');
        $sauruses = array_filter(
            $players,
            fn($p) => $p->getPositionalName() === 'Saurus'
        );
        $this->assertCount(6, $sauruses);

        foreach ($sauruses as $s) {
            $this->assertTrue($s->hasSkill(SkillName::Block), 'Saurus should have Block');
        }
    }

    public function testChaosWarriorsAllGetBlock(): void
    {
        $players = getDevelopedRaceRoster(TeamSide::HOME, 'Chaos');
        $warriors = array_filter(
            $players,
            fn($p) => $p->getPositionalName() === 'Chaos Warrior'
        );
        $this->assertCount(4, $warriors);

        foreach ($warriors as $w) {
            $this->assertTrue($w->hasSkill(SkillName::Block), 'Chaos Warrior should have Block');
        }
    }

    public function testBaseRosterUnchangedForHome(): void
    {
        $basePlayers = getRaceRoster(TeamSide::HOME, 'Human');
        // Verify base roster wasn't mutated
        $blitzers = array_filter(
            $basePlayers,
            fn($p) => $p->getPositionalName() === 'Blitzer'
        );
        foreach ($blitzers as $b) {
            $this->assertCount(1, $b->getSkills(), 'Base blitzer should still have only Block');
        }
    }

    public function testAwayTeamWorks(): void
    {
        $players = getDevelopedRaceRoster(TeamSide::AWAY, 'Dwarf');
        $this->assertCount(11, $players);

        // Verify IDs are offset for away
        $ids = array_keys($players);
        $this->assertEquals(12, min($ids));
    }

    /**
     * @return array<string, array{string}>
     */
    public static function allRacesProvider(): array
    {
        return [
            'Human' => ['Human'],
            'Orc' => ['Orc'],
            'Skaven' => ['Skaven'],
            'Dwarf' => ['Dwarf'],
            'Wood Elf' => ['Wood Elf'],
            'Chaos' => ['Chaos'],
            'Undead' => ['Undead'],
            'Lizardmen' => ['Lizardmen'],
            'Dark Elf' => ['Dark Elf'],
            'Halfling' => ['Halfling'],
            'Norse' => ['Norse'],
            'High Elf' => ['High Elf'],
            'Vampire' => ['Vampire'],
            'Amazon' => ['Amazon'],
            'Necromantic' => ['Necromantic'],
            'Bretonnian' => ['Bretonnian'],
            'Khemri' => ['Khemri'],
            'Goblin' => ['Goblin'],
            'Chaos Dwarf' => ['Chaos Dwarf'],
            'Ogre' => ['Ogre'],
            'Nurgle' => ['Nurgle'],
            'Pro Elf' => ['Pro Elf'],
            'Slann' => ['Slann'],
            'Underworld' => ['Underworld'],
            'Khorne' => ['Khorne'],
            'Chaos Pact' => ['Chaos Pact'],
        ];
    }
}
