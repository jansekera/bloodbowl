<?php

declare(strict_types=1);

namespace App\Tests\Engine;

use App\DTO\ActionResult;
use App\DTO\GameState;
use App\Engine\ActionResolver;
use App\Engine\FixedDiceRoller;
use App\Engine\RulesEngine;
use App\Enum\ActionType;
use App\Enum\PlayerState;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use PHPUnit\Framework\TestCase;

/**
 * P312 (uživatel 20.11.2026: „ano“) — Wrestle a pole Side Step volí ve webové hře kouč-člověk (dřív rozhodlo pravidlo
 * AI i za něj). Parita s C++ `CoachQuestion::USE_WRESTLE` / `USE_SIDE_STEP`.
 *
 * `rules_bb2016.txt` ř. 8670-8676 (Wrestle): "This player may use Wrestle when he blocks or is blocked and a 'Both
 * Down' result on the Block dice is chosen by either coach. Instead of applying the 'Both Down' result, both players
 * are wrestled to the ground." Ř. 8474-8478 (Side Step): "his coach may choose to move the player to any adjacent
 * square, not just the three squares shown on the Push Back diagram."
 *
 * Kostka bloku: 1 lebka · 2 Both Down · 3-4 Pushed · 5 Stumbles · 6 Defender Down.
 */
final class WrestleSideStepKoucTest extends TestCase
{
    /**
     * Útočník HOME (5,5) → obránce AWAY (6,5).
     *
     * @param list<SkillName> $utocnik
     * @param list<SkillName> $obrance
     */
    private function stav(array $utocnik, array $obrance): GameState
    {
        return (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, skills: $utocnik, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, skills: $obrance, id: 2)
            ->withBallOffPitch()
            ->build();
    }

    /**
     * @param list<TeamSide> $lide
     * @param list<int> $kostky
     */
    private function resolver(array $lide, array $kostky): ActionResolver
    {
        $r = new ActionResolver(new FixedDiceRoller($kostky));
        $r->setInteractiveBlocks(in_array(TeamSide::HOME, $lide, true));   // kostku volí útočník-člověk; tah AI ne
        $r->setHumanSides($lide);

        return $r;
    }

