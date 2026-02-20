<?php

declare(strict_types=1);

namespace App\Tests\Repository;

use App\Database;
use App\Repository\RaceRepository;
use PHPUnit\Framework\TestCase;

final class NewRaceSeedTest extends TestCase
{
    private RaceRepository $repository;

    protected function setUp(): void
    {
        $this->repository = new RaceRepository(Database::getConnection());
    }

    public function testChaosRaceSeeded(): void
    {
        $races = $this->repository->findAllWithPositionals();
        $chaos = null;
        foreach ($races as $race) {
            if ($race->getName() === 'Chaos') {
                $chaos = $race;
                break;
            }
        }

        $this->assertNotNull($chaos, 'Chaos race should be seeded');
        $this->assertSame(70000, $chaos->getRerollCost());
        $this->assertTrue($chaos->hasApothecary());

        $names = array_map(fn($p) => $p->getName(), $chaos->getPositionals());
        $this->assertContains('Beastman', $names);
        $this->assertContains('Chaos Warrior', $names);
        $this->assertContains('Minotaur', $names);
        $this->assertCount(3, $chaos->getPositionals());
    }

    public function testChaosPositionalStats(): void
    {
        $races = $this->repository->findAllWithPositionals();
        $chaos = null;
        foreach ($races as $race) {
            if ($race->getName() === 'Chaos') {
                $chaos = $race;
                break;
            }
        }
        $this->assertNotNull($chaos);

        // Find Beastman and check stats + skills
        $beastman = null;
        $minotaur = null;
        foreach ($chaos->getPositionals() as $pos) {
            if ($pos->getName() === 'Beastman') {
                $beastman = $pos;
            }
            if ($pos->getName() === 'Minotaur') {
                $minotaur = $pos;
            }
        }

        $this->assertNotNull($beastman);
        $this->assertSame(6, $beastman->getStats()->getMovement());
        $this->assertSame(3, $beastman->getStats()->getStrength());
        $this->assertSame(3, $beastman->getStats()->getAgility());
        $this->assertSame(8, $beastman->getStats()->getArmour());
        $skillNames = array_map(fn($s) => $s->getName(), $beastman->getStartingSkills());
        $this->assertContains('Horns', $skillNames);

        $this->assertNotNull($minotaur);
        $this->assertSame(5, $minotaur->getStats()->getStrength());
        $skillNames = array_map(fn($s) => $s->getName(), $minotaur->getStartingSkills());
        $this->assertContains('Loner', $skillNames);
        $this->assertContains('Wild Animal', $skillNames);
        $this->assertContains('Mighty Blow', $skillNames);
        $this->assertContains('Horns', $skillNames);
        $this->assertContains('Frenzy', $skillNames);
    }

    public function testUndeadRaceSeeded(): void
    {
        $races = $this->repository->findAllWithPositionals();
        $undead = null;
        foreach ($races as $race) {
            if ($race->getName() === 'Undead') {
                $undead = $race;
                break;
            }
        }

        $this->assertNotNull($undead, 'Undead race should be seeded');
        $this->assertSame(70000, $undead->getRerollCost());
        $this->assertFalse($undead->hasApothecary());

        $names = array_map(fn($p) => $p->getName(), $undead->getPositionals());
        $this->assertContains('Skeleton', $names);
        $this->assertContains('Zombie', $names);
        $this->assertContains('Ghoul', $names);
        $this->assertContains('Wight', $names);
        $this->assertContains('Mummy', $names);
        $this->assertCount(5, $undead->getPositionals());
    }

