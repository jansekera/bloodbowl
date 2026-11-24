<?php

declare(strict_types=1);

namespace App\Tests\Engine;

use App\DTO\ActionResult;
use App\DTO\GameState;
use App\DTO\TeamStateDTO;
use App\Engine\ActionResolver;
use App\Engine\FixedDiceRoller;
use App\Engine\PilingOn;
use App\Engine\RulesEngine;
use App\Enum\ActionType;
use App\Enum\PlayerState;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use PHPUnit\Framework\TestCase;

/**
 * P310 (uživatel 24.11.2026: „1 vše, 2 ano, 3 ano“ + „piling on dává našeho hráče na zem - cíl pro faul“) — Piling On,
 * `rules_bb2016.txt` ř. 8361-8371: "The player may use this skill after they have made a block as part a Block or
 * Blitz action, but only if they are currently standing adjacent to the victim and the victim was Knocked Down. You
 * can use a team re-roll to re-roll the Armour roll or Injury roll. Then the Piling On player is Placed Prone in their
 * own square - no Armour roll is made for them, and this does not cause a turnover unless the Piling On player is
 * carrying the ball. ... If a player with the Loner skill wishes to use Piling On, roll a D6. On a 1-3, the team
 * re-roll is spent, but they remain standing and cannot re-roll the Armour or Injury roll."
 *
 * Dřív PHP Piling On použil vždy, bez přehozu, bez Loneru a sousedství a házel znovu brnění i zranění. Parita s C++
 * `engine/tests/test_p310_piling_on.cpp`. Kostka bloku: 1 lebka · 2 Both Down · 3-4 Pushed · 5 Stumbles · 6 Down.
 */
final class PilingOnTest extends TestCase
{
    /**
     * Útočník HOME (5,5) s Block + Piling On vedle obránce AWAY (6,5); míč drží soupeř daleko (AI smí přehoz utratit).
     *
     * @param list<SkillName> $navic
     */
    private function stav(int $prehozy = 1, array $navic = [], int $avUtocnika = 8, bool $prehozPouzit = false): GameState
    {
        $home = TeamStateDTO::create(1, 'Home', 'Human', TeamSide::HOME, $prehozy);
        if ($prehozPouzit) {
            $home = $home->withRerollUsed();
        }

        return (new GameStateBuilder())
            ->withHomeTeam($home)
            ->addPlayer(TeamSide::HOME, 5, 5, armour: $avUtocnika, skills: [SkillName::Block, SkillName::PilingOn, ...$navic], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, id: 2)
            ->addPlayer(TeamSide::AWAY, 20, 2, id: 3)
            ->withBallCarried(3)
            ->build();
    }

    /** @param list<int> $kostky */
    private function ai(GameState $s, array $kostky): ActionResult
    {
        return (new ActionResolver(new FixedDiceRoller($kostky)))->resolve($s, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);
    }

    /** @param list<int> $kostky */
    private function clovek(array $kostky): ActionResolver
    {
        $r = new ActionResolver(new FixedDiceRoller($kostky));
        $r->setInteractiveBlocks(true);
        $r->setHumanSides([TeamSide::HOME]);

        return $r;
    }

    /**
     * Kouč-člověk zablokuje a vybere první kostku; `$kostky` = kostka bloku, pak hody po výběru.
     *
     * @param list<int> $kostky
     */
    private function clovekBlok(GameState $s, array $kostky): ActionResult
    {
        $hod = $this->clovek([$kostky[0]])->resolve($s, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);
        $kostky = array_slice($kostky, 1);
        $this->assertNotNull($hod->getNewState()->getPendingBlock(), 'fixtura: čeká volba kostky');

        return $this->clovek($kostky)->resolve($hod->getNewState(), ActionType::CHOOSE_BLOCK_DIE, ['faceIndex' => 0]);
    }

    /** @return list<string> */
    private function typy(ActionResult $r): array
    {
        return array_map(static fn($e): string => $e->getType(), $r->getEvents());
    }

    // ── kouč-člověk (otázka 3: „ano“) ──

    public function testClovekBrneniVydrzeloOtazka(): void
    {
        $s = $this->clovekBlok($this->stav(), [2, 1, 1])->getNewState();   // Both Down, brnění 1+1 drží

        $q = $s->getPendingPilingOn();
        $this->assertNotNull($q);
        $this->assertSame(1, $q->getPlayerId());
        $this->assertSame(2, $q->getDefenderId());
        $this->assertTrue($q->isArmour());
        $this->assertSame([ActionType::RESOLVE_PILING_ON->value], array_column((new RulesEngine())->getAvailableActions($s), 'type'));
        $this->assertNotSame([], (new RulesEngine())->validate($s, ActionType::END_TURN, []), 'bez odpovědi nic jiného');
        $this->assertNotSame([], (new RulesEngine())->validate($s, ActionType::RESOLVE_PILING_ON, ['use' => 1]), 'use musí být bool');
    }

