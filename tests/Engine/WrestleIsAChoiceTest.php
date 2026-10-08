<?php

declare(strict_types=1);

namespace App\Tests\Engine;

use App\DTO\ActionResult;
use App\DTO\GameState;
use App\Engine\ActionResolver;
use App\Engine\FixedDiceRoller;
use App\Enum\ActionType;
use App\Enum\PlayerState;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use PHPUnit\Framework\TestCase;

/**
 * WRESTLE JE VOLBA, NE POVINNOST (audit parity 08.10.2026, nález 6).
 *
 * `rules_bb2016.txt` ř. 8671-8676: "This player **may** use Wrestle when he blocks or is
 * blocked and a 'Both Down' result on the Block dice is chosen by either coach. Instead of
 * applying the 'Both Down' result, both players are wrestled to the ground. Both players are
 * Placed Prone in their respective squares even if one or both have the Block skill. Do not
 * make Armour rolls for either player." ř. 1820: "Skill use is not mandatory."
 *
 * Stará mechanika: měl-li Wrestle kdokoli z dvojice, použil se vždy. Hráč, kterému by
 * běžný výsledek Both Down prospěl (má Block, soupeř ne), tak o výhodu přišel.
 *
 * Kostky: 2 = Both Down (1 kostka, stejná síla). Další dvě jsou hod na brnění (1+1 = 2,
 * brnění 8 neprorazí), který hází jen běžný Both Down -- Wrestle na brnění nehází.
 */
final class WrestleIsAChoiceTest extends TestCase
{
    /**
     * @param list<SkillName> $utocnik
     * @param list<SkillName> $obrance
     */
    private function blokBothDown(array $utocnik, array $obrance, ?int $nosic = null): ActionResult
    {
        $b = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, skills: $utocnik, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 7, skills: $obrance, id: 2);
        if ($nosic !== null) {
            $b = $b->withBallCarried($nosic);
        }

        return (new ActionResolver(new FixedDiceRoller([2, 1, 1, 1, 1, 1, 1])))
            ->resolve($b->build(), ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);
    }

    /** @return list<string> */
    private function typy(ActionResult $r): array
    {
        return array_map(fn($e) => $e->getType(), $r->getEvents());
    }

    private function stav(ActionResult $r, int $id): PlayerState
    {
        return $r->getNewState()->requirePlayer($id)->getState();
    }

    public function testUtocnikSBlockemWrestleNepouzijeKdyzBothDownSloziJenObrance(): void
    {
        // Útočník Block + Wrestle, obránce bez Blocku: běžný Both Down složí jen obránce
        //   (a hází se mu na brnění). Wrestle by položil oba a nic nezískal.
        $r = $this->blokBothDown([SkillName::Block, SkillName::Wrestle], []);

        $this->assertNotContains('wrestle', $this->typy($r), 'ř. 8671-8672 "may use" + ř. 1820');
        $this->assertSame(PlayerState::STANDING, $this->stav($r, 1));
        $this->assertSame(PlayerState::PRONE, $this->stav($r, 2));
        $this->assertContains('armour_roll', $this->typy($r), 'běžný Both Down hází obránci na brnění');
        $this->assertFalse($r->isTurnover());
    }

    public function testObranceSBlockemWrestleNepouzijeKdyzBothDownSloziJenUtocnika(): void
    {
        // Útočník bez Blocku, obránce Block + Wrestle: běžný Both Down složí jen útočníka
        //   = turnover. Wrestle by položil i obránce a turnover by zrušil.
        $r = $this->blokBothDown([], [SkillName::Block, SkillName::Wrestle]);

        $this->assertNotContains('wrestle', $this->typy($r), 'ř. 8671-8672 "may use" + ř. 1820');
        $this->assertSame(PlayerState::PRONE, $this->stav($r, 1));
        $this->assertSame(PlayerState::STANDING, $this->stav($r, 2));
        $this->assertTrue($r->isTurnover(), 'sražený útočník = turnover (ř. 603-607)');
    }

    public function testUtocnikBezBlockuWrestlePouzijeProtiBlocku(): void
    {
        // Kontrola opačným směrem (skill se nesmí vypnout úplně): útočník Wrestle bez Blocku
        //   proti Blocku by běžným Both Down padl sám ⇒ Wrestle položí oba, bez turnoveru.
        $r = $this->blokBothDown([SkillName::Wrestle], [SkillName::Block]);

        $this->assertContains('wrestle', $this->typy($r));
        $this->assertSame(PlayerState::PRONE, $this->stav($r, 1));
        $this->assertSame(PlayerState::PRONE, $this->stav($r, 2), 'ř. 8675-8676: i když má Block');
        $this->assertNotContains('armour_roll', $this->typy($r), 'ř. 8676-8677: bez hodu na brnění');
        $this->assertFalse($r->isTurnover());
    }

    public function testUtocnikSBlockemWrestlePouzijeProtiBlocku(): void
    {
        // Oba mají Block: běžný Both Down by neudělal nic, Wrestle obránce položí.
        $r = $this->blokBothDown([SkillName::Block, SkillName::Wrestle], [SkillName::Block]);

        $this->assertContains('wrestle', $this->typy($r));
        $this->assertSame(PlayerState::PRONE, $this->stav($r, 2));
        $this->assertFalse($r->isTurnover());
    }

    public function testObranceSBlockemWrestlePouzijeNaNosiceSBlockem(): void
    {
        // Útočník s míčem a Blockem, obránce Block + Wrestle: běžný Both Down nic neudělá,
        //   Wrestle položí nosiče tým na tahu ⇒ turnover (ř. 8677-8678).
        $r = $this->blokBothDown([SkillName::Block], [SkillName::Block, SkillName::Wrestle], nosic: 1);

        $this->assertContains('wrestle', $this->typy($r));
        $this->assertTrue($r->isTurnover(), 'ř. 8677-8678: aktivní hráč držel míč');
    }

    public function testUtocnikSMicemABlockemWrestleNepouzije(): void
    {
        // Wrestle by položil vlastního nosiče = turnover z vlastní volby (ř. 8677-8678).
        $r = $this->blokBothDown([SkillName::Block, SkillName::Wrestle], [SkillName::Block], nosic: 1);

        $this->assertNotContains('wrestle', $this->typy($r));
        $this->assertSame(PlayerState::STANDING, $this->stav($r, 1));
        $this->assertFalse($r->isTurnover());
    }
}