    public function testUndeadPositionalStats(): void
    {
        $races = $this->repository->findAllWithPositionals();
        $undead = null;
        foreach ($races as $race) {
            if ($race->getName() === 'Undead') {
                $undead = $race;
                break;
            }
        }
        $this->assertNotNull($undead);

        $wight = null;
        $mummy = null;
        $ghoul = null;
        foreach ($undead->getPositionals() as $pos) {
            if ($pos->getName() === 'Wight') $wight = $pos;
            if ($pos->getName() === 'Mummy') $mummy = $pos;
            if ($pos->getName() === 'Ghoul') $ghoul = $pos;
        }

        // Wight: 6/3/3/8, Block+Regen
        $this->assertNotNull($wight);
        $this->assertSame(6, $wight->getStats()->getMovement());
        $this->assertSame(3, $wight->getStats()->getStrength());
        $this->assertSame(8, $wight->getStats()->getArmour());
        $wightSkills = array_map(fn($s) => $s->getName(), $wight->getStartingSkills());
        $this->assertContains('Block', $wightSkills);
        $this->assertContains('Regeneration', $wightSkills);

        // Mummy: 3/5/1/9, MB+Regen
        $this->assertNotNull($mummy);
        $this->assertSame(3, $mummy->getStats()->getMovement());
        $this->assertSame(5, $mummy->getStats()->getStrength());
        $this->assertSame(1, $mummy->getStats()->getAgility());
        $this->assertSame(9, $mummy->getStats()->getArmour());
        $mummySkills = array_map(fn($s) => $s->getName(), $mummy->getStartingSkills());
        $this->assertContains('Mighty Blow', $mummySkills);
        $this->assertContains('Regeneration', $mummySkills);

        // Ghoul: 7/3/3/7, Dodge
        $this->assertNotNull($ghoul);
        $this->assertSame(7, $ghoul->getStats()->getMovement());
        $this->assertSame(3, $ghoul->getStats()->getAgility());
        $ghoulSkills = array_map(fn($s) => $s->getName(), $ghoul->getStartingSkills());
        $this->assertContains('Dodge', $ghoulSkills);
    }

    // ========== STEP 9: Lizardmen + Dark Elf ==========

    public function testLizardmenRaceSeeded(): void
    {
        $races = $this->repository->findAllWithPositionals();
        $lizardmen = null;
        foreach ($races as $race) {
            if ($race->getName() === 'Lizardmen') {
                $lizardmen = $race;
                break;
            }
        }

        $this->assertNotNull($lizardmen, 'Lizardmen race should be seeded');
        $this->assertSame(60000, $lizardmen->getRerollCost());
        $this->assertTrue($lizardmen->hasApothecary());

        $names = array_map(fn($p) => $p->getName(), $lizardmen->getPositionals());
        $this->assertContains('Skink', $names);
        $this->assertContains('Saurus', $names);
        $this->assertContains('Kroxigor', $names);
        $this->assertCount(3, $lizardmen->getPositionals());
    }

    public function testLizardmenPositionalStats(): void
    {
        $races = $this->repository->findAllWithPositionals();
        $lizardmen = null;
        foreach ($races as $race) {
            if ($race->getName() === 'Lizardmen') {
                $lizardmen = $race;
                break;
            }
        }
        $this->assertNotNull($lizardmen);

        $skink = null;
        $saurus = null;
        $kroxigor = null;
        foreach ($lizardmen->getPositionals() as $pos) {
            if ($pos->getName() === 'Skink') $skink = $pos;
            if ($pos->getName() === 'Saurus') $saurus = $pos;
            if ($pos->getName() === 'Kroxigor') $kroxigor = $pos;
        }

        // Skink: 8/2/3/7, Dodge+Stunty
        $this->assertNotNull($skink);
        $this->assertSame(8, $skink->getStats()->getMovement());
        $this->assertSame(2, $skink->getStats()->getStrength());
        $this->assertSame(3, $skink->getStats()->getAgility());
        $this->assertSame(7, $skink->getStats()->getArmour());
        $skinkSkills = array_map(fn($s) => $s->getName(), $skink->getStartingSkills());
        $this->assertContains('Dodge', $skinkSkills);
        $this->assertContains('Stunty', $skinkSkills);

        // Saurus: 6/4/1/9
        $this->assertNotNull($saurus);
        $this->assertSame(6, $saurus->getStats()->getMovement());
        $this->assertSame(4, $saurus->getStats()->getStrength());
        $this->assertSame(1, $saurus->getStats()->getAgility());
        $this->assertSame(9, $saurus->getStats()->getArmour());
        $this->assertEmpty($saurus->getStartingSkills());

        // Kroxigor: 6/5/1/9, big guy skills
        $this->assertNotNull($kroxigor);
        $this->assertSame(5, $kroxigor->getStats()->getStrength());
        $kroxSkills = array_map(fn($s) => $s->getName(), $kroxigor->getStartingSkills());
        $this->assertContains('Loner', $kroxSkills);
        $this->assertContains('Bone-head', $kroxSkills);
        $this->assertContains('Mighty Blow', $kroxSkills);
        $this->assertContains('Prehensile Tail', $kroxSkills);
    }

