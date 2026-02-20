<?php
declare(strict_types=1);

namespace App\Tests\Engine;

use App\Enum\SkillName;
use App\Enum\TeamSide;
use PHPUnit\Framework\TestCase;

// Include race roster definitions (shared with cli/simulate.php)
require_once __DIR__ . '/../../cli/race_rosters.php';

/**
 * Tests for race roster generation in simulate.php.
 */
final class RaceRosterTest extends TestCase
{
    /**
     * @dataProvider raceProvider
     */
    public function testAllRacesProduceElevenPlayers(string $race): void
    {
        $players = getRaceRoster(TeamSide::HOME, $race);
        $this->assertCount(11, $players, "Race {$race} should produce exactly 11 players");
    }

    public static function raceProvider(): array
    {
        return [
            ['Human'], ['Orc'], ['Skaven'], ['Dwarf'], ['Wood Elf'],
            ['Chaos'], ['Undead'], ['Lizardmen'], ['Dark Elf'],
            ['Halfling'], ['Norse'], ['High Elf'],
        ];
    }

    public function testHalflingRosterHasTwoTreemen(): void
    {
        $players = getRaceRoster(TeamSide::HOME, 'Halfling');
        $treemen = array_filter($players, fn($p) => $p->getPositionalName() === 'Treeman');
        $this->assertCount(2, $treemen);

        $treeman = reset($treemen);
        $this->assertTrue($treeman->hasSkill(SkillName::TakeRoot));
        $this->assertTrue($treeman->hasSkill(SkillName::ThrowTeamMate));
        $this->assertTrue($treeman->hasSkill(SkillName::MightyBlow));
        $this->assertTrue($treeman->hasSkill(SkillName::StandFirm));
        $this->assertEquals(6, $treeman->getStats()->getStrength());
    }

    public function testNorseAllLinemenHaveBlock(): void
    {
        $players = getRaceRoster(TeamSide::AWAY, 'Norse');
        $linemen = array_filter($players, fn($p) => $p->getPositionalName() === 'Lineman');
        $this->assertCount(5, $linemen);

        foreach ($linemen as $lineman) {
            $this->assertTrue($lineman->hasSkill(SkillName::Block), 'Norse Lineman should have Block');
        }
    }

    public function testHighElfBlitzerStats(): void
    {
        $players = getRaceRoster(TeamSide::HOME, 'High Elf');
        $blitzers = array_filter($players, fn($p) => $p->getPositionalName() === 'Blitzer');
        $this->assertCount(4, $blitzers);

        $blitzer = reset($blitzers);
        $this->assertEquals(7, $blitzer->getStats()->getMovement());
        $this->assertEquals(3, $blitzer->getStats()->getStrength());
        $this->assertEquals(4, $blitzer->getStats()->getAgility());
        $this->assertEquals(8, $blitzer->getStats()->getArmour());
        $this->assertTrue($blitzer->hasSkill(SkillName::Block));
    }

    public function testDarkElfAssassinSkills(): void
    {
        // Dark Elf roster in simulate.php uses Witch Elf (not Assassin)
        // Verify Witch Elf has correct skills
        $players = getRaceRoster(TeamSide::HOME, 'Dark Elf');
        $witchElves = array_filter($players, fn($p) => $p->getPositionalName() === 'Witch Elf');
        $this->assertCount(2, $witchElves);

        $witch = reset($witchElves);
        $this->assertTrue($witch->hasSkill(SkillName::Dodge));
        $this->assertTrue($witch->hasSkill(SkillName::Frenzy));
        $this->assertTrue($witch->hasSkill(SkillName::JumpUp));
    }

    public function testRaceRosterSkillsCorrect(): void
    {
        // Spot check various positionals
        $chaos = getRaceRoster(TeamSide::HOME, 'Chaos');
        $beastmen = array_filter($chaos, fn($p) => $p->getPositionalName() === 'Beastman');
        $this->assertCount(7, $beastmen);
        $this->assertTrue(reset($beastmen)->hasSkill(SkillName::Horns));

        $undead = getRaceRoster(TeamSide::AWAY, 'Undead');
        $mummies = array_filter($undead, fn($p) => $p->getPositionalName() === 'Mummy');
        $this->assertCount(2, $mummies);
        $this->assertTrue(reset($mummies)->hasSkill(SkillName::MightyBlow));

        $lizardmen = getRaceRoster(TeamSide::HOME, 'Lizardmen');
        $skinks = array_filter($lizardmen, fn($p) => $p->getPositionalName() === 'Skink');
        $this->assertCount(5, $skinks);
        $this->assertTrue(reset($skinks)->hasSkill(SkillName::Dodge));
        $this->assertTrue(reset($skinks)->hasSkill(SkillName::Stunty));
    }

    public function testUnknownRaceThrows(): void
    {
        $this->expectException(\InvalidArgumentException::class);
        getRaceRoster(TeamSide::HOME, 'NonexistentRace');
    }
}
