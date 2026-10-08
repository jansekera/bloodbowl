<?php

declare(strict_types=1);

namespace App\Tests\Engine;

use App\DTO\ActionResult;
use App\DTO\GameEvent;
use App\DTO\GameState;
use App\Engine\ActionResolver;
use App\Engine\FixedDiceRoller;
use App\Enum\ActionType;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use PHPUnit\Framework\TestCase;

/**
 * DAUNTLESS (audit parity 08.10.2026, nález 8).
 *
 * `rules_bb2016.txt` ř. 8026-8035: "The skill only works when the player attempts to block
 * an opponent who is stronger than himself. ... rolls a D6 and adds it to his strength. If
 * the total is **equal to or lower** than the opponent's Strength, the player must block
 * using his normal Strength. If the total is **greater**, then the player with the Dauntless
 * skill counts as having a Strength **equal to his opponent's** when he makes the block. The
 * strength of both players is calculated **before any defensive or offensive assists are
 * added** but after all other modifiers."
 *
 * Stará mechanika: (a) úspěch už při rovnosti; (b) po úspěchu `max(síla útočníka, síla
 * obránce VČETNĚ jeho asistencí)` -- obranné asistence tím zmizely; (c) při druhé ráně
 * Frenzy se Dauntless neházel vůbec.
 */
final class DauntlessRulesTest extends TestCase
{
    /** @param list<int> $kostky */
    private function blok(GameState $state, array $kostky): ActionResult
    {
        return (new ActionResolver(new FixedDiceRoller($kostky)))
            ->resolve($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);
    }

    /** @return list<GameEvent> */
    private function rany(ActionResult $r): array
    {
        return array_values(array_filter($r->getEvents(), fn(GameEvent $e) => $e->getType() === 'block'));
    }

    public function testRovnostSouctuASilySoupereJeNeuspech(): void
    {
        // ST3 proti ST5, hod 2 ⇒ 2+3 = 5 = síla soupeře ⇒ "equal to or lower" = neúspěch
        //   ⇒ 3 proti 5 = 2 kostky, vybírá obránce. Stará mechanika (>=): 1 kostka.
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, strength: 3, skills: [SkillName::Dauntless], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 7, strength: 5, id: 2)
            ->build();

        $rany = $this->rany($this->blok($state, [2, 3, 3]));

        $this->assertSame(2, $rany[0]->getData()['diceCount'], 'ř. 8029-8030: rovnost nestačí');
        $this->assertFalse($rany[0]->getData()['attackerChooses']);
    }

    public function testSoucetOJednaVyssiJeUspech(): void
    {
        // Kontrola opačným směrem: hod 3 ⇒ 3+3 = 6 > 5 ⇒ síla 5 proti 5 = 1 kostka.
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, strength: 3, skills: [SkillName::Dauntless], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 7, strength: 5, id: 2)
            ->build();

        $rany = $this->rany($this->blok($state, [3, 3]));

        $this->assertSame(1, $rany[0]->getData()['diceCount'], 'ř. 8030-8033');
    }

    public function testObranneAsistenceSePrictouAzPoVyrovnaniSily(): void
    {
        // ST3 proti ST4, obránci asistuje spoluhráč (5,8) sousedící s útočníkem.
        //   Hod 6 ⇒ útočník má sílu 4 (= soupeřova, PŘED asistencemi); pak 4 proti 4+1
        //   ⇒ 2 kostky, vybírá obránce. Stará mechanika vyrovnala na 5 ⇒ 1 kostka.
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, strength: 3, skills: [SkillName::Dauntless], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 7, strength: 4, id: 2)
            ->addPlayer(TeamSide::AWAY, 5, 8, id: 3)
            ->build();

        $rany = $this->rany($this->blok($state, [6, 3, 3]));

        $this->assertSame(2, $rany[0]->getData()['diceCount'], 'ř. 8033-8035: síla se počítá před asistencemi');
        $this->assertFalse($rany[0]->getData()['attackerChooses']);
    }

    public function testUtocneAsistenceSePrictouKVyrovnaneSile(): void
    {
        // ST3 proti ST4, útočníkovi asistuje spoluhráč (7,8) sousedící s obráncem.
        //   Hod 6 ⇒ síla 4, +1 asistence = 5 proti 4 ⇒ 2 kostky, vybírá útočník.
        //   Stará mechanika: max(3+1, 4) = 4 proti 4 ⇒ 1 kostka.
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, strength: 3, skills: [SkillName::Dauntless], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 7, strength: 4, id: 2)
            ->addPlayer(TeamSide::HOME, 7, 8, id: 3)
            ->build();

        $rany = $this->rany($this->blok($state, [6, 3, 3]));

        $this->assertSame(2, $rany[0]->getData()['diceCount'], 'ř. 8033-8035: asistence se přičtou až potom');
        $this->assertTrue($rany[0]->getData()['attackerChooses']);
    }

    public function testDruhaRanaFrenzyHaziDauntlessZnovu(): void
    {
        // ST3 Dauntless + Frenzy proti ST4. Kostky: Dauntless 6 (úspěch) · rána 3 = Pushed ·
        //   druhá rána: Dauntless 6 (úspěch) · rána 3 = Pushed.
        //   Stará mechanika u druhé rány Dauntless neházela: 3 proti 4 = 2 kostky obránce.
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, strength: 3, skills: [SkillName::Dauntless, SkillName::Frenzy], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 7, strength: 4, id: 2)
            ->build();

        $rany = $this->rany($this->blok($state, [6, 3, 6, 3]));

        $this->assertCount(2, $rany, 'fixtura: po Pushed musí přijít druhá rána Frenzy');
        $this->assertSame(1, $rany[1]->getData()['diceCount'], 'ř. 8026-8027: skill platí pro každý blok silnějšího soupeře');
        $this->assertTrue($rany[1]->getData()['attackerChooses']);
    }

    public function testVMultipleBlockSePorovnavaSeSilouObranceZvysenouODva(): void
    {
        // ř. 8300-8301 (Multiple Block): "each defender's strength is increased by 2" --
        //   modifikátor síly, a Dauntless se počítá "after all other modifiers" (ř. 8034-8035).
        //   Měří se DRUHÁ rána (u první obránci asistuje druhý cíl): ST3 proti ST3+2, hod 3
        //   ⇒ 3+3 = 6 > 5 ⇒ 5 proti 5 = 1 kostka.
        //   Stará mechanika porovnávala holé síly (3 proti 3 ⇒ žádný hod): 2 kostky obránce.
        // Kostky: Dauntless 3 · první rána 2 kostky (5 proti 5+1) 3, 3 = Pushed ·
        //   Dauntless 3 · druhá rána 3 = Pushed.
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, strength: 3, skills: [SkillName::Dauntless, SkillName::MultipleBlock], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 6, strength: 3, id: 2)
            ->addPlayer(TeamSide::AWAY, 6, 8, strength: 3, id: 3)
            ->build();

        $r = (new ActionResolver(new FixedDiceRoller([3, 3, 3, 3, 3])))
            ->resolve($state, ActionType::MULTIPLE_BLOCK, ['playerId' => 1, 'targetId' => 2, 'targetId2' => 3]);
        $rany = $this->rany($r);

        $this->assertCount(2, $rany);
        $this->assertSame(1, $rany[1]->getData()['diceCount']);
        $this->assertTrue($rany[1]->getData()['attackerChooses']);
    }
}
