<?php

declare(strict_types=1);

namespace App\Tests\Engine;

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
 * P308 (uživatel 20.11.2026: „ano“): AI blokuje hráče kouče-člověka a kostku vybírá OBRÁNCE ⇒ web se zeptá člověka.
 *
 * `rules_bb2016.txt` ř. 628-634: "One Player Stronger -- Two Block Dice* ... * The coach of the stronger player
 * picks which block dice is used."
 *
 * Stará mechanika: v tahu AI se blok vyhodnotil hned (`autoResolvePendingBlock`) a kostku za člověka-obránce
 * zvolil engine (`autoChooseBlockDie`, nejhorší pro útočníka). Nová: akce se přeruší otázkou kouči (`Coach`)
 * a po odpovědi se přehraje se stejnými kostkami (`ReplayContext`).
 *
 * Fixtura: AI = HOME id 1 (ST 3) blokuje AWAY id 2 (ST 4, kouč-člověk) ⇒ 2 kostky, vybírá obránce.
 * Kostky bloku 6 = Defender Down (index 0), 1 = Attacker Down (index 1). Engine by za obránce vzal Attacker Down;
 * kouč tu volí Defender Down (index 0) — jen tak se odliší, kdo kostku vybral.
 */
final class BlockDieObranceKoucTest extends TestCase
{
    private function stav(int $silaObrance = 4): GameState
    {
        return (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 3, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, strength: $silaObrance, armour: 8, id: 2)
            ->withBallOffPitch()
            ->build()
            ->withAiTeam(TeamSide::HOME);
    }

    /** Resolver tahu AI: bloky neinteraktivní (`setInteractiveBlocks` jen na tahu člověka), člověk = AWAY. */
    private function resolverTahuAi(FixedDiceRoller $dice, bool $clovekAway = true): ActionResolver
    {
        $r = new ActionResolver($dice);
        $r->setHumanSides($clovekAway ? [TeamSide::AWAY] : []);

        return $r;
    }

    public function testAiBlokujeObranceClovekaKteryVybiraZeptaSe(): void
    {
        $r = $this->resolverTahuAi(new FixedDiceRoller([6, 1]))
            ->resolve($this->stav(), ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);

        $q = $r->getNewState()->getPendingBlockDie();
        $this->assertNotNull($q, 'ř. 633-634: kostku vybírá kouč silnějšího hráče — člověk-obránce, ne engine');
        $this->assertSame(1, $q->getAttackerId());
        $this->assertSame(2, $q->getDefenderId());
        $this->assertFalse($q->isAttackerChooses());
        $this->assertSame(['defender_down', 'attacker_down'], array_map(fn($f) => $f->value, $q->getFaces()));
        $this->assertTrue($r->getNewState()->hasPendingReplayQuestion(), 'tah AI stojí, dokud kouč neodpoví');
        $this->assertFalse($r->isTurnover());
        $this->assertSame(PlayerState::STANDING, $r->getNewState()->requirePlayer(1)->getState(), 'do odpovědi se nic nevyhodnotilo');
        $this->assertSame(PlayerState::STANDING, $r->getNewState()->requirePlayer(2)->getState());
    }

    public function testOdpovedKouceRozhodneOKostce(): void
    {
        // 6, 1 = kostky bloku; 1, 1 = brnění sraženého obránce (AV8 drží)
        $dice = new FixedDiceRoller([6, 1, 1, 1]);
        $resolver = $this->resolverTahuAi($dice);
        $r = $resolver->resolve($this->stav(), ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);

        $this->assertSame([], (new RulesEngine())->validate($r->getNewState(), ActionType::RESOLVE_BLOCK_DIE, ['faceIndex' => 0]));
        $r = $resolver->resolve($r->getNewState(), ActionType::RESOLVE_BLOCK_DIE, ['faceIndex' => 0]);

        $this->assertNull($r->getNewState()->getPendingBlockDie());
        $this->assertNull($r->getNewState()->getPendingBlock());
        $this->assertSame(PlayerState::PRONE, $r->getNewState()->requirePlayer(2)->getState(), 'kouč zvolil Defender Down');
        $this->assertSame(PlayerState::STANDING, $r->getNewState()->requirePlayer(1)->getState());
        $this->assertFalse($r->isTurnover());
        $this->assertFalse($dice->hasRemainingRolls(), 'přehrání použilo tytéž kostky bloku, nehodilo nové');
    }

