<?php

declare(strict_types=1);

namespace App\Tests\Engine;

use App\Engine\ActionResolver;
use App\Engine\FixedDiceRoller;
use App\Enum\ActionType;
use App\Enum\PlayerState;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use PHPUnit\Framework\TestCase;

/**
 * P278, P279 (audit goblinů 10.11.2026) — dopad hozeného hráče v PHP, parita s C++ ttm_handler.cpp.
 *
 * `rules_bb2016.txt` ř. 8617-8622: „If the final square he scatters into is occupied by another player, treat the
 * player landed on as Knocked Down and roll for Armour (even if already Prone or Stunned), and then the player being
 * thrown will scatter one more square … he cannot land on more than one player.“ Right Stuff ř. 8421-8431: landing
 * roll „unless he landed on another player“; „If the landing roll is failed or he landed on another player he is
 * Placed Prone and must pass an Armour roll … does not cause a turnover, unless he was holding the ball.“
 */
final class TtmLandingRulesTest extends TestCase
{
    public function testP278HracNaKterehoDopadneJeSrazenAHozenyLezi(): void
    {
        // Troll (5,5) hází goblina (6,5) na (8,5); rozptyly 3, 7, 7 ⇒ (7,5), kde stojí soupeř 3.
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 5, skills: [SkillName::ThrowTeamMate], id: 1)
            ->addPlayer(TeamSide::HOME, 6, 5, skills: [SkillName::RightStuff], id: 2)
            ->addPlayer(TeamSide::AWAY, 7, 5, id: 3)
            ->withBallOffPitch()
            ->build();
        // přesnost 4, rozptyly 3/7/7, brnění soupeře 2+2 (drží), rozptyl o pole D8 3 ⇒ (8,5), brnění hozeného 2+2
        $dice = new FixedDiceRoller([4, 3, 7, 7, 2, 2, 3, 2, 2]);
        $r = (new ActionResolver($dice))->resolve($state, ActionType::THROW_TEAM_MATE, [
            'playerId' => 1, 'targetId' => 2, 'targetX' => 8, 'targetY' => 5,
        ]);

        $s = $r->getNewState();
        $this->assertSame(PlayerState::PRONE, $s->requirePlayer(3)->getState(), 'na koho dopadl, je sražen');
        $this->assertSame(PlayerState::PRONE, $s->requirePlayer(2)->getState(), 'hozený bez hodu na přistání leží');
        $this->assertSame(8, $s->requirePlayer(2)->requirePosition()->getX());
        $this->assertFalse($dice->hasRemainingRolls());
        $this->assertFalse($r->isTurnover(), 'sražen soupeř, hozený bez míče');
    }

    public function testP279NepovedenePristaniSMicemJeTurnover(): void
    {
        // jako ThrowTeamMateTest::testFailLandingWithBallBounce: přistání 1 ⇒ leží, míč odskočí ⇒ turnover (ř. 8430-8431)
        $state = (new GameStateBuilder())
            // bez týmových přehozů: test měří nepovedený hod, ne přehoz (P283)
            ->withHomeTeam(\App\DTO\TeamStateDTO::create(1, 'Home', 'Human', TeamSide::HOME, 0))
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 5, skills: [SkillName::ThrowTeamMate], id: 1)
            ->addPlayer(TeamSide::HOME, 6, 5, skills: [SkillName::RightStuff], id: 2)
            ->withBallCarried(2)
            ->build();
        $dice = new FixedDiceRoller([4, 3, 3, 3, 1, 2, 2, 3]);
        $r = (new ActionResolver($dice))->resolve($state, ActionType::THROW_TEAM_MATE, [
            'playerId' => 1, 'targetId' => 2, 'targetX' => 8, 'targetY' => 5,
        ]);

        $this->assertTrue($r->isTurnover());
        $this->assertNull($r->getNewState()->getBall()->getCarrierId());
    }

    public function testP283TymovyPrehozZachraniPristani(): void
    {
        // ř. 929-931 týmový přehoz na kterýkoli hod. Přesnost 4, rozptyly 3/3/7 ⇒ (9,5); přistání AG3 4+: 1 ⇒ přehoz 6
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 5, skills: [SkillName::ThrowTeamMate], id: 1)
            ->addPlayer(TeamSide::HOME, 6, 5, skills: [SkillName::RightStuff], id: 2)
            ->withBallOffPitch()
            ->build();
        $dice = new FixedDiceRoller([4, 3, 3, 7, 1, 6]);
        $r = (new ActionResolver($dice))->resolve($state, ActionType::THROW_TEAM_MATE, [
            'playerId' => 1, 'targetId' => 2, 'targetX' => 8, 'targetY' => 5,
        ]);

        $s = $r->getNewState();
        $this->assertSame(PlayerState::STANDING, $s->requirePlayer(2)->getState());
        $this->assertSame(9, $s->requirePlayer(2)->requirePosition()->getX());
        $this->assertSame(2, $s->getTeamState(TeamSide::HOME)->getRerolls());
        $this->assertFalse($dice->hasRemainingRolls());
    }

    public function testP283TymovyPrehozZachraniFumbleHodu(): void
    {
        // přesnost 1 = fumble ⇒ přehoz 4; rozptyly 3/3/7 ⇒ (9,5); přistání 6
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 5, skills: [SkillName::ThrowTeamMate], id: 1)
            ->addPlayer(TeamSide::HOME, 6, 5, skills: [SkillName::RightStuff], id: 2)
            ->withBallOffPitch()
            ->build();
        $dice = new FixedDiceRoller([1, 4, 3, 3, 7, 6]);
        $r = (new ActionResolver($dice))->resolve($state, ActionType::THROW_TEAM_MATE, [
            'playerId' => 1, 'targetId' => 2, 'targetX' => 8, 'targetY' => 5,
        ]);

        $s = $r->getNewState();
        $this->assertSame(9, $s->requirePlayer(2)->requirePosition()->getX());
        $this->assertSame(PlayerState::STANDING, $s->requirePlayer(2)->getState());
        $this->assertFalse($dice->hasRemainingRolls());
    }
}
