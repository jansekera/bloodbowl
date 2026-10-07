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

    public function testDwarfCornersAreLongbeardsWithStandFirm(): void
    {
        $players = getTv1500RaceRoster(TeamSide::HOME, 'Dwarf');
        $standFirm = array_filter($players, static fn(MatchPlayerDTO $p): bool => $p->hasSkill(SkillName::StandFirm));
        $this->assertCount(4, $standFirm);
        foreach ($standFirm as $p) {
            $this->assertSame('Blocker', $p->getPositionalName(), 'Blitzeři na rohy nepatří (uživatel 07.10.)');
            $this->assertFalse($p->hasSkill(SkillName::Frenzy));
        }
        $this->assertSame(2, count(array_filter($standFirm, static fn(MatchPlayerDTO $p): bool => $p->hasSkill(SkillName::Guard))));
        $this->assertSame(2, count(array_filter($standFirm, static fn(MatchPlayerDTO $p): bool => $p->hasSkill(SkillName::MightyBlow))));
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
        $this->assertSame(3, $this->withSkill($players, SkillName::Wrestle));
        $this->assertSame(1, $this->withSkill($players, SkillName::Pro));
    }

    public function testOrcAndHumanCornersHaveStandFirm(): void
    {
        foreach (['Orc', 'Human'] as $race) {
            $players = getTv1500RaceRoster(TeamSide::HOME, $race);
            $this->assertSame(4, $this->withSkill($players, SkillName::StandFirm), $race);
            $this->assertSame(1, $this->withSkill($players, SkillName::Pro), $race);
            $this->assertSame(2, $this->withSkill($players, SkillName::Wrestle), $race);
        }
    }
}
