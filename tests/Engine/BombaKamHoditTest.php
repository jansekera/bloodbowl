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
use App\Enum\PlayerState;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use App\Service\GameOrchestrator;
use PHPUnit\Framework\TestCase;

/**
 * P272 (uživatel 9.11.2026: „player must try to catch and if success throw — na ptaní mi z toho vyplývá — kam hodit
 * po chycení“) — webová hra, parita s C++ `CoachQuestion::BOMB_RETHROW_TARGET`.
 *
 * `rules_bb2016.txt` ř. 7959-7963: chycenou / zachycenou bombu hráč hned hodí dál (mimořádná akce). Chytání je
 * povinné (engine zkouší vždy); kouče-člověka se ptá jen, KAM hodit. Hráče AI volí engine jako dosud.
 *
 * Scéna (jako BombThrowTest::testP264ChycenouBombuChytajiciHodiDal): HOME 1 bombardier bez rukou na (5,7) hází
 * na AWAY 12 na (10,7); hod 6 přesně, 12 chytá 6. Engine by hodil zpět na 1 (jediný soupeř = cena 1).
 */
final class BombaKamHoditTest extends TestCase
{
    private function scena(): GameState
    {
        return (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1, skills: [SkillName::Bombardier, SkillName::NoHands])
            ->addPlayer(TeamSide::AWAY, 10, 7, id: 12)
            ->withBallOffPitch()
            ->build();
    }

    /**
     * @param list<int> $kostky
     * @param list<TeamSide> $lide
     */
    private function resolver(array $kostky, array $lide = [TeamSide::AWAY]): ActionResolver
    {
        $r = new ActionResolver(new FixedDiceRoller($kostky));
        $r->setInteractiveBombRethrow($lide);

        return $r;
    }

    private function hod(GameState $s, ActionResolver $r): ActionResult
    {
        return $r->resolve($s, ActionType::BOMB_THROW, ['playerId' => 1, 'targetX' => 10, 'targetY' => 7]);
    }

    public function testKouceChytajicihoSeZeptaKamHoditAStavZustanePredAkci(): void
    {
        $q = $this->hod($this->scena(), $this->resolver([6, 6]))->getNewState();

        $otazka = $q->getPendingBombRethrow();
        $this->assertNotNull($otazka, 'bombu chytil hráč kouče-člověka ⇒ otázka');
        $this->assertSame(12, $otazka->getPlayerId());
        $this->assertContains(['x' => 14, 'y' => 7, 'range' => 'short_pass'], $otazka->getTargets(), 'nabídka = legální pole hodu z (10,7)');
        $this->assertNotContains(10, array_map(static fn(array $t) => $t['x'] === 10 && $t['y'] === 7 ? 10 : 0, $otazka->getTargets()), 'vlastní pole ne');
        $this->assertSame(['x' => 5, 'y' => 7], $otazka->getSuggested(), 'doporučení = volba enginu (na bombardiera bez rukou)');
        $this->assertFalse($q->requirePlayer(1)->hasActed(), 'do odpovědi stav před akcí (akce se přehraje)');
        $this->assertSame(
            [ActionType::RESOLVE_BOMB_RETHROW->value],
            array_column((new RulesEngine())->getAvailableActions($q), 'type'),
        );
        $this->assertNotSame([], (new RulesEngine())->validate($q, ActionType::END_TURN, []), 'nic jiného do odpovědi');
    }

    public function testBombaLetiTamKamKoucUkazal(): void
    {
        $q = $this->hod($this->scena(), $this->resolver([6, 6]))->getNewState();
        // Přehrají se zapsané hody (6, 6), nový je jen hod 12 na (14,7): 6 přesně ⇒ výbuch v prázdnu, nikdo nepadne.
        $kostky = new FixedDiceRoller([6]);
        $r = new ActionResolver($kostky);
        $r->setInteractiveBombRethrow([TeamSide::AWAY]);
        $po = $r->resolve($q, ActionType::RESOLVE_BOMB_RETHROW, ['targetX' => 14, 'targetY' => 7]);

        $this->assertNull($po->getNewState()->getPendingBombRethrow());
        $this->assertSame(PlayerState::STANDING, $po->getNewState()->requirePlayer(1)->getState(), 'engine by hodil na 1');
        $this->assertSame(PlayerState::STANDING, $po->getNewState()->requirePlayer(12)->getState());
        $this->assertTrue($po->getNewState()->requirePlayer(1)->hasActed());
        $this->assertFalse($po->isTurnover());
        $this->assertFalse($kostky->hasRemainingRolls());
        $dopady = array_values(array_filter($po->getEvents(), static fn($e) => $e->getType() === 'bomb_landing'));
        $this->assertCount(1, $dopady);
        $this->assertSame('(14,7)', $dopady[0]->getData()['position'] ?? null);
    }