    public function testClovekAnoPrehodiBrneniALehne(): void
    {
        $q = $this->clovekBlok($this->stav(), [2, 1, 1])->getNewState();
        $r = $this->clovek([6, 6, 4, 4])->resolve($q, ActionType::RESOLVE_PILING_ON, ['use' => true]);   // přehoz 12, KO
        $s = $r->getNewState();

        $this->assertNull($s->getPendingPilingOn());
        $this->assertFalse($r->isTurnover());
        $this->assertSame(PlayerState::KO, $s->requirePlayer(2)->getState());
        $this->assertSame(PlayerState::PRONE, $s->requirePlayer(1)->getState());
        $this->assertSame(0, $s->getTeamState(TeamSide::HOME)->getRerolls());
        $this->assertFalse($s->getTeamState(TeamSide::HOME)->canUseReroll());
        $this->assertContains('piling_on', $this->typy($r));
    }

    public function testClovekNeNicSeNeprehodi(): void
    {
        $q = $this->clovekBlok($this->stav(), [2, 1, 1])->getNewState();
        $s = $this->clovek([])->resolve($q, ActionType::RESOLVE_PILING_ON, ['use' => false])->getNewState();

        $this->assertSame(PlayerState::STANDING, $s->requirePlayer(1)->getState());
        $this->assertSame(PlayerState::PRONE, $s->requirePlayer(2)->getState());
        $this->assertSame(1, $s->getTeamState(TeamSide::HOME)->getRerolls());
    }

    public function testClovekStunnedPrehodiZraneni(): void
    {
        $q = $this->clovekBlok($this->stav(), [2, 6, 6, 1, 1])->getNewState();   // brnění padlo, zranění 2 = Stunned
        $this->assertFalse($q->getPendingPilingOn()?->isArmour() ?? true, 'otázka na zranění');
        $s = $this->clovek([4, 4])->resolve($q, ActionType::RESOLVE_PILING_ON, ['use' => true])->getNewState();

        $this->assertSame(PlayerState::KO, $s->requirePlayer(2)->getState());
        $this->assertSame(PlayerState::PRONE, $s->requirePlayer(1)->getState());
    }

    /** „re-roll the Armour roll OR Injury roll“: po přehozeném brnění se Stunned znovu nepřehazuje. */
    public function testJedenPrehozBrneniPakStunnedUzNe(): void
    {
        $q = $this->clovekBlok($this->stav(2), [2, 1, 1])->getNewState();
        $s = $this->clovek([6, 6, 1, 1])->resolve($q, ActionType::RESOLVE_PILING_ON, ['use' => true])->getNewState();

        $this->assertNull($s->getPendingPilingOn());
        $this->assertSame(PlayerState::STUNNED, $s->requirePlayer(2)->getState());
        $this->assertSame(1, $s->getTeamState(TeamSide::HOME)->getRerolls());
    }

    public function testBezPrehozuSeNepta(): void
    {
        $s = $this->clovekBlok($this->stav(0), [2, 1, 1])->getNewState();
        $this->assertNull($s->getPendingPilingOn());
        $this->assertSame(PlayerState::STANDING, $s->requirePlayer(1)->getState());
    }

    public function testPrehozVTahuUzPouzitSeNepta(): void
    {
        $s = $this->clovekBlok($this->stav(2, prehozPouzit: true), [2, 1, 1])->getNewState();
        $this->assertNull($s->getPendingPilingOn());
    }

    /** „standing adjacent to the victim“: Defender Down bez follow-upu ⇒ obránce o pole dál. */
    public function testBezFollowUpuNesousediNepta(): void
    {
        $hod = $this->clovek([6])->resolve($this->stav(), ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);
        $s = $this->clovek([1, 1])->resolve($hod->getNewState(), ActionType::CHOOSE_BLOCK_DIE, ['faceIndex' => 0, 'followUp' => false])->getNewState();

        $this->assertSame(7, $s->requirePlayer(2)->requirePosition()->getX());
        $this->assertNull($s->getPendingPilingOn());
        $this->assertSame(PlayerState::STANDING, $s->requirePlayer(1)->getState());
    }

    /** Loner 1-3: „the team re-roll is spent, but they remain standing and cannot re-roll“. */
    public function testLonerNeuspechPrehozPropadneStojiDal(): void
    {
        $q = $this->clovekBlok($this->stav(navic: [SkillName::Loner]), [2, 1, 1])->getNewState();
        $s = $this->clovek([3])->resolve($q, ActionType::RESOLVE_PILING_ON, ['use' => true])->getNewState();

        $this->assertSame(0, $s->getTeamState(TeamSide::HOME)->getRerolls());
        $this->assertSame(PlayerState::STANDING, $s->requirePlayer(1)->getState());
        $this->assertSame(PlayerState::PRONE, $s->requirePlayer(2)->getState());
    }

