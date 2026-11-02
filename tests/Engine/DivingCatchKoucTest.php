<?php

declare(strict_types=1);

namespace App\Tests\Engine;

use App\AI\AICoachInterface;
use App\Container\Container;
use App\Container\ServiceProvider;
use App\DTO\ActionResult;
use App\DTO\GameState;
use App\Engine\ActionResolver;
use App\Engine\BallResolver;
use App\Engine\DiceRollerInterface;
use App\Engine\DivingCatchQuestion;
use App\Engine\FixedDiceRoller;
use App\Engine\InjuryResolver;
use App\Engine\KickoffResolver;
use App\Engine\RulesEngine;
use App\Engine\ScatterCalculator;
use App\Engine\TacklezoneCalculator;
use App\Enum\ActionType;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use App\Service\GameOrchestrator;
use App\ValueObject\Position;
use PHPUnit\Framework\TestCase;

/**
 * Diving Catch jako otázka kouči (uživatel 30.10.2026, parita s C++ `USE_DIVING_CATCH`).
 *
 * `rules_bb2016.txt` ř. 8065-8071: "the player CAN ATTEMPT to catch any pass, kick off or crowd
 * throw-in, but not bouncing ball, that would land in an empty square in one of his tackle zones …
 * If there are two or more players attempting to use this skill then they get in each other's way and
 * neither can use it." Skill je volba (ř. 1820 "Skill use is not mandatory").
 * Ptá se kouče-člověka hráče (i v tahu soupeře); AI rozhoduje jako dosud (skočí, kromě kopajícího
 * na lajně u výkopu).
 *
 * Přihrávky: nepřesná z (5,5) na (10,5), přesnost 2, týmový přehoz 2, rozptyl J, V, Z ⇒ míč dopadá
 * na prázdné (10,6); hráč 2 s Diving Catch stojí na (10,7) (jako DivingCatchTest).
 */
final class DivingCatchKoucTest extends TestCase
{
    private const HODY_PRIHRAVKY = [2, 2, 5, 3, 7];

    /**
     * @param list<int> $kostky
     * @param list<TeamSide> $lide
     */
    private function resolver(array $kostky, array $lide): ActionResolver
    {
        $r = new ActionResolver(new FixedDiceRoller($kostky));
        $r->setInteractiveDivingCatch($lide);

        return $r;
    }

    private function prihravka(TeamSide $skokan = TeamSide::HOME): GameState
    {
        return (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, agility: 3, id: 1)
            ->addPlayer($skokan, 10, 7, agility: 3, skills: [SkillName::DivingCatch], id: 2)
            ->withBallCarried(1)
            ->build();
    }

    /**
     * @param list<TeamSide> $lide
     * @param list<int> $navic kostky po hodech přihrávky
     */
    private function prihraj(GameState $s, array $lide, array $navic = []): ActionResult
    {
        return $this->resolver(array_merge(self::HODY_PRIHRAVKY, $navic), $lide)
            ->resolve($s, ActionType::PASS, ['playerId' => 1, 'targetX' => 10, 'targetY' => 5]);
    }

    /** @return list<string> */
    private function udalosti(ActionResult $r): array
    {
        return array_map(static fn($e) => $e->getType(), $r->getEvents());
    }

    /** @return array{0: int, 1: int}|null */
    private function mic(GameState $s): ?array
    {
        $p = $s->getBall()->getPosition();

        return $p === null ? null : [$p->getX(), $p->getY()];
    }

    public function testOtazkaPrijdeKouciSkokanaAStavZustanePredAkci(): void
    {
        $s = $this->prihravka();
        $q = $this->prihraj($s, [TeamSide::HOME]);

        $otazka = $q->getNewState()->getPendingDivingCatch();
        $this->assertNotNull($otazka, 'jediný kandidát kouče-člověka ⇒ otázka');
        $this->assertSame(2, $otazka->getPlayerId());
        $this->assertSame([10, 6], [$otazka->getLanding()->getX(), $otazka->getLanding()->getY()]);
        $this->assertSame(1, $q->getNewState()->getBall()->getCarrierId(), 'do odpovědi se stav nemění (akce se přehraje)');
        $this->assertSame(
            [ActionType::RESOLVE_DIVING_CATCH->value],
            array_column((new RulesEngine())->getAvailableActions($q->getNewState()), 'type'),
        );
        $this->assertNotSame([], (new RulesEngine())->validate($q->getNewState(), ActionType::END_TURN, []), 'nic jiného do odpovědi');
    }