    public function testPoleMimoNabidkuSeOdmitne(): void
    {
        $q = $this->hod($this->scena(), $this->resolver([6, 6]))->getNewState();
        $rules = new RulesEngine();

        $this->assertNotSame([], $rules->validate($q, ActionType::RESOLVE_BOMB_RETHROW, ['targetX' => 25, 'targetY' => 0]), 'mimo dosah');
        $this->assertNotSame([], $rules->validate($q, ActionType::RESOLVE_BOMB_RETHROW, []), 'bez cíle');
        $this->assertSame([], $rules->validate($q, ActionType::RESOLVE_BOMB_RETHROW, ['targetX' => 14, 'targetY' => 7]));
        $this->assertNotSame([], $rules->validate($this->scena(), ActionType::RESOLVE_BOMB_RETHROW, ['targetX' => 14, 'targetY' => 7]), 'bez otázky odpověď nejde');
    }

    public function testHraceAINeptaEngineHodiSam(): void
    {
        // Člověk hraje HOME, chytá hráč AWAY (AI) ⇒ bez otázky, engine hodí zpět na 1: 6 přesně; zbroj 3+3.
        $kostky = new FixedDiceRoller([6, 6, 6, 3, 3]);
        $r = new ActionResolver($kostky);
        $r->setInteractiveBombRethrow([TeamSide::HOME]);
        $po = $this->hod($this->scena(), $r);

        $this->assertNull($po->getNewState()->getPendingBombRethrow());
        $this->assertSame(PlayerState::PRONE, $po->getNewState()->requirePlayer(1)->getState());
        $this->assertFalse($kostky->hasRemainingRolls());
    }

    public function testOtazkaPrezijeUlozeniStavu(): void
    {
        $q = $this->hod($this->scena(), $this->resolver([6, 6]))->getNewState();
        $ulozeny = GameState::fromArray(json_decode((string) json_encode($q->toArray()), true));

        $this->assertSame(12, $ulozeny->getPendingBombRethrow()?->getPlayerId());
        $po = $this->resolver([6])->resolve($ulozeny, ActionType::RESOLVE_BOMB_RETHROW, ['targetX' => 14, 'targetY' => 7]);
        $this->assertSame(PlayerState::STANDING, $po->getNewState()->requirePlayer(1)->getState());
    }

    public function testVTahuAIOrchestratorZastaviAPoOdpovediTahAIPokracuje(): void
    {
        // AI = HOME hází bombu na hráče 12 kouče-člověka (AWAY); ten ji chytí ⇒ tah AI stojí na otázce.
        $s = $this->scena()->withAiTeam(TeamSide::HOME);
        $tahy = 0;
        $ai = $this->createStub(AICoachInterface::class);
        $ai->method('decideAction')->willReturnCallback(static function () use (&$tahy): array {
            return $tahy++ === 0
                ? ['action' => ActionType::BOMB_THROW, 'params' => ['playerId' => 1, 'targetX' => 10, 'targetY' => 7]]
                : ['action' => ActionType::END_TURN, 'params' => []];
        });
        $o = new GameOrchestrator(new RulesEngine(), new FixedDiceRoller(array_merge([6, 6, 6], array_fill(0, 40, 3))), $ai);
        $q = $o->executeAction($s->withActiveTeam(TeamSide::AWAY), ActionType::END_TURN, [])['state'];

        $this->assertSame(TeamSide::HOME, $q->getActiveTeam(), 'tah AI stojí');
        $this->assertSame(12, $q->getPendingBombRethrow()?->getPlayerId(), 'kouč-člověk dostane otázku v tahu AI');

        $po = $o->executeAction($q, ActionType::RESOLVE_BOMB_RETHROW, ['targetX' => 14, 'targetY' => 7])['state'];
        $this->assertNull($po->getPendingBombRethrow());
        $this->assertSame(PlayerState::STANDING, $po->requirePlayer(1)->getState());
        $this->assertSame(TeamSide::AWAY, $po->getActiveTeam(), 'tah AI po odpovědi doběhl (END_TURN)');
    }
}