    /** „does not cause a turnover unless the Piling On player is carrying the ball“. */
    public function testNosicLehneTurnover(): void
    {
        $s0 = $this->stav();
        $s0 = $s0->withBall(\App\DTO\BallState::carried($s0->requirePlayer(1)->requirePosition(), 1));
        $q = $this->clovekBlok($s0, [2, 1, 1])->getNewState();
        $r = $this->clovek([1, 1, 1, 1, 1, 1])->resolve($q, ActionType::RESOLVE_PILING_ON, ['use' => true]);

        $this->assertTrue($r->isTurnover());
        $this->assertSame(PlayerState::PRONE, $r->getNewState()->requirePlayer(1)->getState());
        $this->assertNotSame(1, $r->getNewState()->getBall()->getCarrierId());
    }

    // ── AI ──

    public function testAiStunnedPrehodiZraneni(): void
    {
        $s = $this->ai($this->stav(), [2, 6, 6, 1, 1, 4, 4])->getNewState();
        $this->assertSame(PlayerState::KO, $s->requirePlayer(2)->getState());
        $this->assertSame(PlayerState::PRONE, $s->requirePlayer(1)->getState());
    }

    /** Otázka 2: dokud náš nosič nedohrál, přehoz se šetří. */
    public function testAiNasNosicJesteHrajeSetri(): void
    {
        $s0 = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, skills: [SkillName::Block, SkillName::PilingOn], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, id: 2)
            ->addPlayer(TeamSide::HOME, 2, 2, id: 4)
            ->withBallCarried(4)
            ->build();
        $s = $this->ai($s0, [2, 6, 6, 1, 1])->getNewState();
        $this->assertSame(PlayerState::STUNNED, $s->requirePlayer(2)->getState());
        $this->assertSame(PlayerState::STANDING, $s->requirePlayer(1)->getState());

        $dohral = $s0->withPlayer($s0->requirePlayer(4)->withHasActed(true));
        $s = $this->ai($dohral, [2, 6, 6, 1, 1, 4, 4])->getNewState();
        $this->assertSame(PlayerState::KO, $s->requirePlayer(2)->getState(), 'nosič dohrál ⇒ přehoz smí jít');
    }

    public function testAiVolnyMicSetri(): void
    {
        $s0 = $this->stav()->withBall(\App\DTO\BallState::onGround(new \App\ValueObject\Position(3, 3)));
        $s = $this->ai($s0, [2, 6, 6, 1, 1])->getNewState();
        $this->assertSame(PlayerState::STANDING, $s->requirePlayer(1)->getState());
    }

    /** (a) brnění vydrželo: AV9 bez soupeřů vedle přehodí (zisk 0,116 > faul 0,069) … */
    public function testAiBrneniBezHrozbyFaulu(): void
    {
        $s = $this->ai($this->stav(avUtocnika: 9), [2, 1, 1, 6, 6, 4, 4])->getNewState();
        $this->assertSame(PlayerState::KO, $s->requirePlayer(2)->getState());
        $this->assertSame(PlayerState::PRONE, $s->requirePlayer(1)->getState());
    }

    /** … se dvěma stojícími soupeři vedle (asistence k faulu, 0,174) ne — „cíl pro faul“. */
    public function testAiBrneniDvaAsistentiFauluNe(): void
    {
        $s0 = $this->stav(avUtocnika: 9);
        $b = (new GameStateBuilder())->addPlayer(TeamSide::AWAY, 4, 4, id: 5)->addPlayer(TeamSide::AWAY, 4, 6, id: 6)->build();
        $s0 = $s0->withPlayer($b->requirePlayer(5))->withPlayer($b->requirePlayer(6));
        $r = $this->ai($s0, [2, 2, 1, 1]);   // dva asistenti obránce ⇒ dvě kostky bloku (obě Both Down)
        $s = $r->getNewState();

        $this->assertSame(PlayerState::STANDING, $s->requirePlayer(1)->getState());
        $this->assertSame(1, $s->getTeamState(TeamSide::HOME)->getRerolls());
    }

    public function testSanceZraneniStuntyAThickSkull(): void
    {
        $p = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 1, 1, id: 1)
            ->addPlayer(TeamSide::HOME, 1, 2, skills: [SkillName::Stunty], id: 2)
            ->addPlayer(TeamSide::HOME, 1, 3, skills: [SkillName::ThickSkull], id: 3)
            ->build();
        $this->assertEqualsWithDelta(15 / 36, PilingOn::zraneniVen($p->requirePlayer(1), 0), 1e-9);
        $this->assertEqualsWithDelta(21 / 36, PilingOn::zraneniVen($p->requirePlayer(2), 0), 1e-9);
        $this->assertEqualsWithDelta(10 / 36, PilingOn::zraneniVen($p->requirePlayer(3), 0), 1e-9);
    }
}