    public function testAnoSkociAChyta(): void
    {
        $q = $this->prihraj($this->prihravka(), [TeamSide::HOME])->getNewState();
        // Odpověď jiným resolverem s novými kostkami: zapsané hody se přehrají, chytání 4 (AG3 4+) je nový hod.
        $r = $this->resolver([4], [TeamSide::HOME])->resolve($q, ActionType::RESOLVE_DIVING_CATCH, ['use' => true]);

        $this->assertContains('diving_catch', $this->udalosti($r));
        $this->assertSame(2, $r->getNewState()->getBall()->getCarrierId());
        $this->assertNull($r->getNewState()->getPendingDivingCatch());
        $this->assertFalse($r->isTurnover());
    }

    public function testNeBezPokusuMicDopadneNormalne(): void
    {
        $q = $this->prihraj($this->prihravka(), [TeamSide::HOME])->getNewState();
        // Ne ⇒ míč dopadne na (10,6) a odskočí: D8 4 = JV na (11,7). Kdyby skočil, 4 by chytil.
        $r = $this->resolver([4], [TeamSide::HOME])->resolve($q, ActionType::RESOLVE_DIVING_CATCH, ['use' => false]);

        $this->assertNotContains('diving_catch', $this->udalosti($r));
        $this->assertNull($r->getNewState()->getBall()->getCarrierId());
        $this->assertSame([11, 7], $this->mic($r->getNewState()));
        $this->assertTrue($r->isTurnover());
    }

    public function testPtaSeIVTahuSoupereKouceSkokana(): void
    {
        // Přihrává HOME, skáče hráč AWAY kouče-člověka ("any pass", ř. 8065).
        $q = $this->prihraj($this->prihravka(TeamSide::AWAY), [TeamSide::AWAY]);

        $this->assertSame(2, $q->getNewState()->getPendingDivingCatch()?->getPlayerId());
    }

    public function testNeptaSeKouceDruheStrany(): void
    {
        // Člověk hraje HOME, skáče hráč AWAY (AI) ⇒ bez otázky, AI skočí jako dosud (chytá 4).
        $r = $this->prihraj($this->prihravka(TeamSide::AWAY), [TeamSide::HOME], [4]);

        $this->assertNull($r->getNewState()->getPendingDivingCatch());
        $this->assertContains('diving_catch', $this->udalosti($r));
        $this->assertSame(2, $r->getNewState()->getBall()->getCarrierId());
    }

    public function testOdrazejiciSeMicSeNechytaANepta(): void
    {
        // "but not bouncing ball" (ř. 8066): odskok z (5,6) D8 5 = J na prázdné (5,7) vedle hráče na (5,8).
        $s = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 8, agility: 3, skills: [SkillName::DivingCatch], id: 1)
            ->withBallOnGround(5, 6)
            ->build();
        $ball = new BallResolver(new FixedDiceRoller([5, 6]), new TacklezoneCalculator(), new ScatterCalculator());
        $ball->setInteractiveDivingCatch([TeamSide::HOME]);

        $r = $ball->resolveBounce($s, new Position(5, 6));

