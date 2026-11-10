<?php

declare(strict_types=1);

namespace App\Tests\Engine;

use App\DTO\ActionResult;
use App\DTO\GameState;
use App\DTO\TeamStateDTO;
use App\Engine\ActionResolver;
use App\Engine\FixedDiceRoller;
use App\Engine\Pathfinder;
use App\Engine\TacklezoneCalculator;
use App\Enum\ActionType;
use App\Enum\PlayerState;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use PHPUnit\Framework\TestCase;

/**
 * P277 (audit goblinů 10.11.2026) — PHP Leap podle `rules_bb2016.txt` ř. 8264-8277: „move the player to any empty
 * square 1 to 2 squares from their current square and then make an Agility roll for the player. No modifiers apply
 * to this D6 roll unless he has Very Long Legs … A player may only use the Leap skill once per turn.“ GFI za každé
 * pole navíc (ř. 1701). Parita s C++ move_handler.cpp resolveLeap.
 */
final class LeapRulesTest extends TestCase
{
    /** Pogoer AG3 Leap na (5,7), obklopený vlastními hráči (bez zón) ⇒ ven jen skokem. */
    private function obklopeny(int $ma = 6, int $prehozy = 0): GameState
    {
        $b = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, movement: $ma, agility: 3, skills: [SkillName::Leap], id: 1)
            ->withHomeTeam(TeamStateDTO::create(1, 'Home', 'Goblin', TeamSide::HOME, $prehozy));
        $id = 2;
        foreach ([[-1, -1], [0, -1], [1, -1], [-1, 0], [1, 0], [-1, 1], [0, 1], [1, 1]] as [$dx, $dy]) {
            $b = $b->addPlayer(TeamSide::HOME, 5 + $dx, 7 + $dy, id: $id++);
        }
        return $b->build();
    }

    /** @param list<int> $kostky */
    private function skoc(GameState $s, int $x, int $y, array $kostky): ActionResult
    {
        return (new ActionResolver(new FixedDiceRoller($kostky)))->resolve($s, ActionType::MOVE, ['playerId' => 1, 'x' => $x, 'y' => $y]);
    }

    /** @return array<string, mixed>|null */
    private function skok(ActionResult $r): ?array
    {
        foreach ($r->getEvents() as $e) {
            if ($e->getType() === 'leap') {
                return $e->getData();
            }
        }
        return null;
    }

    public function testNaSkokNeplatiZonyUCile(): void
    {
        // cíl (7,7) se dvěma zónami soupeře: hod je 7-3 = 4+ bez modifikátorů (dřív 4 + 2 = 6+)
        $s = $this->obklopeny();
        foreach ([20 => [8, 6], 21 => [8, 8]] as $id => [$x, $y]) {
            $s = $s->withPlayer(\App\DTO\MatchPlayerDTO::create(
                id: $id,
                playerId: $id,
                name: "Soupeř {$id}",
                number: $id,
                positionalName: 'Lineman',
                stats: new \App\ValueObject\PlayerStats(6, 3, 3, 8),
                skills: [],
                teamSide: TeamSide::AWAY,
                position: new \App\ValueObject\Position($x, $y),
            ));
        }
        $r = $this->skoc($s, 7, 7, [4]);
        $this->assertSame(4, $this->skok($r)['target'] ?? null);
        $this->assertTrue($this->skok($r)['success'] ?? false);
        $this->assertFalse($r->isTurnover());
    }

    public function testSkokNaPoleJezdcemJednimSkokemBezGfi(): void
    {
        // (7,6) = o 2 vodorovně a 1 svisle: „any empty square 1 to 2 squares“ ⇒ jeden skok za 2 pole. Dřív jen 8 polí
        // přesně 2 rovně/šikmo ⇒ (7,6) jen skok + krok = 3 pole.
        $s = $this->obklopeny(ma: 2);
        $cesta = (new Pathfinder(new TacklezoneCalculator()))->findPathTo($s, $s->requirePlayer(1), new \App\ValueObject\Position(7, 6));
        $this->assertNotNull($cesta);
        $this->assertCount(1, $cesta->getSteps());
        $this->assertTrue($cesta->getSteps()[0]->isLeap());
        $this->assertFalse($cesta->getSteps()[0]->isGfi());
    }

    public function testSkokNadRamecPohybuHaziGfiZaKazdePole(): void
    {
        // MA 1, skok stojí 2 ⇒ jedno pole navíc ⇒ jeden GFI (ř. 1701): skok 6, GFI 1 ⇒ pád v cíli + brnění 1+1
        $r = $this->skoc($this->obklopeny(ma: 1), 7, 7, [6, 1, 1, 1]);
        $this->assertTrue($r->isTurnover());
        $this->assertSame(PlayerState::PRONE, $r->getNewState()->requirePlayer(1)->getState());
        $this->assertSame([7, 7], [$r->getNewState()->requirePlayer(1)->getPosition()?->getX(), $r->getNewState()->requirePlayer(1)->getPosition()?->getY()]);
    }

    public function testSkokJenJednouZaKolo(): void
    {
        // Hráč skočil na (7,7). Tah smí pokračovat (jako po přehozeném úhybu, kdy RerollHandler hasMoved nenastaví) —
        // druhý skok v tomtéž kole se ale nabídnout nesmí (ř. 8277 „only use the Leap skill once per turn“).
        $po = $this->skoc($this->obklopeny(), 7, 7, [6])->getNewState();
        $hrac = $po->requirePlayer(1)->withHasMoved(false);

        $skoky = 0;
        foreach ((new Pathfinder(new TacklezoneCalculator()))->findValidMoves($po->withPlayer($hrac), $hrac) as $cesta) {
            foreach ($cesta->getSteps() as $krok) {
                $skoky += $krok->isLeap() ? 1 : 0;
            }
        }
        $this->assertSame(0, $skoky);
    }

    public function testAiPrehodiNepovedenySkokTymovymPrehozem(): void
    {
        // bez interaktivních přehozů (AI): skok 1 ⇒ týmový přehoz ⇒ 6 ⇒ skočí (ř. 929-931)
        $r = $this->skoc($this->obklopeny(prehozy: 1), 7, 7, [1, 6]);
        $this->assertFalse($r->isTurnover());
        $this->assertSame(7, $r->getNewState()->requirePlayer(1)->getPosition()?->getX());
        $this->assertSame(0, $r->getNewState()->getTeamState(TeamSide::HOME)->getRerolls());
    }
}
