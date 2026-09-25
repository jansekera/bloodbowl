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
 * Kdo drzi pole proti odtlaceni (balik E, E18-E20; vzor C++ `holdsGround`):
 * - Take Root: "may not ... be pushed back for any reason" (`rules_bb2016.txt` r. 8578-8579),
 *   zakoreneni konci srazenim nebo polozenim (r. 8575-8576);
 * - Stand Firm (r. 8510-8516) jen STOJICI: "Only Extraordinary skills work when a
 *   player is Prone or Stunned" (r. 1824-1825).
 */
final class HoldsGroundTest extends TestCase
{
    public function testRootedAndStandFirmRules(): void
    {
        $s = (new GameStateBuilder())
            ->addPlayer(TeamSide::AWAY, 6, 5, skills: [SkillName::StandFirm], id: 1)
            ->addPronePlayer(TeamSide::AWAY, 7, 5, skills: [SkillName::StandFirm], id: 2)
            ->addPlayer(TeamSide::AWAY, 8, 5, id: 3)
            ->build();

        $this->assertTrue($s->requirePlayer(1)->holdsGround(TeamSide::HOME), 'stojici Stand Firm proti souperi');
        $this->assertFalse($s->requirePlayer(1)->holdsGround(TeamSide::AWAY), 'vlastni tym Stand Firm nevyuzije');
        $this->assertFalse($s->requirePlayer(2)->holdsGround(TeamSide::HOME), 'lezici Stand Firm nepouzije');
        $zakoreneny = $s->requirePlayer(3)->withRooted(true);
        $this->assertTrue($zakoreneny->holdsGround(TeamSide::HOME));
        $this->assertTrue($zakoreneny->holdsGround(TeamSide::AWAY), 'zakoreneny i proti vlastnimu tymu');
    }

    public function testKnockdownEndsRoot(): void
    {
        $p = (new GameStateBuilder())->addPlayer(TeamSide::HOME, 5, 5, id: 1)->build()->requirePlayer(1)->withRooted(true);

        $this->assertFalse($p->withState(PlayerState::PRONE)->isRooted());
        $this->assertTrue($p->withState(PlayerState::STANDING)->isRooted());
    }

    /** Bezny blok, vysledek Pushed: zakoreneny zustane stat na miste. */
    public function testRootedDefenderIsNotPushedByBlock(): void
    {
        $s = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, id: 2)
            ->withBallOffPitch()
            ->build();
        $s = $s->withPlayer($s->requirePlayer(2)->withRooted(true));

        // 1 kostka: 3 = Pushed
        $r = (new ActionResolver(new FixedDiceRoller([3])))->resolve($s, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);

        $pos = $r->getNewState()->requirePlayer(2)->requirePosition();
        $this->assertSame([6, 5], [$pos->getX(), $pos->getY()]);
    }

    /** Fanatic proti zakorenenemu stromu: Pushed ho nepohne, Fanatic nepostoupi. */
    public function testBallAndChainCannotPushRootedPlayer(): void
    {
        $s = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, movement: 1, id: 1, skills: [SkillName::BallAndChain])
            ->addPlayer(TeamSide::AWAY, 6, 7, id: 2)
            ->withBallOffPitch()
            ->build();
        $s = $s->withPlayer($s->requirePlayer(2)->withRooted(true));

        // D6 3 => rovne na (6,7); 1 kostka: 3 = Pushed
        $r = (new ActionResolver(new FixedDiceRoller([3, 3])))->resolve($s, ActionType::BALL_AND_CHAIN, ['playerId' => 1]);

        $strom = $r->getNewState()->requirePlayer(2)->requirePosition();
        $bnc = $r->getNewState()->requirePlayer(1)->requirePosition();
        $this->assertSame([6, 7], [$strom->getX(), $strom->getY()]);
        $this->assertSame([5, 7], [$bnc->getX(), $bnc->getY()], 'pole se neuvolnilo, follow-up neni');
    }

    /** Fanatic proti LEZICIMU se Stand Firm: odtlaci ho (r. 1824-1825). */
    public function testBallAndChainPushesProneStandFirm(): void
    {
        $s = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, movement: 1, id: 1, skills: [SkillName::BallAndChain])
            ->addPronePlayer(TeamSide::AWAY, 6, 7, skills: [SkillName::StandFirm], id: 2)
            ->addPlayer(TeamSide::AWAY, 9, 7, id: 3)
            ->withBallOffPitch()
            ->build();

        // D6 3 => (6,7) lezici; odtlaceni + brneni 2+2 drzi
        $r = (new ActionResolver(new FixedDiceRoller([3, 2, 2])))->resolve($s, ActionType::BALL_AND_CHAIN, ['playerId' => 1]);

        $pos = $r->getNewState()->requirePlayer(2)->requirePosition();
        $this->assertSame(7, $pos->getX(), 'odtlacen, Stand Firm vleze neplati');
        $this->assertLessThanOrEqual(1, abs($pos->getY() - 7));
    }
}
