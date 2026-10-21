<?php

declare(strict_types=1);

namespace App\Tests\Engine;

use App\AI\AICoachInterface;
use App\DTO\ActionResult;
use App\DTO\GameState;
use App\Engine\ActionResolver;
use App\Engine\FixedDiceRoller;
use App\Engine\RulesEngine;
use App\Enum\ActionType;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use App\Exception\ValidationException;
use App\Service\GameOrchestrator;
use PHPUnit\Framework\TestCase;

/**
 * P226 — PARITA S C++ P130 (`holdsGround` → `USE_STAND_FIRM`): webová hra se kouče-člověka
 * bránícího hráčem se Stand Firm ptá, jestli ho použije.
 *
 * `rules_bb2016.txt` ř. 8510-8516 (Stand Firm): "A player with this skill **may choose** to not be
 * pushed back as the result of a block … If a player is pushed back into a player with using Stand
 * Firm then neither player moves." Skill je volba (ř. 1820 "Skill use is not mandatory.").
 * Zakořeněný se neptá ("may not be pushed back for any reason", ř. 8578-8579). AI se neptá.
 */
final class StandFirmKoucTest extends TestCase
{
    /**
     * @param list<int> $kostky
     * @param list<TeamSide> $lide strany kouče-člověka (ptá se jich na Stand Firm)
     */
    private function resolver(array $kostky, array $lide = [TeamSide::HOME, TeamSide::AWAY]): ActionResolver
    {
        $r = new ActionResolver(new FixedDiceRoller($kostky));
        $r->setInteractiveBlocks(true);
        $r->setInteractiveStandFirm($lide);

        return $r;
    }

    /**
     * Útočník HOME (5,7) → obránce AWAY (6,7) se Stand Firm; okolí prázdné.
     *
     * @param list<SkillName> $utocnik
     * @param list<SkillName> $obrance
     */
    private function stav(array $utocnik = [], array $obrance = [SkillName::StandFirm]): GameState
    {
        return (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, skills: $utocnik, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 7, skills: $obrance, id: 2)
            ->withBallOffPitch()
            ->build();
    }

    /**
     * Hodí blok (1 kostka = Pushed) a útočník zvolí první kostku.
     *
     * @param list<TeamSide> $lide
     * @param list<int> $kostky
     * @param array<string, mixed> $navic
     */
    private function zvolKostku(GameState $stav, array $lide = [TeamSide::HOME, TeamSide::AWAY], array $navic = [], ActionType $akce = ActionType::BLOCK, array $kostky = [3]): ActionResult
    {
        $hod = $this->resolver($kostky, $lide)->resolve($stav, $akce, ['playerId' => 1, 'targetId' => 2]);
        $this->assertNotNull($hod->getNewState()->getPendingBlock(), 'fixtura: čeká volba kostky');

        return $this->resolver([3, 3, 3, 3, 3, 3], $lide)->resolve($hod->getNewState(), ActionType::CHOOSE_BLOCK_DIE, ['faceIndex' => 0] + $navic);
    }

    /** @param array<string, mixed> $params */
    private function odpovez(GameState $s, array $params): ActionResult
    {
        return $this->resolver(array_fill(0, 12, 3))->resolve($s, ActionType::RESOLVE_STAND_FIRM, $params);
    }

    /** @return array{0: int, 1: int} */
    private function pole(GameState $s, int $id): array
    {
        $p = $s->requirePlayer($id)->requirePosition();

        return [$p->getX(), $p->getY()];
    }

    public function testClovekObranceDostaneOtazkuANicSeZatimNehne(): void
    {
        $s = $this->zvolKostku($this->stav())->getNewState();

        $otazka = $s->getPendingStandFirm();
        $this->assertNotNull($otazka, 'Pushed + Stand Firm + kouč-člověk ⇒ otázka');
        $this->assertSame(2, $otazka->getPlayerId());
        $this->assertSame([6, 7], $this->pole($s, 2));
        $this->assertSame([5, 7], $this->pole($s, 1));
        $this->assertSame([ActionType::RESOLVE_STAND_FIRM->value], array_column((new RulesEngine())->getAvailableActions($s), 'type'));
    }