        $this->assertSame([5, 7], $this->mic($r['state']));
    }

    /**
     * Vhazování od (0,7) na prázdné (5,7); hráč 1 s Diving Catch na (5,8) (jako DivingCatchTest).
     *
     * @return array{state: GameState, events: list<\App\DTO\GameEvent>}
     */
    private function vhazovani(?bool $odpoved): array
    {
        $s = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 8, agility: 3, skills: [SkillName::DivingCatch], id: 1)
            ->withBallOnGround(0, 7)
            ->build();
        $ball = new BallResolver(new FixedDiceRoller([3, 3, 3, 4]), new TacklezoneCalculator(), new ScatterCalculator());
        $ball->setInteractiveDivingCatch([TeamSide::HOME]);
        if ($odpoved !== null) {
            $ball->setDivingCatchAnswers([$odpoved]);
        }

        return $ball->resolveThrowIn($s, new Position(0, 7), new Position(-1, 7));
    }

    public function testVhazovaniSePta(): void
    {
        $this->expectException(DivingCatchQuestion::class);
        $this->vhazovani(null);
    }

    public function testVhazovaniNeOdskociZPoleDopadu(): void
    {
        // Ne ⇒ odskok z (5,7) D8 4 = JV na (6,8); ano by 4 chytil.
        $this->assertSame([6, 8], $this->mic($this->vhazovani(false)['state']));
        $this->assertSame(1, $this->vhazovani(true)['state']->getBall()->getCarrierId());
    }

    /**
     * Výkop na (10,7), rozptyl V o 2 ⇒ prázdné (12,7); jediný hráč s Diving Catch je KOPAJÍCÍ (AWAY) na
     * lajně (13,7) — jako DivingCatchTest::testKickingDivingCatchPlayerOnTheLineDoesNotDiveIntoATouchback.
     *
     * @param list<TeamSide> $lide
     * @param list<bool> $odpovedi
     * @return array{state: GameState, events: list<\App\DTO\GameEvent>}
     */
    private function vykop(array $lide, array $odpovedi): array
    {
        $s = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 6, 7, agility: 3, id: 1)
            ->addPlayer(TeamSide::AWAY, 13, 7, agility: 3, skills: [SkillName::DivingCatch], id: 2)
            ->build();
        $dice = new FixedDiceRoller([3, 2, 3, 4, 3, 3, 7]);
        $scatter = new ScatterCalculator();
        $ball = new BallResolver($dice, new TacklezoneCalculator(), $scatter);
        $ball->setInteractiveDivingCatch($lide);
        $ball->setDivingCatchAnswers($odpovedi);

        return (new KickoffResolver($dice, $scatter, $ball, new InjuryResolver()))->resolveKickoff($s, new Position(10, 7));
    }

    public function testVykopKopajicihoClovekaSePtaIKdyzByToBylTouchback(): void
    {
        $this->expectException(DivingCatchQuestion::class);
        $this->vykop([TeamSide::AWAY], []);
    }

    public function testVykopAnoKopajiciSkociDoTouchbacku(): void
    {
        // "as if it had landed in his own square" (ř. 8068) — jeho pole je v kopající polovině ⇒ touchback.
        $typy = array_map(static fn($e) => $e->getType(), $this->vykop([TeamSide::AWAY], [true])['events']);

        $this->assertContains('diving_catch', $typy);
        $this->assertContains('touchback', $typy);
    }

    public function testVykopNeAAIBezeZmenyMicOdskoci(): void
    {
        // Ne (člověk) i AI (kopající na lajně neskočí) ⇒ odraz z (12,7) D8 7 = Z na (11,7).
        foreach ([[[TeamSide::AWAY], [false]], [[TeamSide::HOME], []]] as [$lide, $odpovedi]) {
            $r = $this->vykop($lide, $odpovedi);
            $this->assertNotContains('touchback', array_map(static fn($e) => $e->getType(), $r['events']));
            $this->assertSame([11, 7], $this->mic($r['state']));
        }
    }

    /**
     * Recenze 30.10. C/2: PŘIJÍMAJÍCÍ hráč (HOME) kouče-člověka v přijímající polovině — nejčastější případ.
     * Jako DivingCatchTest::testKickOffLandingNextToDivingCatchPlayer: výkop na (6,7), rozptyl V o 1 ⇒
     * prázdné (7,7), hráč s Diving Catch na (7,8). Ne ⇒ odskok z (7,7) D8 4 = JV na (8,8); ano ⇒ chytá 4.
     *
     * @param list<bool> $odpovedi
     * @return array{state: GameState, events: list<\App\DTO\GameEvent>}
     */
    private function vykopPrijimajici(array $odpovedi): array
    {
        $s = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 7, 8, agility: 3, skills: [SkillName::DivingCatch], id: 1)
            ->addPlayer(TeamSide::AWAY, 15, 7, id: 2)
            ->build();
        $dice = new FixedDiceRoller([3, 1, 3, 4, 3, 3, 4]);
        $scatter = new ScatterCalculator();
        $ball = new BallResolver($dice, new TacklezoneCalculator(), $scatter);
        $ball->setInteractiveDivingCatch([TeamSide::HOME]);
        $ball->setDivingCatchAnswers($odpovedi);

        return (new KickoffResolver($dice, $scatter, $ball, new InjuryResolver()))->resolveKickoff($s, new Position(6, 7));
    }

    public function testVykopPrijimajicihoClovekaSePta(): void
    {
        $this->expectException(DivingCatchQuestion::class);
        $this->vykopPrijimajici([]);
    }

    public function testVykopPrijimajiciNeMicOdskociZPoleDopaduAnoChyta(): void
    {
        $this->assertSame([8, 8], $this->mic($this->vykopPrijimajici([false])['state']));
        $this->assertSame(1, $this->vykopPrijimajici([true])['state']->getBall()->getCarrierId());
    }

    public function testDveOtazkyVJedneAkciOdpovediSeSpotrebujiPopoade(): void
    {
        // Recenze 30.10. C/3: fronta odpovědí délky 2. Přihrávka (5,1) → (10,1), přesnost 2, přehoz 2,
        // rozptyl S, V, Z ⇒ prázdné (10,0) vedle hráče 2 (DC, (10,1)) = otázka 1. Ne ⇒ dopad a odraz D8 1 = S
        // ven ⇒ vhazování od horní lajny: šablona 3 = rovně dolů, 2D6 3+3 ⇒ prázdné (10,5) vedle hráče 3
        // (DC, (10,6)) = otázka 2. Ano ⇒ hráč 3 chytá 6. Kdyby přehrání dalo druhou odpověď (ano) hráči 2,
        // chytil by hráč 2.
        $s = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 1, agility: 3, id: 1)
            ->addPlayer(TeamSide::HOME, 10, 1, agility: 3, skills: [SkillName::DivingCatch], id: 2)
            ->addPlayer(TeamSide::HOME, 10, 6, agility: 3, skills: [SkillName::DivingCatch], id: 3)
            ->withBallCarried(1)
            ->build();

        $q1 = $this->resolver([2, 2, 1, 3, 7], [TeamSide::HOME])
            ->resolve($s, ActionType::PASS, ['playerId' => 1, 'targetX' => 10, 'targetY' => 1])->getNewState();
        $this->assertSame(2, $q1->getPendingDivingCatch()?->getPlayerId());

        // Odpověď jiným resolverem jen s novými kostkami — hody přihrávky se přehrají ze záznamu.
        $q2 = $this->resolver([1, 3, 3, 3], [TeamSide::HOME])
            ->resolve($q1, ActionType::RESOLVE_DIVING_CATCH, ['use' => false])->getNewState();
        $druha = $q2->getPendingDivingCatch();
        $this->assertNotNull($druha);
        $this->assertSame(3, $druha->getPlayerId(), 'druhá otázka u vhazování');
        $this->assertSame([10, 5], [$druha->getLanding()->getX(), $druha->getLanding()->getY()]);
        $this->assertSame([false], $druha->getAnswers());

        $r = $this->resolver([6], [TeamSide::HOME])->resolve($q2, ActionType::RESOLVE_DIVING_CATCH, ['use' => true]);
        $this->assertNull($r->getNewState()->getPendingDivingCatch());
        $this->assertSame(3, $r->getNewState()->getBall()->getCarrierId());
    }

    public function testVTahuAIOrchestratorZastaviAPoOdpovediTahAIPokracuje(): void
    {
        // AI = AWAY přihrává z (5,5); člověk HOME má hráče 2 s Diving Catch na (10,7).
        $s = (new GameStateBuilder())
            ->addPlayer(TeamSide::AWAY, 5, 5, agility: 3, id: 1)
            ->addPlayer(TeamSide::HOME, 10, 7, agility: 3, skills: [SkillName::DivingCatch], id: 2)
            ->addPlayer(TeamSide::HOME, 1, 1, id: 3)
            ->withBallCarried(1)
            ->withActiveTeam(TeamSide::HOME)
            ->build()
            ->withAiTeam(TeamSide::AWAY);

        $tahy = 0;
        $ai = $this->createStub(AICoachInterface::class);
        $ai->method('decideAction')->willReturnCallback(static function () use (&$tahy): array {
            return $tahy++ === 0
                ? ['action' => ActionType::PASS, 'params' => ['playerId' => 1, 'targetX' => 10, 'targetY' => 5]]
                : ['action' => ActionType::END_TURN, 'params' => []];
        });
        // Hody přihrávky, pak chytání 4 (nový hod po odpovědi), pak výplň.
        $o = new GameOrchestrator(new RulesEngine(), new FixedDiceRoller(array_merge(self::HODY_PRIHRAVKY, [4], array_fill(0, 40, 3))), $ai);

        $q = $o->executeAction($s, ActionType::END_TURN, [])['state'];
        $this->assertSame(TeamSide::AWAY, $q->getActiveTeam(), 'tah AI stojí');
        $this->assertSame(2, $q->getPendingDivingCatch()?->getPlayerId(), 'kouč-člověk dostane otázku v tahu AI');

        $po = $o->executeAction($q, ActionType::RESOLVE_DIVING_CATCH, ['use' => true])['state'];
        $this->assertNull($po->getPendingDivingCatch());
        $this->assertSame(2, $po->getBall()->getCarrierId(), 'ano ⇒ chytil (4 na 4+)');
        $this->assertSame(TeamSide::HOME, $po->getActiveTeam(), 'míč u soupeře = turnover, tah AI skončil');
    }

    public function testOtazkaPrezijeUlozeniStavu(): void
    {
        // Stav se mezi požadavky ukládá (toArray/fromArray) — odpověď musí jít i po obnovení.
        $q = $this->prihraj($this->prihravka(), [TeamSide::HOME])->getNewState();
        $ulozeny = GameState::fromArray(json_decode((string) json_encode($q->toArray()), true));

        $r = $this->resolver([4], [TeamSide::HOME])->resolve($ulozeny, ActionType::RESOLVE_DIVING_CATCH, ['use' => true]);
        $this->assertSame(2, $r->getNewState()->getBall()->getCarrierId());
    }

    public function testActionResolverZKontejneruPrehrajeAkciSeStejnymiHody(): void
    {
        // Recenze 30.10. C/1: kontejner předával resolvery s vlastními kostkami mimo záznam ⇒ po odpovědi
        // se přihrávka přehrála s NOVÝMI hody (tady 4, 3, 3 … místo 2, 2, 5, 3, 7) a míč šel jinam.
        $c = new Container();
        ServiceProvider::register($c);
        $c->set(DiceRollerInterface::class, fn() => new FixedDiceRoller(array_merge(self::HODY_PRIHRAVKY, [4], array_fill(0, 20, 3))));
        $r = $c->get(ActionResolver::class);
        $r->setInteractiveDivingCatch([TeamSide::HOME]);

        $q = $r->resolve($this->prihravka(), ActionType::PASS, ['playerId' => 1, 'targetX' => 10, 'targetY' => 5])->getNewState();
        $this->assertSame(2, $q->getPendingDivingCatch()?->getPlayerId());

        $po = $r->resolve($q, ActionType::RESOLVE_DIVING_CATCH, ['use' => true]);
        $this->assertContains('diving_catch', $this->udalosti($po));
        $this->assertSame(2, $po->getNewState()->getBall()->getCarrierId(), 'přehrání = tytéž hody, pak chytání 4');
    }
}