    public function testDoOdpovediNicJinehoANeplatnaKostka(): void
    {
        $r = $this->resolverTahuAi(new FixedDiceRoller([6, 1]))
            ->resolve($this->stav(), ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);
        $stav = $r->getNewState();
        $rules = new RulesEngine();

        $this->assertNotSame([], $rules->validate($stav, ActionType::END_TURN, []));
        $this->assertNotSame([], $rules->validate($stav, ActionType::CHOOSE_BLOCK_DIE, ['faceIndex' => 0]));
        $this->assertNotSame([], $rules->validate($stav, ActionType::RESOLVE_BLOCK_DIE, ['faceIndex' => 2]), 'padly jen 2 kostky');
        $this->assertNotSame([], $rules->validate($stav, ActionType::RESOLVE_BLOCK_DIE, []));
        $this->assertSame([['type' => ActionType::RESOLVE_BLOCK_DIE->value]], $rules->getAvailableActions($stav));
        $this->assertNotSame([], $rules->validate($this->stav(), ActionType::RESOLVE_BLOCK_DIE, ['faceIndex' => 0]), 'bez otázky odpověď nejde');
    }

    public function testObranceAiVybiraDalSam(): void
    {
        // AI × AI (simulace): nikdo se neptá, obránce si vezme Attacker Down jako dřív
        $r = $this->resolverTahuAi(new FixedDiceRoller([6, 1, 1, 1]), clovekAway: false)
            ->resolve($this->stav(), ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);

        $this->assertNull($r->getNewState()->getPendingBlockDie());
        $this->assertTrue($r->isTurnover());
        $this->assertSame(PlayerState::PRONE, $r->getNewState()->requirePlayer(1)->getState());
    }

    public function testSilnejsiUtocnikAiVybiraSamIProtiCloveku(): void
    {
        // ST 3 proti ST 2 ⇒ vybírá útočník (AI): člověka se neptá, AI vezme Defender Down
        $r = $this->resolverTahuAi(new FixedDiceRoller([6, 1, 1, 1]))
            ->resolve($this->stav(silaObrance: 2), ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);

        $this->assertNull($r->getNewState()->getPendingBlockDie());
        $this->assertSame(PlayerState::PRONE, $r->getNewState()->requirePlayer(2)->getState());
    }

    public function testMultipleBlockAiSeZeptaNaKazdyBlok(): void
    {
        // Multiple Block: obránci +2 ST (ř. 8300-8301) ⇒ ST 4 proti 3+1 asistence+2 = 6, vybírá obránce u obou ran
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 4, skills: [SkillName::MultipleBlock, SkillName::Block], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, strength: 3, armour: 9, id: 2)
            ->addPlayer(TeamSide::AWAY, 5, 6, strength: 3, armour: 9, id: 3)
            ->withBallOffPitch()
            ->build()
            ->withAiTeam(TeamSide::HOME);
        // 6, 1 = kostky 1. rány; 1, 1 = brnění hráče 2; 6, 1 = kostky 2. rány; 1, 1 = brnění hráče 3
        $resolver = $this->resolverTahuAi(new FixedDiceRoller([6, 1, 1, 1, 6, 1, 1, 1]));
        $r = $resolver->resolve($state, ActionType::MULTIPLE_BLOCK, ['playerId' => 1, 'targetId' => 2, 'targetId2' => 3]);

        $this->assertSame(2, $r->getNewState()->getPendingBlockDie()?->getDefenderId(), 'ř. 633-634: u 1. rány volí člověk');
        $r = $resolver->resolve($r->getNewState(), ActionType::RESOLVE_BLOCK_DIE, ['faceIndex' => 0]);

