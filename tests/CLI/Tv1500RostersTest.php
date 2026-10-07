<?php

declare(strict_types=1);

namespace App\Tests\CLI;

use App\DTO\MatchPlayerDTO;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use PHPUnit\Framework\TestCase;

require_once __DIR__ . '/../../cli/tv1500_rosters.php';

/**
 * P165 (07.10.2026): úroveň TV1500 — PHP jedenáctka má být táž jako v C++
 * (engine/tests/test_game_simulator.cpp, DevelopedRoster.Tv1500*). Čísla níž jsou tatáž.
 */
final class Tv1500RostersTest extends TestCase
{
    /** @param array<int, MatchPlayerDTO> $players */
    private function withSkill(array $players, SkillName $skill): int
    {
        return count(array_filter($players, static fn(MatchPlayerDTO $p): bool => $p->hasSkill($skill)));
    }

    public function testEveryTeamFieldsElevenPlayers(): void
    {
        foreach (array_keys(TV1500_ROSTERS) as $race) {
            $this->assertCount(11, getTv1500RaceRoster(TeamSide::HOME, $race), (string) $race);
        }
    }

    public function testDwarfStandFirmOnBreakersAndTwoCorners(): void
    {
        $players = getTv1500RaceRoster(TeamSide::HOME, 'Dwarf');
        $standFirm = array_filter($players, static fn(MatchPlayerDTO $p): bool => $p->hasSkill(SkillName::StandFirm));
        $this->assertCount(6, $standFirm);
        $byPosition = array_count_values(array_map(static fn(MatchPlayerDTO $p): string => (string) $p->getPositionalName(), $standFirm));
        $this->assertSame(2, $byPosition['Blitzer'] ?? 0, 'prorážeči');
        $this->assertSame(2, $byPosition['Troll Slayer'] ?? 0, 'prorážeči');
        $this->assertSame(2, $byPosition['Blocker'] ?? 0, 'dva rohoví Longbeardi s Guardem');
        foreach ($standFirm as $p) {
            $this->assertFalse($p->hasSkill(SkillName::Wrestle), 'Stand Firm raději někomu, kdo nemá Wrestle');
        }
        $this->assertSame(6, $this->withSkill($players, SkillName::MightyBlow));
        $this->assertSame(1, $this->withSkill($players, SkillName::Pro));
        $this->assertSame(4, $this->withSkill($players, SkillName::Guard));
        $this->assertSame(1, $this->withSkill($players, SkillName::Wrestle));
    }

    public function testWoodElfPassingSkillsAndBlockLinemen(): void
    {
        $players = getTv1500RaceRoster(TeamSide::HOME, 'Wood Elf');
        $this->assertSame(1, $this->withSkill($players, SkillName::Accurate));
        $this->assertSame(2, $this->withSkill($players, SkillName::DivingCatch));
        $this->assertSame(1, $this->withSkill($players, SkillName::Pro));
        $this->assertSame(2, $this->withSkill($players, SkillName::Wrestle));
        $this->assertSame(1, $this->withSkill($players, SkillName::SideStep), 'Side Step jen Wardancer, rohoví Linemani ne');
        $this->assertSame(8, $this->withSkill($players, SkillName::Block));
    }

    public function testSkavenOneFastGutterRunnerAndOneWithWrestle(): void
    {
        $players = getTv1500RaceRoster(TeamSide::HOME, 'Skaven');
        $this->assertSame(1, $this->withSkill($players, SkillName::SureFeet));
        $this->assertSame(1, $this->withSkill($players, SkillName::Sprint));
        $ma = array_count_values(array_map(static fn(MatchPlayerDTO $p): int => $p->getStats()->getMovement(), $players));
        $this->assertSame(1, $ma[10] ?? 0);
        $this->assertSame(3, $ma[9] ?? 0);
        $this->assertSame(4, $this->withSkill($players, SkillName::Wrestle), '3 Linemani + lovec');
        $this->assertSame(1, $this->withSkill($players, SkillName::Horns), 'lovec = Gutter Runner s Wrestle a Horns');
        $this->assertSame(2, $this->withSkill($players, SkillName::MightyBlow), 'jeden Blitzer + Rat Ogre');
        $this->assertSame(1, $this->withSkill($players, SkillName::Pro));
        $this->assertSame(1, $this->withSkill($players, SkillName::WildAnimal), 'Rat Ogre v téže sestavě (uživatel 08.10.)');
    }

    public function testOrcCornersHaveStandFirm(): void
    {
        $players = getTv1500RaceRoster(TeamSide::HOME, 'Orc');
        $this->assertSame(4, $this->withSkill($players, SkillName::StandFirm));
        $this->assertSame(6, $this->withSkill($players, SkillName::Guard));
        $this->assertSame(1, $this->withSkill($players, SkillName::Pro));
        $this->assertSame(2, $this->withSkill($players, SkillName::Wrestle));
    }

    /** Uživatel 07.10.2026: lovec Wrestle místo Stand Firm, Blitzer bez Guard Tackle místo Stand Firm. */
    public function testHumanHunterHasWrestleAndTwoBlitzersHaveTackle(): void
    {
        $players = getTv1500RaceRoster(TeamSide::HOME, 'Human');
        $this->assertSame(2, $this->withSkill($players, SkillName::StandFirm), 'jen dva Blitzeři s Guard');
        $this->assertSame(1, $this->withSkill($players, SkillName::Dauntless), 'jeden Blitzer s Guard, místo Mighty Blow');
        $this->assertSame(2, $this->withSkill($players, SkillName::Guard));
        $this->assertSame(3, $this->withSkill($players, SkillName::MightyBlow));
        $this->assertCount(11, $players);
        $this->assertSame(2, $this->withSkill($players, SkillName::Tackle));
        $this->assertSame(3, $this->withSkill($players, SkillName::Wrestle), 'dva Linemani + lovec');
        $this->assertSame(1, $this->withSkill($players, SkillName::Pro));
        foreach ($players as $p) {
            if ($p->hasSkill(SkillName::StripBall)) {
                $this->assertTrue($p->hasSkill(SkillName::Wrestle));
                $this->assertTrue($p->hasSkill(SkillName::Tackle));
                $this->assertFalse($p->hasSkill(SkillName::StandFirm));
            }
            if ($p->hasSkill(SkillName::StandFirm)) {
                $this->assertTrue($p->hasSkill(SkillName::Guard));
            }
        }
    }
}