    public function testNeObranceOdtlacenAUtocnikNasleduje(): void
    {
        $q = $this->zvolKostku($this->stav())->getNewState();
        $s = $this->odpovez($q, ['use' => false])->getNewState();

        $this->assertNull($s->getPendingStandFirm());
        $this->assertNull($s->getPendingBlock());
        $this->assertSame(7, $this->pole($s, 2)[0], 'bez Stand Firm odtlačen za (6,7)');
        $this->assertSame([6, 7], $this->pole($s, 1), 'follow-up');
    }

    public function testAnoObranceDrziPole(): void
    {
        $q = $this->zvolKostku($this->stav())->getNewState();
        $s = $this->odpovez($q, ['use' => true])->getNewState();

        $this->assertNull($s->getPendingStandFirm());
        $this->assertNull($s->getPendingBlock());
        $this->assertSame([6, 7], $this->pole($s, 2));
        $this->assertSame([5, 7], $this->pole($s, 1));
        $this->assertTrue($s->requirePlayer(1)->hasActed(), 'blok doběhl');
    }

    public function testFollowUpZKostkySeZachovaPresOtazku(): void
    {
        $q = $this->zvolKostku($this->stav(), navic: ['followUp' => false])->getNewState();
        $s = $this->odpovez($q, ['use' => false])->getNewState();

        $this->assertSame(7, $this->pole($s, 2)[0]);
        $this->assertSame([5, 7], $this->pole($s, 1), 'followUp=false z volby kostky platí i po otázce');
    }

    /** Obránce bez Stand Firm, za ním tři hráči se Stand Firm ⇒ řetěz (ř. 641-646, 8514-8516). */
    private function retez(): GameState
    {
        return (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 7, id: 2)
            ->addPlayer(TeamSide::AWAY, 7, 6, skills: [SkillName::StandFirm], id: 3)
            ->addPlayer(TeamSide::AWAY, 7, 7, skills: [SkillName::StandFirm], id: 4)
            ->addPlayer(TeamSide::AWAY, 7, 8, skills: [SkillName::StandFirm], id: 5)
            ->withBallOffPitch()
            ->build();
    }

    public function testRetezSeKouceZeptaANeUvolniPole(): void
    {
        // 1 kostka Pushed (hráči za obráncem útočníka nesousedí ⇒ žádné asistence)
        $q = $this->zvolKostku($this->retez(), kostky: [3, 3, 3])->getNewState();
        $otazka = $q->getPendingStandFirm();
        $this->assertNotNull($otazka, 'řetěz do hráče se Stand Firm ⇒ otázka jeho kouči');
        $kdo = $otazka->getPlayerId();
        $this->assertContains($kdo, [3, 4, 5]);
        $puvodni = $this->pole($q, $kdo);
        $this->assertSame([6, 7], $this->pole($q, 2), 'do odpovědi se nikdo nehne');

        $s = $this->odpovez($q, ['use' => false])->getNewState();
        $this->assertNull($s->getPendingStandFirm());
        $this->assertSame($puvodni, $this->pole($s, 2), 'obránce na uvolněné pole');
        $this->assertSame(8, $this->pole($s, $kdo)[0], 'hráč v řetězu odtlačen dál');
    }

    public function testRetezAnoUVsechNikdoSeNehne(): void
    {
        $s = $this->zvolKostku($this->retez(), kostky: [3, 3, 3])->getNewState();
        $tazani = [];
        while (($otazka = $s->getPendingStandFirm()) !== null) {
            $tazani[] = $otazka->getPlayerId();
            $this->assertLessThan(4, count($tazani), 'každý se ptá jednou');
            $s = $this->odpovez($s, ['use' => true])->getNewState();
        }
        sort($tazani);
        $this->assertSame([3, 4, 5], $tazani);
        $this->assertSame([6, 7], $this->pole($s, 2), '"neither player moves"');
        $this->assertNull($s->getPendingBlock());
    }