    /** Tah AI (HOME) proti obránci-člověku (AWAY): AI vybere kostku sama. */
    private function blokAI(GameState $s, int $kostka): GameState
    {
        return $this->resolver([TeamSide::AWAY], [$kostka])->resolve($s, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2])->getNewState();
    }

    /**
     * Útočník-člověk (HOME) hodí kostku a zvolí ji.
     *
     * @param list<TeamSide> $lide
     */
    private function blokClovek(GameState $s, int $kostka, array $lide = [TeamSide::HOME]): GameState
    {
        $hod = $this->resolver($lide, [$kostka])->resolve($s, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);
        $this->assertNotNull($hod->getNewState()->getPendingBlock(), 'fixtura: čeká volba kostky');

        return $this->resolver($lide, [])->resolve($hod->getNewState(), ActionType::CHOOSE_BLOCK_DIE, ['faceIndex' => 0])->getNewState();
    }

    /**
     * @param list<TeamSide> $lide
     * @param array<string, mixed> $params
     * @param list<int> $kostky
     */
    private function odpoved(GameState $q, ActionType $a, array $params, array $kostky = [], array $lide = [TeamSide::AWAY]): ActionResult
    {
        return $this->resolver($lide, $kostky)->resolve($q, $a, $params);
    }

    /** @return list<string> */
    private function typy(ActionResult $r): array
    {
        return array_map(static fn($e): string => $e->getType(), $r->getEvents());
    }

    // ── Wrestle ──

    public function testObranceClovekDostaneOtazkuNaWrestle(): void
    {
        $q = $this->blokAI($this->stav([SkillName::Block], [SkillName::Wrestle]), 2);

        $otazka = $q->getPendingWrestle();
        $this->assertNotNull($otazka);
        $this->assertSame(2, $otazka->getPlayerId());
        $this->assertSame(1, $otazka->getOpponentId());
        $this->assertSame(PlayerState::STANDING, $q->requirePlayer(2)->getState(), 'do odpovědi se nic nestalo');
        $this->assertSame([ActionType::RESOLVE_WRESTLE->value], array_column((new RulesEngine())->getAvailableActions($q), 'type'));
        $this->assertNotSame([], (new RulesEngine())->validate($q, ActionType::RESOLVE_WRESTLE, ['use' => 'ano']));
    }

    public function testObranceAnoObaNaZemiBezBrneni(): void
    {
        $q = $this->blokAI($this->stav([SkillName::Block], [SkillName::Wrestle]), 2);
        $r = $this->odpoved($q, ActionType::RESOLVE_WRESTLE, ['use' => true]);
        $s = $r->getNewState();

        $this->assertNull($s->getPendingWrestle());
        $this->assertSame(PlayerState::PRONE, $s->requirePlayer(1)->getState());
        $this->assertSame(PlayerState::PRONE, $s->requirePlayer(2)->getState());
        $this->assertContains('wrestle', $this->typy($r));
        $this->assertNotContains('armor_roll', $this->typy($r));
    }

    /** Ne ⇒ Both Down platí: útočník s Blockem stojí, obránce bez Blocku je sražen (AI by Wrestle použila). */
    public function testObranceNeBothDownPlati(): void
    {
        $q = $this->blokAI($this->stav([SkillName::Block], [SkillName::Wrestle]), 2);
        $s = $this->odpoved($q, ActionType::RESOLVE_WRESTLE, ['use' => false], [1, 1])->getNewState();

        $this->assertSame(PlayerState::STANDING, $s->requirePlayer(1)->getState());
        $this->assertSame(PlayerState::PRONE, $s->requirePlayer(2)->getState());
    }

    public function testUtocnikClovekAnoObaNaZemiBezTurnoveru(): void
    {
        $q = $this->blokClovek($this->stav([SkillName::Wrestle], []), 2);
        $this->assertSame(1, $q->getPendingWrestle()?->getPlayerId());
        $r = $this->odpoved($q, ActionType::RESOLVE_WRESTLE, ['use' => true], [], [TeamSide::HOME]);

        $this->assertFalse($r->isTurnover(), 'Wrestle bez míče turnover nedělá (ř. 8677-8678)');
        $this->assertSame(PlayerState::PRONE, $r->getNewState()->requirePlayer(1)->getState());
        $this->assertSame(PlayerState::PRONE, $r->getNewState()->requirePlayer(2)->getState());
    }

    /** Ne ⇒ útočník bez Blocku padá sám: turnover (AI by Wrestle použila vždy). */
    public function testUtocnikClovekNePadaSamTurnover(): void
    {
        $q = $this->blokClovek($this->stav([SkillName::Wrestle], []), 2);
        $r = $this->odpoved($q, ActionType::RESOLVE_WRESTLE, ['use' => false], [1, 1, 1, 1], [TeamSide::HOME]);

        $this->assertTrue($r->isTurnover());
        $this->assertSame(PlayerState::PRONE, $r->getNewState()->requirePlayer(1)->getState());
        $this->assertNotContains('wrestle', $this->typy($r));
    }

    /** Pořadí jako C++: napřed obránce; jeho „ano“ položí oba ⇒ útočníka se už neptá. */
    public function testOboustrannyWrestleNapredObranceAnoUtocnikaSeNepta(): void
    {
        $oba = [TeamSide::HOME, TeamSide::AWAY];
        $q = $this->blokClovek($this->stav([SkillName::Wrestle], [SkillName::Wrestle]), 2, $oba);
        $this->assertSame(2, $q->getPendingWrestle()?->getPlayerId(), 'napřed obránce');
        $s = $this->odpoved($q, ActionType::RESOLVE_WRESTLE, ['use' => true], [], $oba)->getNewState();

        $this->assertNull($s->getPendingWrestle(), 'útočníka se už neptá');
        $this->assertSame(PlayerState::PRONE, $s->requirePlayer(1)->getState());
    }

    public function testObranceNeUtocnikDostaneOtazku(): void
    {
        $oba = [TeamSide::HOME, TeamSide::AWAY];
        $q = $this->blokClovek($this->stav([SkillName::Wrestle, SkillName::Block], [SkillName::Wrestle]), 2, $oba);
        $q2 = $this->odpoved($q, ActionType::RESOLVE_WRESTLE, ['use' => false], [], $oba)->getNewState();

        $this->assertSame(1, $q2->getPendingWrestle()?->getPlayerId(), 'po „ne“ obránce se ptá útočníka');
    }

    public function testAiObranceSeNepta(): void
    {
        $s = $this->resolver([], [2])->resolve($this->stav([SkillName::Block], [SkillName::Wrestle]), ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2])->getNewState();

        $this->assertNull($s->getPendingWrestle());
        $this->assertSame(PlayerState::PRONE, $s->requirePlayer(1)->getState(), 'AI obránce Wrestle použila (útočník má Block)');
    }

    // ── Side Step ──

    public function testSideStepOtazkaNabidneVsechnaVolnaSousedniPole(): void
    {
        $q = $this->blokAI($this->stav([], [SkillName::SideStep]), 3);   // Pushed

        $otazka = $q->getPendingSideStep();
        $this->assertNotNull($otazka);
        $this->assertSame(2, $otazka->getPlayerId());
        $pole = array_map(static fn($p): string => $p->getX() . ',' . $p->getY(), $otazka->getSquares());
        $this->assertContains('5,4', $pole, 'i pole mimo diagram odtlačení (ř. 8474-8476 „any adjacent square“)');
        $this->assertNotContains('5,5', $pole, 'pole útočníka není volné');
        $this->assertCount(7, $pole);
        $this->assertSame(6, $q->requirePlayer(2)->requirePosition()->getX(), 'do odpovědi se nic nepohnulo');
        $this->assertNotSame([], (new RulesEngine())->validate($q, ActionType::RESOLVE_SIDE_STEP, ['use' => true, 'x' => 9, 'y' => 9]), 'jen nabídnuté pole');
        $this->assertSame([], (new RulesEngine())->validate($q, ActionType::RESOLVE_SIDE_STEP, ['use' => true, 'x' => 5, 'y' => 4]));
    }

    public function testSideStepAnoNaZvolenePole(): void
    {
        $q = $this->blokAI($this->stav([], [SkillName::SideStep]), 3);
        $s = $this->odpoved($q, ActionType::RESOLVE_SIDE_STEP, ['use' => true, 'x' => 5, 'y' => 4])->getNewState();

        $this->assertNull($s->getPendingSideStep());
        $this->assertSame([5, 4], [$s->requirePlayer(2)->requirePosition()->getX(), $s->requirePlayer(2)->requirePosition()->getY()]);
    }

    /** Ne ⇒ běžné odtlačení: jedno ze tří polí diagramu (sloupec 7). */
    public function testSideStepNeBezneOdtlaceni(): void
    {
        $q = $this->blokAI($this->stav([], [SkillName::SideStep]), 3);
        $s = $this->odpoved($q, ActionType::RESOLVE_SIDE_STEP, ['use' => false])->getNewState();

        $this->assertSame(7, $s->requirePlayer(2)->requirePosition()->getX());
    }

    public function testAiSideStepSeNepta(): void
    {
        $s = $this->resolver([], [3])->resolve($this->stav([], [SkillName::SideStep]), ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2])->getNewState();

        $this->assertNull($s->getPendingSideStep());
    }

    /** Grab útočníka a Side Step se ruší (ř. 8151-8153) ⇒ na Side Step se neptá. */
    public function testProtiGrabuSeNaSideStepNepta(): void
    {
        $q = $this->blokAI($this->stav([SkillName::Grab], [SkillName::SideStep]), 3);

        $this->assertNull($q->getPendingSideStep());
    }

    /** Stand Firm (otázka bez přehrání) ⇒ „ne“ ⇒ Side Step (otázka s přehráním uvnitř RESOLVE_STAND_FIRM). */
    public function testStandFirmNePakSideStep(): void
    {
        $q = $this->blokAI($this->stav([], [SkillName::StandFirm, SkillName::SideStep]), 3);
        $this->assertNotNull($q->getPendingStandFirm(), 'napřed Stand Firm');
        $q2 = $this->odpoved($q, ActionType::RESOLVE_STAND_FIRM, ['use' => false])->getNewState();
        $this->assertNotNull($q2->getPendingSideStep(), 'po „ne“ Side Step');
        $s = $this->odpoved($q2, ActionType::RESOLVE_SIDE_STEP, ['use' => true, 'x' => 5, 'y' => 6])->getNewState();

        $this->assertNull($s->getPendingSideStep());
        $this->assertNull($s->getPendingStandFirm());
        $this->assertSame([5, 6], [$s->requirePlayer(2)->requirePosition()->getX(), $s->requirePlayer(2)->requirePosition()->getY()]);
    }

    public function testUlozeniOtazekDoStavu(): void
    {
        $w = $this->blokAI($this->stav([SkillName::Block], [SkillName::Wrestle]), 2);
        $this->assertSame(2, GameState::fromArray($w->toArray())->getPendingWrestle()?->getPlayerId());
        $ss = $this->blokAI($this->stav([], [SkillName::SideStep]), 3);
        $zpet = GameState::fromArray($ss->toArray())->getPendingSideStep();
        $this->assertNotNull($zpet);
        $this->assertCount(7, $zpet->getSquares());
    }
}