        $this->assertSame(3, $r->getNewState()->getPendingBlockDie()?->getDefenderId(), 'u 2. rány znovu člověk');
        $this->assertSame(PlayerState::PRONE, $r->getNewState()->requirePlayer(2)->getState(), 'stav k zobrazení: 1. rána už padla');

        // 2. odpověď: Defender Down i u hráče 3 (brnění 1+1 drží); útočník stojí, akce skončila
        $r = $resolver->resolve($r->getNewState(), ActionType::RESOLVE_BLOCK_DIE, ['faceIndex' => 0]);
        $this->assertNull($r->getNewState()->getPendingBlockDie());
        $this->assertSame(PlayerState::PRONE, $r->getNewState()->requirePlayer(2)->getState());
        $this->assertSame(PlayerState::PRONE, $r->getNewState()->requirePlayer(3)->getState());
        $this->assertSame(PlayerState::STANDING, $r->getNewState()->requirePlayer(1)->getState());
        $this->assertTrue($r->getNewState()->requirePlayer(1)->hasActed());
        $this->assertFalse($r->isTurnover());
    }

    /**
     * @param list<\App\DTO\GameEvent> $udalosti
     * @return list<string>
     */
    private function typy(array $udalosti): array
    {
        return array_map(static fn($e): string => $e->getType(), $udalosti);
    }

    public function testFrenzyDruhaRanaSeZeptaZnovuAPrvniRanaSeNeopakuje(): void
    {
        // ST 3 proti 4 ⇒ 2 kostky, vybírá obránce: 1. rána 3, 4 = Pushed, Pushed; 2. rána 6, 1; brnění 1+1
        $s = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 3, skills: [SkillName::Frenzy], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, strength: 4, armour: 9, id: 2)
            ->withBallOffPitch()->build()->withAiTeam(TeamSide::HOME);
        $resolver = $this->resolverTahuAi(new FixedDiceRoller([3, 4, 6, 1, 1, 1, 1, 1, 1, 1]));

        $a = $resolver->resolve($s, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);
        $this->assertFalse($a->getNewState()->getPendingBlockDie()?->isFrenzy(), '1. rána');
        $b = $resolver->resolve($a->getNewState(), ActionType::RESOLVE_BLOCK_DIE, ['faceIndex' => 0]);
        $q2 = $b->getNewState()->getPendingBlockDie();
        $this->assertTrue($q2?->isFrenzy(), 'druhá rána Frenzy: ř. 633-634 platí i pro ni');
        $this->assertSame(['defender_down', 'attacker_down'], array_map(fn($f) => $f->value, $q2->getFaces()));
        $this->assertContains('frenzy', $this->typy($b->getEvents()));

        $c = $resolver->resolve($b->getNewState(), ActionType::RESOLVE_BLOCK_DIE, ['faceIndex' => 0]);
        $this->assertNull($c->getNewState()->getPendingBlockDie());
        $this->assertSame(PlayerState::PRONE, $c->getNewState()->requirePlayer(2)->getState());
        $this->assertNotContains('frenzy', $this->typy($c->getEvents()), 'události 1. rány kouč už viděl');
        $this->assertCount(1, array_keys($this->typy($c->getEvents()), 'block'), 'jen 2. rána');
    }

    public function testProPrehodiDrivNezSeObranceZeptá(): void
    {
        // 1, 1 = Attacker Down ×2 ⇒ AI-útočník zkusí Pro (4 = povedl), nové kostky 6, 1 ⇒ pak teprve otázka
        $s = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 3, skills: [SkillName::Pro], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, strength: 4, armour: 9, id: 2)
            ->withBallOffPitch()->build()->withAiTeam(TeamSide::HOME);
        $dice = new FixedDiceRoller([1, 1, 4, 6, 1, 1, 1]);
        $resolver = $this->resolverTahuAi($dice);

        $a = $resolver->resolve($s, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);
        $this->assertSame(['defender_down', 'attacker_down'], array_map(fn($f) => $f->value, $a->getNewState()->getPendingBlockDie()?->getFaces() ?? []));
        $this->assertContains('pro', $this->typy($a->getEvents()));

        $b = $resolver->resolve($a->getNewState(), ActionType::RESOLVE_BLOCK_DIE, ['faceIndex' => 0]);
        $this->assertSame(PlayerState::PRONE, $b->getNewState()->requirePlayer(2)->getState());
        $this->assertNotContains('pro', $this->typy($b->getEvents()), 'Pro se po odpovědi neopakuje');
        $this->assertFalse($dice->hasRemainingRolls());
    }

    public function testOrchestratorTahAiStojiAPoOdpovediDobehne(): void
    {
        $s = (new GameStateBuilder())
            ->addPlayer(TeamSide::AWAY, 5, 5, strength: 3, id: 1)
            ->addPlayer(TeamSide::HOME, 6, 5, strength: 4, armour: 9, id: 2)
            ->withBallOffPitch()->withActiveTeam(TeamSide::HOME)->build()->withAiTeam(TeamSide::AWAY);
        $o = $this->orchestrator();

        $x = $o->executeAction($s, ActionType::END_TURN, []);
        $this->assertNotNull($x['state']->getPendingBlockDie(), 'tah AI stojí na otázce');
        $this->assertSame(TeamSide::AWAY, $x['state']->getActiveTeam());

        $y = $o->executeAction($x['state'], ActionType::RESOLVE_BLOCK_DIE, ['faceIndex' => 0]);
        $this->assertSame(PlayerState::PRONE, $y['state']->requirePlayer(2)->getState(), 'kouč zvolil Defender Down');
        $this->assertSame(TeamSide::HOME, $y['state']->getActiveTeam(), 'AI dohrála tah');
        $this->assertSame('end_turn', $this->typy($y['events'])[count($y['events']) - 1]);
    }

    /** AI (away): napřed BLOCK hráčem 1 na hráče 2, pak END_TURN. */
    private function orchestrator(): \App\Service\GameOrchestrator
    {
        $tahy = 0;
        $ai = $this->createStub(\App\AI\AICoachInterface::class);
        $ai->method('decideAction')->willReturnCallback(static function () use (&$tahy): array {
            return $tahy++ === 0
                ? ['action' => ActionType::BLOCK, 'params' => ['playerId' => 1, 'targetId' => 2]]
                : ['action' => ActionType::END_TURN, 'params' => []];
        });

        return new \App\Service\GameOrchestrator(new RulesEngine(), new FixedDiceRoller(array_merge([6, 1, 1, 1], array_fill(0, 40, 3))), $ai);
    }

    public function testOdpovedKoucePisLogJakoStranuCloveka(): void
    {
        // Review P308 (8): odpověď kouče v tahu AI je rozhodnutí člověka (home), ne AI (away)
        $s = (new GameStateBuilder())
            ->addPlayer(TeamSide::AWAY, 5, 5, strength: 3, id: 1)
            ->addPlayer(TeamSide::HOME, 6, 5, strength: 4, armour: 9, id: 2)
            ->withBallOffPitch()->withActiveTeam(TeamSide::HOME)->build()->withAiTeam(TeamSide::AWAY);
        $o = $this->orchestrator();
        $x = $o->executeAction($s, ActionType::END_TURN, []);

        $log = tempnam(sys_get_temp_dir(), 'p308log');
        $o->setGameLogger(new \App\AI\GameLogger($log));
        $o->executeAction($x['state'], ActionType::RESOLVE_BLOCK_DIE, ['faceIndex' => 0]);
        $prvni = json_decode((string) strtok((string) file_get_contents($log), "\n"), true);
        unlink($log);

        $this->assertSame('state', $prvni['type'] ?? null);
        $this->assertSame('home', $prvni['perspective'] ?? null, 'rozhodl kouč-člověk');
    }

    public function testMultipleBlockClovekaUtocnikaKdeVybiraSeZepta(): void
    {
        // P319: ř. 633-634 — silnější útočník-člověk (ST 6 proti 3+2 = 5) volí kostku sám; dřív za něj volil engine.
        //   Kostky 1, 6: engine by vzal Defender Down (index 1), člověk volí Attacker Down (index 0).
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 6, skills: [SkillName::MultipleBlock], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, strength: 3, armour: 9, id: 2)
            ->addPlayer(TeamSide::AWAY, 4, 5, strength: 3, armour: 9, id: 3)
            ->withBallOffPitch()
            ->build()
            ->withAiTeam(TeamSide::AWAY);
        $resolver = new ActionResolver(new FixedDiceRoller([1, 6, 1, 1]));
        $resolver->setInteractiveBlocks(true);
        $resolver->setHumanSides([TeamSide::HOME]);

        $r = $resolver->resolve($state, ActionType::MULTIPLE_BLOCK, ['playerId' => 1, 'targetId' => 2, 'targetId2' => 3]);
        $q = $r->getNewState()->getPendingBlockDie();
        $this->assertNotNull($q, 'ř. 633-634: volí kouč silnějšího = člověk-útočník');
        $this->assertSame(2, $q->getDefenderId());
        $this->assertTrue($q->isAttackerChooses(), 'web ukáže, kdo volí');

        $r = $resolver->resolve($r->getNewState(), ActionType::RESOLVE_BLOCK_DIE, ['faceIndex' => 0]);
        $this->assertTrue($r->isTurnover(), 'zvolil Attacker Down');
        $this->assertSame(PlayerState::PRONE, $r->getNewState()->requirePlayer(1)->getState());
        $this->assertSame(PlayerState::STANDING, $r->getNewState()->requirePlayer(2)->getState());
    }

    public function testBallAndChainAiProtiSilnejsimuClovekuSeZepta(): void
    {
        // P320: B&C blokuje „following normal blocking rules“ (ř. 7819-7822) ⇒ ř. 633-634: silnější obránce-člověk volí.
        //   D6 3 = pohyb na (6,7); kostky 6, 1: engine by za obránce vzal Attacker Down, člověk volí Defender Down.
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, movement: 1, strength: 2, id: 1, skills: [SkillName::BallAndChain])
            ->addPlayer(TeamSide::AWAY, 6, 7, strength: 4, armour: 9, id: 2)
            ->withBallOffPitch()
            ->build()
            ->withAiTeam(TeamSide::HOME);
        $resolver = $this->resolverTahuAi(new FixedDiceRoller([3, 6, 1, 1, 1]));

        $r = $resolver->resolve($state, ActionType::BALL_AND_CHAIN, ['playerId' => 1]);
        $q = $r->getNewState()->getPendingBlockDie();
        $this->assertNotNull($q, 'ř. 633-634 + 7819-7822');
        $this->assertSame(['defender_down', 'attacker_down'], array_map(fn($f) => $f->value, $q->getFaces()));

        $r = $resolver->resolve($r->getNewState(), ActionType::RESOLVE_BLOCK_DIE, ['faceIndex' => 0]);
        $this->assertSame(PlayerState::PRONE, $r->getNewState()->requirePlayer(2)->getState());
        $this->assertSame(PlayerState::STANDING, $r->getNewState()->requirePlayer(1)->getState());
    }

    public function testOtazkaPrezijeUlozeniStavuAKlientNedostanePrehrani(): void
    {
        $r = $this->resolverTahuAi(new FixedDiceRoller([6, 1]))
            ->resolve($this->stav(), ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);

        $obnoveny = GameState::fromArray($r->getNewState()->toArray());
        $this->assertSame(
            $r->getNewState()->getPendingBlockDie()?->toArray(),
            $obnoveny->getPendingBlockDie()?->toArray(),
        );
        $klient = $r->getNewState()->toClientArray()['pendingBlockDie'];
        $this->assertSame(['defender_down', 'attacker_down'], $klient['faces']);
        $this->assertArrayNotHasKey('replay', $klient);
    }
}