    public function testDarkElfRaceSeeded(): void
    {
        $races = $this->repository->findAllWithPositionals();
        $darkElf = null;
        foreach ($races as $race) {
            if ($race->getName() === 'Dark Elf') {
                $darkElf = $race;
                break;
            }
        }

        $this->assertNotNull($darkElf, 'Dark Elf race should be seeded');
        $this->assertSame(50000, $darkElf->getRerollCost());
        $this->assertTrue($darkElf->hasApothecary());

        $names = array_map(fn($p) => $p->getName(), $darkElf->getPositionals());
        $this->assertContains('Lineman', $names);
        $this->assertContains('Runner', $names);
        $this->assertContains('Assassin', $names);
        $this->assertContains('Blitzer', $names);
        $this->assertContains('Witch Elf', $names);
        $this->assertCount(5, $darkElf->getPositionals());
    }

    public function testDarkElfPositionalStats(): void
    {
        $races = $this->repository->findAllWithPositionals();
        $darkElf = null;
        foreach ($races as $race) {
            if ($race->getName() === 'Dark Elf') {
                $darkElf = $race;
                break;
            }
        }
        $this->assertNotNull($darkElf);

        $runner = null;
        $assassin = null;
        $witchElf = null;
        foreach ($darkElf->getPositionals() as $pos) {
            if ($pos->getName() === 'Runner') $runner = $pos;
            if ($pos->getName() === 'Assassin') $assassin = $pos;
            if ($pos->getName() === 'Witch Elf') $witchElf = $pos;
        }

        // Runner: 7/3/4/7, Dump-Off
        $this->assertNotNull($runner);
        $this->assertSame(7, $runner->getStats()->getMovement());
        $this->assertSame(4, $runner->getStats()->getAgility());
        $this->assertSame(7, $runner->getStats()->getArmour());
        $runnerSkills = array_map(fn($s) => $s->getName(), $runner->getStartingSkills());
        $this->assertContains('Dump-Off', $runnerSkills);

        // Assassin: 6/3/4/8, Shadowing+Stab
        $this->assertNotNull($assassin);
        $this->assertSame(6, $assassin->getStats()->getMovement());
        $this->assertSame(4, $assassin->getStats()->getAgility());
        $this->assertSame(8, $assassin->getStats()->getArmour());
        $assassinSkills = array_map(fn($s) => $s->getName(), $assassin->getStartingSkills());
        $this->assertContains('Shadowing', $assassinSkills);
        $this->assertContains('Stab', $assassinSkills);

        // Witch Elf: 7/3/4/7, Dodge+Frenzy+JumpUp
        $this->assertNotNull($witchElf);
        $this->assertSame(7, $witchElf->getStats()->getMovement());
        $this->assertSame(7, $witchElf->getStats()->getArmour());
        $witchSkills = array_map(fn($s) => $s->getName(), $witchElf->getStartingSkills());
        $this->assertContains('Dodge', $witchSkills);
        $this->assertContains('Frenzy', $witchSkills);
        $this->assertContains('Jump Up', $witchSkills);
    }
}