    public function testVolnePoleZaObrancemRetezNeniNepta(): void
    {
        // hráč se Stand Firm jen vedle -- odtlačení do volného pole ho nepotřebuje
        $s = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 7, id: 2)
            ->addPlayer(TeamSide::AWAY, 7, 7, skills: [SkillName::StandFirm], id: 4)
            ->withBallOffPitch()
            ->build();
        $r = $this->zvolKostku($s, kostky: [3, 3])->getNewState();

        $this->assertNull($r->getPendingStandFirm());
        $this->assertSame(7, $this->pole($r, 2)[0]);
    }

    public function testZakorenenySeNepta(): void
    {
        $s = $this->stav();
        $s = $s->withPlayer($s->requirePlayer(2)->withRooted(true));
        $r = $this->zvolKostku($s)->getNewState();

        $this->assertNull($r->getPendingStandFirm());
        $this->assertSame([6, 7], $this->pole($r, 2));
    }

    public function testAIObranceSeNeptaADrziPole(): void
    {
        // jen HOME je člověk ⇒ AWAY (AI) rozhoduje sám jako dosud
        $r = $this->zvolKostku($this->stav(), lide: [TeamSide::HOME])->getNewState();

        $this->assertNull($r->getPendingStandFirm());
        $this->assertSame([6, 7], $this->pole($r, 2));
    }

    public function testJuggernautVBlitzuSeNepta(): void
    {
        // Juggernaut ř. 8193-8196: v blitzu soupeř Stand Firm použít nesmí
        $r = $this->zvolKostku($this->stav(utocnik: [SkillName::Juggernaut]), akce: ActionType::BLITZ)->getNewState();

        $this->assertNull($r->getPendingStandFirm());
        $this->assertSame(7, $this->pole($r, 2)[0]);
    }

    public function testNeStandFirmPakOtazkaNaGrab(): void
    {
        // P224 se proti Stand Firm neptá; po odmítnutí Stand Firm se útočníka ptá na Grab
        $q = $this->zvolKostku($this->stav(utocnik: [SkillName::Grab]))->getNewState();
        $this->assertNotNull($q->getPendingStandFirm());
        $this->assertNull($q->getPendingGrab(), 'napřed Stand Firm');

        $g = $this->odpovez($q, ['use' => false])->getNewState();
        $this->assertNull($g->getPendingStandFirm());
        $this->assertNotNull($g->getPendingGrab(), 'obránce se odtlačit nechá ⇒ Grab');

        $s = $this->resolver([3, 3, 3])->resolve($g, ActionType::RESOLVE_GRAB, ['use' => true, 'x' => 6, 'y' => 6])->getNewState();
        $this->assertNull($s->getPendingStandFirm(), 'na Stand Firm se znovu neptá');
        $this->assertSame([6, 6], $this->pole($s, 2));
    }

    public function testAnoStandFirmGrabSeNepta(): void
    {
        $q = $this->zvolKostku($this->stav(utocnik: [SkillName::Grab]))->getNewState();
        $s = $this->odpovez($q, ['use' => true])->getNewState();

        $this->assertNull($s->getPendingGrab());
        $this->assertSame([6, 7], $this->pole($s, 2));
    }

    public function testValidace(): void
    {
        $q = $this->zvolKostku($this->stav())->getNewState();
        $e = new RulesEngine();

        $this->assertNotSame([], $e->validate($q, ActionType::END_TURN, []), 'dokud kouč neodpoví, nic jiného');
        $this->assertNotSame([], $e->validate($q, ActionType::CHOOSE_BLOCK_DIE, ['faceIndex' => 0]));
        foreach (['abc', 'true', 1, 0, '', null] as $podvrh) {
            $this->assertNotSame([], $e->validate($q, ActionType::RESOLVE_STAND_FIRM, ['use' => $podvrh]), 'use=' . var_export($podvrh, true));
        }
        $this->assertNotSame([], $e->validate($q, ActionType::RESOLVE_STAND_FIRM, []));
        $this->assertSame([], $e->validate($q, ActionType::RESOLVE_STAND_FIRM, ['use' => true]));
        $this->assertNotSame([], $e->validate($this->stav(), ActionType::RESOLVE_STAND_FIRM, ['use' => true]), 'bez otázky');
    }

    public function testOtazkaProjdeToArrayFromArray(): void
    {
        // i s odpověďmi v řetězu a followUp z volby kostky
        $q = $this->zvolKostku($this->retez(), navic: ['followUp' => false], kostky: [3, 3, 3])->getNewState();
        $q = $this->odpovez($q, ['use' => true])->getNewState();
        $this->assertNotNull($q->getPendingStandFirm());

        $znovu = GameState::fromArray(json_decode(json_encode($q->toArray(), JSON_THROW_ON_ERROR), true, 512, JSON_THROW_ON_ERROR));
        $this->assertEquals($q->getPendingStandFirm(), $znovu->getPendingStandFirm());
        $this->assertNull(GameState::fromArray($this->stav()->toArray())->getPendingStandFirm());

        // grab z odpovědi se zachová
        $g = $this->odpovez($this->zvolKostku($this->stav(utocnik: [SkillName::Grab]))->getNewState(), ['use' => false])->getNewState();
        $g2 = GameState::fromArray(json_decode(json_encode($g->toArray(), JSON_THROW_ON_ERROR), true, 512, JSON_THROW_ON_ERROR));
        $this->assertEquals($g->getPendingGrab(), $g2->getPendingGrab());
    }

    public function testVTahuAIOrchestratorZastaviAPoOdpovediTahAIPokracuje(): void
    {
        // AI = AWAY útočí, člověk HOME brání hráčem se Stand Firm
        $s = (new GameStateBuilder())
            ->addPlayer(TeamSide::AWAY, 5, 7, id: 1)
            ->addPlayer(TeamSide::HOME, 6, 7, skills: [SkillName::StandFirm], id: 2)
            ->addPlayer(TeamSide::HOME, 1, 1, id: 3)
            ->withBallOffPitch()
            ->withActiveTeam(TeamSide::HOME)
            ->build()
            ->withAiTeam(TeamSide::AWAY);

        $tahy = 0;
        $ai = $this->createStub(AICoachInterface::class);
        $ai->method('decideAction')->willReturnCallback(static function () use (&$tahy): array {
            return $tahy++ === 0
                ? ['action' => ActionType::BLOCK, 'params' => ['playerId' => 1, 'targetId' => 2]]
                : ['action' => ActionType::END_TURN, 'params' => []];
        });
        $o = new GameOrchestrator(new RulesEngine(), new FixedDiceRoller(array_fill(0, 40, 3)), $ai);

        $r = $o->executeAction($s, ActionType::END_TURN, []);
        $q = $r['state'];
        $this->assertSame(TeamSide::AWAY, $q->getActiveTeam(), 'tah AI stojí');
        $this->assertNotNull($q->getPendingStandFirm(), 'kouč-člověk dostane otázku v tahu AI');
        $this->assertSame(2, $q->getPendingStandFirm()->getPlayerId());
        $this->assertSame([6, 7], $this->pole($q, 2));

        $po = $o->executeAction($q, ActionType::RESOLVE_STAND_FIRM, ['use' => false])['state'];
        $this->assertNull($po->getPendingStandFirm());
        $this->assertSame(7, $this->pole($po, 2)[0], 'ne ⇒ odtlačen');
        $this->assertSame(TeamSide::HOME, $po->getActiveTeam(), 'AI dohrála tah (END_TURN)');

        $this->expectException(ValidationException::class);
        $o->executeAction($q, ActionType::END_TURN, []);
    }

    /**
     * Review P226 (HIGH): v tahu AI první rána Frenzy řetězem vytlačí hráče kouče do davu (KO ⇒
     * nabídka lékárníka, P206 b) a druhá rána položí otázku na Stand Firm. Obojí čeká naráz --
     * lékárník má přednost (odpovídá se na výsledek první rány), Stand Firm se zeptá potom.
     * Dřív RulesEngine odmítl obě odpovědi a partie nešla dohrát.
     */
    public function testLekarnikPredOtazkouNaStandFirmVTahuAI(): void
    {
        $s = (new GameStateBuilder())
            ->addPlayer(TeamSide::AWAY, 6, 2, skills: [SkillName::Frenzy], id: 1)
            ->addPlayer(TeamSide::HOME, 6, 1, skills: [SkillName::StandFirm], id: 2)
            ->addPlayer(TeamSide::HOME, 5, 0, id: 3)
            ->addPlayer(TeamSide::HOME, 6, 0, id: 4)
            ->addPlayer(TeamSide::HOME, 7, 0, id: 5)
            ->withBallOffPitch()
            ->withActiveTeam(TeamSide::HOME)
            ->build()
            ->withAiTeam(TeamSide::AWAY);
        $tahy = 0;
        $ai = $this->createStub(AICoachInterface::class);
        $ai->method('decideAction')->willReturnCallback(static function () use (&$tahy): array {
            return $tahy++ === 0
                ? ['action' => ActionType::BLOCK, 'params' => ['playerId' => 1, 'targetId' => 2]]
                : ['action' => ActionType::END_TURN, 'params' => []];
        });
        // blok Pushed | zranění v davu 4+4 = KO | druhá rána Pushed, dál trojky
        $o = new GameOrchestrator(new RulesEngine(), new FixedDiceRoller(array_merge([3], [4, 4], array_fill(0, 60, 3))), $ai);
        $e = new RulesEngine();

        $q = $o->executeAction($s, ActionType::END_TURN, [])['state'];
        $this->assertNotNull($q->getPendingStandFirm(), 'fixtura: otázka na první ránu');
        $oba = $o->executeAction($q, ActionType::RESOLVE_STAND_FIRM, ['use' => false])['state'];
        $this->assertNotNull($oba->getPendingApothecary(), 'fixtura: hráč v davu KO ⇒ lékárník');
        $this->assertNotNull($oba->getPendingStandFirm(), 'fixtura: druhá rána Frenzy ⇒ otázka');

        $this->assertSame([ActionType::RESOLVE_APOTHECARY->value], array_column($e->getAvailableActions($oba), 'type'));
        $this->assertSame([], $e->validate($oba, ActionType::RESOLVE_APOTHECARY, ['playerId' => 0]), 'lékárník má přednost');
        $this->assertNotSame([], $e->validate($oba, ActionType::RESOLVE_STAND_FIRM, ['use' => true]), 'Stand Firm až po lékárníkovi');

        $poLekarnikovi = $o->executeAction($oba, ActionType::RESOLVE_APOTHECARY, ['playerId' => 0])['state'];
        $this->assertNull($poLekarnikovi->getPendingApothecary());
        $this->assertNotNull($poLekarnikovi->getPendingStandFirm(), 'otázka na Stand Firm dál čeká');
        $this->assertSame(TeamSide::AWAY, $poLekarnikovi->getActiveTeam(), 'tah AI stojí');
        $this->assertSame([ActionType::RESOLVE_STAND_FIRM->value], array_column($e->getAvailableActions($poLekarnikovi), 'type'));

        $konec = $o->executeAction($poLekarnikovi, ActionType::RESOLVE_STAND_FIRM, ['use' => true])['state'];
        $this->assertNull($konec->getPendingStandFirm());
        $this->assertNull($konec->getPendingBlock());
        $this->assertSame(TeamSide::HOME, $konec->getActiveTeam(), 'AI dohrála tah');
    }

    // === review MEDIUM 2: otázka nese stranu kostky a pole, kam by hráč bez Stand Firm šel ===

    public function testOtazkaNeseStranuKostkyAPoleKamBySel(): void
    {
        foreach ([3 => \App\Enum\BlockDiceFace::PUSHED, 6 => \App\Enum\BlockDiceFace::DEFENDER_DOWN] as $kostka => $strana) {
            $q = $this->zvolKostku($this->stav(), kostky: [$kostka])->getNewState();
            $otazka = $q->getPendingStandFirm();
            $this->assertNotNull($otazka);
            $this->assertSame($strana, $otazka->getFace(), 'ř. 8511-8513: Pushed vs Knock-down');
            $this->assertSame(0, $otazka->getFaceIndex());

            $po = $this->odpovez($q, ['use' => false])->getNewState();
            $this->assertNotNull($otazka->getPushTo());
            $this->assertSame($this->pole($po, 2), [$otazka->getPushTo()->getX(), $otazka->getPushTo()->getY()], 'pole v otázce = skutečné odtlačení');
        }
    }

    public function testOtazkaVRetezuNesePoleHraceVRetezu(): void
    {
        $q = $this->zvolKostku($this->retez())->getNewState();
        $otazka = $q->getPendingStandFirm();
        $this->assertNotNull($otazka);
        $pole = $otazka->getPushTo();
        $this->assertNotNull($pole);
        $this->assertSame(8, $pole->getX());

        $po = $this->odpovez($q, ['use' => false])->getNewState();
        $this->assertSame($this->pole($po, $otazka->getPlayerId()), [$pole->getX(), $pole->getY()]);
    }

    public function testOtazkaUDavuNemaPole(): void
    {
        $s = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 1, id: 1)
            ->addPlayer(TeamSide::AWAY, 5, 0, skills: [SkillName::StandFirm], id: 2)
            ->withBallOffPitch()
            ->build();
        $otazka = $this->zvolKostku($s)->getNewState()->getPendingStandFirm();
        $this->assertNotNull($otazka);
        $this->assertNull($otazka->getPushTo(), 'do davu');
    }

    // === Ball & Chain (uživatel 21.10. „1 ano“): blok "following normal blocking rules" (ř. 7829-7831) ===

    /** B&C HOME (5,7), pohyb 1, síla 7 ⇒ šablona rovně na (6,7) (kostka 3), 3 kostky Pushed. */
    private function bnc(): GameState
    {
        return (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, movement: 1, strength: 7, skills: [SkillName::BallAndChain, SkillName::NoHands], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 7, skills: [SkillName::StandFirm], id: 2)
            ->withBallOffPitch()
            ->build();
    }

    public function testBallAndChainSeKouceZeptaANeOdtlaciAFollowUp(): void
    {
        $q = $this->resolver(array_fill(0, 20, 3))->resolve($this->bnc(), ActionType::BALL_AND_CHAIN, ['playerId' => 1])->getNewState();
        $otazka = $q->getPendingStandFirm();
        $this->assertNotNull($otazka, 'B&C blokuje hráče kouče-člověka se Stand Firm ⇒ otázka');
        $this->assertSame('bnc', $otazka->getKind());
        $this->assertSame([5, 7], $this->pole($q, 1));
        $this->assertSame([6, 7], $this->pole($q, 2));
        $this->assertFalse($q->requirePlayer(1)->hasActed());

        $s = $this->odpovez($q, ['use' => false])->getNewState();
        $this->assertNull($s->getPendingStandFirm());
        $this->assertSame(7, $this->pole($s, 2)[0], 'odtlačen');
        $this->assertSame([6, 7], $this->pole($s, 1), 'povinný follow-up B&C');
        $this->assertTrue($s->requirePlayer(1)->hasActed());
    }

    public function testBallAndChainAnoFanatikZustaneAPolePohybuJeSpotrebovane(): void
    {
        // FAQ 05/2020: hráč se Stand Firm se nenechá odtlačit ⇒ B&C zůstane na svém poli a krok ho
        // to stálo (pohyb 1 ⇒ akce skončila)
        $q = $this->resolver(array_fill(0, 20, 3))->resolve($this->bnc(), ActionType::BALL_AND_CHAIN, ['playerId' => 1])->getNewState();
        $s = $this->odpovez($q, ['use' => true])->getNewState();

        $this->assertNull($s->getPendingStandFirm());
        $this->assertSame([6, 7], $this->pole($s, 2));
        $this->assertSame([5, 7], $this->pole($s, 1));
        $this->assertTrue($s->requirePlayer(1)->hasActed(), 'krok spotřebován, pohyb 1 ⇒ konec');
    }

    public function testBallAndChainAIObranceSeNepta(): void
    {
        $s = $this->resolver(array_fill(0, 20, 3), [TeamSide::HOME])->resolve($this->bnc(), ActionType::BALL_AND_CHAIN, ['playerId' => 1])->getNewState();

        $this->assertNull($s->getPendingStandFirm());
        $this->assertSame([6, 7], $this->pole($s, 2), 'AI Stand Firm použije');
    }

    public function testBallAndChainProtiVlastnimuHraciSeStandFirmSeNepta(): void
    {
        // Stand Firm drží jen proti soupeři (`holdsGround`) -- vlastní hráč B&C se odtlačí, otázka není.
        // Vlastní hráči na všech čtyřech stranách, ať se šablona trefí do některého z nich.
        $s = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, movement: 1, strength: 7, skills: [SkillName::BallAndChain, SkillName::NoHands], id: 1)
            ->addPlayer(TeamSide::HOME, 6, 7, skills: [SkillName::StandFirm], id: 2)
            ->addPlayer(TeamSide::HOME, 4, 7, skills: [SkillName::StandFirm], id: 3)
            ->addPlayer(TeamSide::HOME, 5, 6, skills: [SkillName::StandFirm], id: 4)
            ->addPlayer(TeamSide::HOME, 5, 8, skills: [SkillName::StandFirm], id: 5)
            ->withBallOffPitch()
            ->build();
        $r = $this->resolver(array_fill(0, 20, 3))->resolve($s, ActionType::BALL_AND_CHAIN, ['playerId' => 1])->getNewState();

        $this->assertNull($r->getPendingStandFirm());
        $this->assertNotSame([5, 7], $this->pole($r, 1), 'vlastní hráč odtlačen, B&C následoval');
    }

    // === Multiple Block (uživatel 21.10. „2 ano“): "Make each block in turn as normal", bez follow-upu ===

    private function multipleBlock(): GameState
    {
        return (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, skills: [SkillName::MultipleBlock], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 7, skills: [SkillName::StandFirm], id: 2)
            ->addPlayer(TeamSide::AWAY, 4, 7, skills: [SkillName::StandFirm], id: 3)
            ->withBallOffPitch()
            ->build();
    }

    public function testMultipleBlockSePtaUKazdehoCile(): void
    {
        $r = $this->resolver(array_fill(0, 30, 3));
        $q = $r->resolve($this->multipleBlock(), ActionType::MULTIPLE_BLOCK, ['playerId' => 1, 'targetId' => 2, 'targetId2' => 3])->getNewState();
        $prvni = $q->getPendingStandFirm();
        $this->assertNotNull($prvni, 'první cíl');
        $this->assertSame('mb', $prvni->getKind());
        $this->assertSame(2, $prvni->getPlayerId());
        $this->assertSame([6, 7], $this->pole($q, 2));
        $this->assertSame([4, 7], $this->pole($q, 3));

        $q2 = $this->odpovez($q, ['use' => false])->getNewState();
        $druha = $q2->getPendingStandFirm();
        $this->assertNotNull($druha, 'druhý cíl');
        $this->assertSame(3, $druha->getPlayerId());
        $this->assertSame(7, $this->pole($q2, 2)[0], 'první odtlačen');
        $this->assertFalse($q2->requirePlayer(1)->hasActed());

        $s = $this->odpovez($q2, ['use' => true])->getNewState();
        $this->assertNull($s->getPendingStandFirm());
        $this->assertSame([4, 7], $this->pole($s, 3), 'druhý drží');
        $this->assertSame([5, 7], $this->pole($s, 1), 'Multiple Block bez follow-upu');
        $this->assertTrue($s->requirePlayer(1)->hasActed());
    }

    public function testMultipleBlockAIObranceSeNepta(): void
    {
        $s = $this->resolver(array_fill(0, 30, 3), [TeamSide::HOME])
            ->resolve($this->multipleBlock(), ActionType::MULTIPLE_BLOCK, ['playerId' => 1, 'targetId' => 2, 'targetId2' => 3])->getNewState();

        $this->assertNull($s->getPendingStandFirm());
        $this->assertSame([6, 7], $this->pole($s, 2));
        $this->assertSame([4, 7], $this->pole($s, 3));
        $this->assertTrue($s->requirePlayer(1)->hasActed());
    }
}
