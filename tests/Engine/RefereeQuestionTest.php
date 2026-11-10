<?php

declare(strict_types=1);

namespace App\Tests\Engine;

use App\DTO\GameState;
use App\DTO\TeamStateDTO;
use App\Engine\ActionResolver;
use App\Engine\FixedDiceRoller;
use App\Engine\GameFlowResolver;
use App\Engine\RulesEngine;
use App\Enum\ActionType;
use App\Enum\PlayerState;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use PHPUnit\Framework\TestCase;

/**
 * P291 (uživatel 10.11.2026: „udělej ať se zeptá — člověk může některého hráče preferovat, ať zůstane“).
 * Úplatek (`rules_bb2016.txt` ř. 2871-2879 „may use a bribe“) a Argue the call (ř. 1091-1102 „may argue“) jsou
 * volby kouče. Kouče-člověka se rozhodčí zeptá: u faulu po dubletu (akce se přehraje se stejnými kostkami),
 * u tajné zbraně na konci drive před setupem. Ptá se, jen když je co volit. AI volí dál sama.
 */
final class RefereeQuestionTest extends TestCase
{
    private function faul(int $bribes, bool $coachEjected = false): GameState
    {
        $home = TeamStateDTO::create(1, 'Home', 'Goblin', TeamSide::HOME, 0, bribes: $bribes);
        if ($coachEjected) {
            $home = $home->withHeadCoachEjected();
        }

        return (new GameStateBuilder())
            ->withHomeTeam($home)
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1)
            ->addPronePlayer(TeamSide::AWAY, 6, 7, armour: 8, id: 2)
            ->build();
    }

    private function resolver(FixedDiceRoller $dice): ActionResolver
    {
        $r = new ActionResolver($dice);
        $r->setInteractiveReferee([TeamSide::HOME]);

        return $r;
    }

    public function testFaulSDubletemSeZeptaKouce(): void
    {
        $r = $this->resolver(new FixedDiceRoller([3, 3]))
            ->resolve($this->faul(1), ActionType::FOUL, ['playerId' => 1, 'targetId' => 2]);

        $q = $r->getNewState()->getPendingReferee();
        $this->assertNotNull($q);
        $this->assertSame(1, $q->getPlayerId());
        $this->assertTrue($q->isFoul());
        $this->assertTrue($q->canBribe());
        $this->assertTrue($q->canArgue());
        $this->assertFalse($r->isTurnover());
        $this->assertSame(PlayerState::STANDING, $r->getNewState()->requirePlayer(1)->getState());
    }

    public function testFaulOdpovedNicHracVyloucenUplatekZustane(): void
    {
        $resolver = $this->resolver(new FixedDiceRoller([3, 3]));
        $r = $resolver->resolve($this->faul(1), ActionType::FOUL, ['playerId' => 1, 'targetId' => 2]);
        $r = $resolver->resolve($r->getNewState(), ActionType::RESOLVE_REFEREE, ['bribe' => false, 'argue' => false]);

        $this->assertSame(PlayerState::EJECTED, $r->getNewState()->requirePlayer(1)->getState());
        $this->assertTrue($r->isTurnover());
        $this->assertSame(1, $r->getNewState()->getHomeTeam()->getBribes());
        $this->assertNull($r->getNewState()->getPendingReferee());
    }

    public function testFaulOdpovedUplatekHracZustaneBezTurnoveru(): void
    {
        $resolver = $this->resolver(new FixedDiceRoller([3, 3, 4]));
        $r = $resolver->resolve($this->faul(1), ActionType::FOUL, ['playerId' => 1, 'targetId' => 2]);
        $r = $resolver->resolve($r->getNewState(), ActionType::RESOLVE_REFEREE, ['bribe' => true, 'argue' => false]);

        $this->assertSame(PlayerState::STANDING, $r->getNewState()->requirePlayer(1)->getState());
        $this->assertFalse($r->isTurnover());
        $this->assertSame(0, $r->getNewState()->getHomeTeam()->getBribes());
    }

    public function testFaulOdpovedJenProtest6DoRezerv(): void
    {
        $dice = new FixedDiceRoller([3, 3, 6]);
        $resolver = $this->resolver($dice);
        $r = $resolver->resolve($this->faul(1), ActionType::FOUL, ['playerId' => 1, 'targetId' => 2]);
        $r = $resolver->resolve($r->getNewState(), ActionType::RESOLVE_REFEREE, ['bribe' => false, 'argue' => true]);

        $this->assertSame(PlayerState::OFF_PITCH, $r->getNewState()->requirePlayer(1)->getState());
        $this->assertSame(1, $r->getNewState()->getHomeTeam()->getBribes(), 'úplatek nepoužit');
        $this->assertFalse($dice->hasRemainingRolls());
    }

    public function testBezUplatkuAVyhozenymTreneremSeNepta(): void
    {
        $r = $this->resolver(new FixedDiceRoller([3, 3]))
            ->resolve($this->faul(0, true), ActionType::FOUL, ['playerId' => 1, 'targetId' => 2]);

        $this->assertNull($r->getNewState()->getPendingReferee());
        $this->assertSame(PlayerState::EJECTED, $r->getNewState()->requirePlayer(1)->getState());
    }

    public function testAiSeNeptaAUplatekPouzije(): void
    {
        $r = (new ActionResolver(new FixedDiceRoller([3, 3, 4])))
            ->resolve($this->faul(1), ActionType::FOUL, ['playerId' => 1, 'targetId' => 2]);

        $this->assertNull($r->getNewState()->getPendingReferee());
        $this->assertSame(0, $r->getNewState()->getHomeTeam()->getBribes());
    }

    private function konecDrive(int $bribes = 1): GameState
    {
        $home = TeamStateDTO::create(1, 'Home', 'Goblin', TeamSide::HOME, 0, bribes: $bribes);

        return (new GameStateBuilder())
            ->withHomeTeam($home)
            ->addPlayer(TeamSide::HOME, 5, 5, id: 1)
            ->addPlayer(TeamSide::AWAY, 20, 7, id: 2)
            ->addPlayer(TeamSide::HOME, 6, 6, skills: [SkillName::SecretWeapon], id: 3)
            ->addPlayer(TeamSide::HOME, 6, 8, skills: [SkillName::SecretWeapon], id: 4)
            ->build();
    }

    private function flow(FixedDiceRoller $dice): GameFlowResolver
    {
        $f = new GameFlowResolver($dice);
        $f->setInteractiveReferee([TeamSide::HOME]);

        return $f;
    }

    public function testTajnaZbranNaKonciDriveSeZeptaPredVyloucenim(): void
    {
        $r = $this->flow(new FixedDiceRoller(array_fill(0, 10, 5)))->resolvePostTouchdown($this->konecDrive());

        $q = $r['state']->getPendingReferee();
        $this->assertNotNull($q);
        $this->assertSame(3, $q->getPlayerId());
        $this->assertFalse($q->isFoul());
        $this->assertNotSame(PlayerState::EJECTED, $r['state']->requirePlayer(3)->getState());
        $this->assertNotSame(PlayerState::EJECTED, $r['state']->requirePlayer(4)->getState());
    }

    public function testTajnaZbranOdpovediPoJednomHraci(): void
    {
        $dice = new FixedDiceRoller(array_merge(array_fill(0, 10, 5)));
        $s = $this->flow($dice)->resolvePostTouchdown($this->konecDrive())['state'];

        // hráč 3: nic ⇒ vyloučen; pak otázka na hráče 4
        $resolver = $this->resolver(new FixedDiceRoller([6]));
        $r = $resolver->resolve($s, ActionType::RESOLVE_REFEREE, ['bribe' => false, 'argue' => false]);
        $this->assertSame(PlayerState::EJECTED, $r->getNewState()->requirePlayer(3)->getState());
        $this->assertSame(4, $r->getNewState()->getPendingReferee()?->getPlayerId());

        // hráč 4: protest 6 ⇒ do rezerv, zůstává
        $r = $resolver->resolve($r->getNewState(), ActionType::RESOLVE_REFEREE, ['bribe' => true, 'argue' => true]);
        $this->assertSame(PlayerState::OFF_PITCH, $r->getNewState()->requirePlayer(4)->getState());
        $this->assertSame(1, $r->getNewState()->getHomeTeam()->getBribes(), 'protest stačil, úplatek zůstal');
        $this->assertNull($r->getNewState()->getPendingReferee());
    }

    public function testCekajiciOtazkaBlokujeJineAkceAKontrolujeVolbu(): void
    {
        $r = $this->resolver(new FixedDiceRoller([3, 3]))
            ->resolve($this->faul(0), ActionType::FOUL, ['playerId' => 1, 'targetId' => 2]);
        $s = $r->getNewState();
        $rules = new RulesEngine();

        $this->assertNotSame([], $rules->validate($s, ActionType::END_TURN, []));
        $this->assertNotSame([], $rules->validate($s, ActionType::RESOLVE_REFEREE, ['bribe' => true, 'argue' => false]), 'úplatek není');
        $this->assertSame([], $rules->validate($s, ActionType::RESOLVE_REFEREE, ['bribe' => false, 'argue' => true]));
        $this->assertSame([['type' => ActionType::RESOLVE_REFEREE->value]], $rules->getAvailableActions($s));
    }

    public function testStavSOtazkouPrezijeSerializaci(): void
    {
        $r = $this->resolver(new FixedDiceRoller([3, 3]))
            ->resolve($this->faul(1), ActionType::FOUL, ['playerId' => 1, 'targetId' => 2]);
        $s = GameState::fromArray($r->getNewState()->toArray());

        $this->assertEquals($r->getNewState()->getPendingReferee(), $s->getPendingReferee());
    }
}
