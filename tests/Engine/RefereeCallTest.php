<?php

declare(strict_types=1);

namespace App\Tests\Engine;

use App\DTO\TeamStateDTO;
use App\Engine\ActionResolver;
use App\Engine\BallResolver;
use App\Engine\InjuryResolver;
use App\Engine\ScatterCalculator;
use App\Engine\TacklezoneCalculator;
use App\Engine\FixedDiceRoller;
use App\Engine\GameFlowResolver;
use App\Engine\KickoffResolver;
use App\Enum\ActionType;
use App\Enum\PlayerState;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use PHPUnit\Framework\TestCase;

/**
 * P287 (audit goblinů 10.11.2026) — úplatky (ř. 2871-2879), Argue the call (ř. 1091-1102), Get the Ref (ř. 1270-1282).
 * P290 — Cheering Fans / Brilliant Coaching D3 a při shodě přehoz oběma (ř. 1309-1326).
 * Parita s C++ helpers.cpp refereeSendOff a kickoff_handler.cpp.
 */
final class RefereeCallTest extends TestCase
{
    private function kickoff(FixedDiceRoller $dice): KickoffResolver
    {
        $scatter = new ScatterCalculator();

        return new KickoffResolver($dice, $scatter, new BallResolver($dice, new TacklezoneCalculator(), $scatter), new InjuryResolver());
    }

    private function foulState(int $bribes, bool $coachEjected = false): \App\DTO\GameState
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

    public function testUplatekZrusiVylouceniIZaFaulITurnover(): void
    {
        // brnění 3+3 dublet, drží; úplatek 4 ⇒ účinný
        $dice = new FixedDiceRoller([3, 3, 4]);
        $r = (new ActionResolver($dice))->resolve($this->foulState(1), ActionType::FOUL, ['playerId' => 1, 'targetId' => 2]);

        $this->assertFalse($r->isTurnover());
        $this->assertSame(PlayerState::STANDING, $r->getNewState()->requirePlayer(1)->getState());
        $this->assertSame(0, $r->getNewState()->getHomeTeam()->getBribes());
        $this->assertFalse($dice->hasRemainingRolls());
    }

    public function testNeuspesnyUplatekPakProtest6PosleDoRezervSTurnoverem(): void
    {
        // dublet; úplatek 1 = propadl; protest 6 ⇒ do rezerv, turnover zůstává (ř. 1100-1101)
        $dice = new FixedDiceRoller([3, 3, 1, 6]);
        $r = (new ActionResolver($dice))->resolve($this->foulState(1), ActionType::FOUL, ['playerId' => 1, 'targetId' => 2]);

        $this->assertTrue($r->isTurnover());
        $this->assertSame(PlayerState::OFF_PITCH, $r->getNewState()->requirePlayer(1)->getState());
        $this->assertFalse($dice->hasRemainingRolls());
    }

    public function testProtestNaJednickuVyhodiTreneraAUzNeprotestuje(): void
    {
        $dice = new FixedDiceRoller([3, 3, 1]);
        $r = (new ActionResolver($dice))->resolve($this->foulState(0), ActionType::FOUL, ['playerId' => 1, 'targetId' => 2]);

        $this->assertSame(PlayerState::EJECTED, $r->getNewState()->requirePlayer(1)->getState());
        $this->assertTrue($r->getNewState()->getHomeTeam()->isHeadCoachEjected());

        // vyhozený trenér: žádný další hod na protest
        $dice2 = new FixedDiceRoller([3, 3]);
        $r2 = (new ActionResolver($dice2))->resolve($this->foulState(0, true), ActionType::FOUL, ['playerId' => 1, 'targetId' => 2]);
        $this->assertSame(PlayerState::EJECTED, $r2->getNewState()->requirePlayer(1)->getState());
        $this->assertFalse($dice2->hasRemainingRolls());
    }

    public function testTajnaZbranNaKonciDriveNaprvProtestPakUplatek(): void
    {
        $home = TeamStateDTO::create(1, 'Home', 'Goblin', TeamSide::HOME, 0, bribes: 1);
        $s = (new GameStateBuilder())
            ->withHomeTeam($home)
            ->addPlayer(TeamSide::HOME, 5, 5, id: 1)
            ->addPlayer(TeamSide::AWAY, 20, 7, id: 2)
            ->addPlayer(TeamSide::HOME, 6, 6, skills: [SkillName::SecretWeapon], id: 3)
            ->build();
        // protest 3 (nic), úplatek 5 ⇒ zůstává; pak případné další hody po drivu
        $r = (new GameFlowResolver(new FixedDiceRoller([3, 5, 5, 5, 5, 5])))->resolvePostTouchdown($s);

        $this->assertNotSame(PlayerState::EJECTED, $r['state']->requirePlayer(3)->getState());
        $this->assertSame(0, $r['state']->getHomeTeam()->getBribes());
    }

    public function testGetTheRefDaKazdemuTymuUplatek(): void
    {
        $s = (new GameStateBuilder())->addPlayer(TeamSide::HOME, 5, 5, id: 1)->addPlayer(TeamSide::AWAY, 20, 7, id: 2)->build();
        // 1+1 = 2 = Get the Ref
        $r = $this->kickoff(new FixedDiceRoller([1, 1]))->resolveKickoffTable($s, TeamSide::HOME);

        $this->assertSame(1, $r['state']->getHomeTeam()->getBribes());
        $this->assertSame(1, $r['state']->getAwayTeam()->getBribes());
    }

    public function testP290CheeringFansPriShodeDostanouPrehozOba(): void
    {
        $s = (new GameStateBuilder())->addPlayer(TeamSide::HOME, 5, 5, id: 1)->addPlayer(TeamSide::AWAY, 20, 7, id: 2)->build();
        $homeBefore = $s->getHomeTeam()->getRerolls();
        $awayBefore = $s->getAwayTeam()->getRerolls();
        // 2+4 = 6 Cheering; D3 z D6: 5 ⇒ 3, 6 ⇒ 3 — shoda
        $r = $this->kickoff(new FixedDiceRoller([2, 4, 5, 6]))->resolveKickoffTable($s, TeamSide::HOME);

        $this->assertSame($homeBefore + 1, $r['state']->getHomeTeam()->getRerolls());
        $this->assertSame($awayBefore + 1, $r['state']->getAwayTeam()->getRerolls());
    }

    public function testBrilliantCoachingVyhozenyTrenerMinusJedna(): void
    {
        $home = TeamStateDTO::create(1, 'Home', 'Goblin', TeamSide::HOME, 2)->withHeadCoachEjected();
        $s = (new GameStateBuilder())->withHomeTeam($home)->addPlayer(TeamSide::HOME, 5, 5, id: 1)->addPlayer(TeamSide::AWAY, 20, 7, id: 2)->build();
        $awayBefore = $s->getAwayTeam()->getRerolls();
        // 4+4 = 8 Brilliant; obě D3 = 3, domácí −1 ⇒ hosté vyhrají sami
        $r = $this->kickoff(new FixedDiceRoller([4, 4, 6, 6]))->resolveKickoffTable($s, TeamSide::HOME);

        $this->assertSame(2, $r['state']->getHomeTeam()->getRerolls());
        $this->assertSame($awayBefore + 1, $r['state']->getAwayTeam()->getRerolls());
    }
}
