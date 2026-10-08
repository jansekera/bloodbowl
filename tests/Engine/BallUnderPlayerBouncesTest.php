<?php

declare(strict_types=1);

namespace App\Tests\Engine;

use App\DTO\ActionResult;
use App\DTO\GameState;
use App\Engine\ActionResolver;
use App\Engine\FixedDiceRoller;
use App\Enum\ActionType;
use App\Enum\PlayerState;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use PHPUnit\Framework\TestCase;

/**
 * MÍČ POD HRÁČEM ODSKOČÍ (audit parity 08.10.2026, nález 13).
 *
 * `rules_bb2016.txt` ř. 441-444: "Players that move into the square with the ball at other
 * times (e.g., when pushed back, thrown by another player with Throw Team-Mate, etc.) cannot
 * pick up the ball, and instead it will bounce one square. This does not cause a turnover."
 * ř. 640-641: "A square containing only the ball is considered empty and a player pushed to
 * it will cause the ball to bounce." ř. 896-900: "If ... a player is pushed to or lands in
 * the ball's square ... then it will bounce."
 *
 * Stará mechanika: řešil se jen míč, který hráč NESL. Volný míč na poli, kam byl hráč
 * odtlačen nebo kde při úhybu / GFI spadl, zůstal ležet pod ním.
 *
 * Odskok D8: 3 = o pole doprava (+x), 5 = o pole dolů (+y).
 */
final class BallUnderPlayerBouncesTest extends TestCase
{
    /** @return list<string> */
    private function typy(ActionResult $r): array
    {
        return array_map(fn($e) => $e->getType(), $r->getEvents());
    }

    /** @return array{int, int}|null */
    private function mic(ActionResult $r): ?array
    {
        $p = $r->getNewState()->getBall()->getPosition();

        return $p === null ? null : [$p->getX(), $p->getY()];
    }

    /** Útočník (5,7) → obránce (6,7); jediné volné pole odtlačení je (7,7) a leží na něm míč. */
    private function stavOdtlaceniNaMic(): GameState
    {
        return (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 7, id: 2)
            ->addPronePlayer(TeamSide::AWAY, 7, 6, id: 3)
            ->addPronePlayer(TeamSide::AWAY, 7, 8, id: 4)
            ->withBallOnGround(7, 7)
            ->build();
    }

    public function testOdtlacenyNaPoleSMicemMicOdrazi(): void
    {
        // Kostky: 3 = Pushed · odskok D8 = 3 ⇒ míč z (7,7) na (8,7).
        $r = (new ActionResolver(new FixedDiceRoller([3, 3])))
            ->resolve($this->stavOdtlaceniNaMic(), ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);

        $obrance = $r->getNewState()->requirePlayer(2)->requirePosition();
        $this->assertSame([7, 7], [$obrance->getX(), $obrance->getY()], 'fixtura: odtlačen na pole s míčem');
        $this->assertContains('ball_bounce', $this->typy($r), 'ř. 640-641');
        $this->assertSame([8, 7], $this->mic($r), 'ř. 443-444: "it will bounce one square"');
        $this->assertFalse($r->getNewState()->getBall()->isHeld(), 'ř. 442-443: "cannot pick up the ball"');
        $this->assertFalse($r->isTurnover(), 'ř. 444: "This does not cause a turnover"');
    }

    public function testOdtlacenyASrazenyNaPoleSMicemMicOdrazi(): void
    {
        // Kostky: 6 = Defender Down · odskok D8 = 3 · brnění 1+1 neprorazí.
        $r = (new ActionResolver(new FixedDiceRoller([6, 3, 1, 1])))
            ->resolve($this->stavOdtlaceniNaMic(), ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);

        $this->assertSame(PlayerState::PRONE, $r->getNewState()->requirePlayer(2)->getState());
        $this->assertSame([8, 7], $this->mic($r), 'ř. 896-900');
    }

    /** HOME id 1 na (5,5), AWAY na (5,4) ⇒ krok na (5,6) je úhyb na 3+; na (5,6) leží míč. */
    private function stavUhybNaMic(): GameState
    {
        $s = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, id: 1)
            ->addPlayer(TeamSide::AWAY, 5, 4, id: 2)
            ->withBallOnGround(5, 6)
            ->build();

        return $s->withTeamState(TeamSide::HOME, $s->getTeamState(TeamSide::HOME)->withRerolls(0));
    }

    public function testKdoSpadnePriUhybuNaPoleSMicemMicOdrazi(): void
    {
        // Kostky: úhyb 2 = pád · brnění 1+1 · odskok D8 = 5 ⇒ míč z (5,6) na (5,7).
        $r = (new ActionResolver(new FixedDiceRoller([2, 1, 1, 5])))
            ->resolve($this->stavUhybNaMic(), ActionType::MOVE, ['playerId' => 1, 'x' => 5, 'y' => 6]);

        $this->assertSame(PlayerState::PRONE, $r->getNewState()->requirePlayer(1)->getState(), 'fixtura: hráč spadl');
        $this->assertNotContains('pickup', $this->typy($r), 'ř. 442-443: spadlý míč nezvedá');
        $this->assertSame([5, 7], $this->mic($r), 'ř. 896-900: "lands in the ball\'s square ... it will bounce"');
    }

    public function testKdoSpadnePriUhybuAOdnesouHoMicPresToOdskoci(): void
    {
        // Kostky: úhyb 2 = pád · brnění 6+6 prorazí · zranění 4+4 = KO · odskok D8 = 5.
        $r = (new ActionResolver(new FixedDiceRoller([2, 6, 6, 4, 4, 5])))
            ->resolve($this->stavUhybNaMic(), ActionType::MOVE, ['playerId' => 1, 'x' => 5, 'y' => 6]);

        $this->assertSame(PlayerState::KO, $r->getNewState()->requirePlayer(1)->getState(), 'fixtura: hráč je KO');
        $this->assertSame([5, 7], $this->mic($r), 'ř. 896-900: spadl na pole míče dřív, než ho odnesli');
    }

    public function testKdoSpadnePriGfiNaPoleSMicemMicOdrazi(): void
    {
        // HOME id 1 s MA 1 jde o dvě pole ⇒ druhý krok na (5,7) je GFI; na (5,7) leží míč.
        // Kostky: GFI 1 = pád · brnění 1+1 · odskok D8 = 5 ⇒ míč z (5,7) na (5,8).
        $s = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, movement: 1, id: 1)
            ->addPlayer(TeamSide::AWAY, 20, 12, id: 2)
            ->withBallOnGround(5, 7)
            ->build();
        $s = $s->withTeamState(TeamSide::HOME, $s->getTeamState(TeamSide::HOME)->withRerolls(0));

        $r = (new ActionResolver(new FixedDiceRoller([1, 1, 1, 5])))
            ->resolve($s, ActionType::MOVE, ['playerId' => 1, 'x' => 5, 'y' => 7]);

        $this->assertSame(PlayerState::PRONE, $r->getNewState()->requirePlayer(1)->getState(), 'fixtura: hráč spadl');
        $this->assertSame([5, 8], $this->mic($r), 'ř. 896-900');
    }

    public function testHozenySpoluhracKteryDopadneNaPoleSMicemMicOdrazi(): void
    {
        // ř. 441-444 jmenuje Throw Team-Mate výslovně. Hod z (5,5) na (8,5).
        // Kostky: přesnost 4 · 3× rozptyl D8 = 3 ⇒ dopad na (11,5), kde leží míč ·
        //   přistání 4 (úspěch) · odskok D8 = 3 ⇒ míč na (12,5).
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 5, skills: [SkillName::ThrowTeamMate], id: 1)
            ->addPlayer(TeamSide::HOME, 6, 5, skills: [SkillName::RightStuff], id: 2)
            ->withBallOnGround(11, 5)
            ->build();

        $r = (new ActionResolver(new FixedDiceRoller([4, 3, 3, 3, 4, 3])))
            ->resolve($state, ActionType::THROW_TEAM_MATE, ['playerId' => 1, 'targetId' => 2, 'targetX' => 8, 'targetY' => 5]);

        $hozeny = $r->getNewState()->requirePlayer(2)->requirePosition();
        $this->assertSame([11, 5], [$hozeny->getX(), $hozeny->getY()], 'fixtura: dopadl na pole s míčem');
        $this->assertSame([12, 5], $this->mic($r), 'ř. 443-444: "it will bounce one square"');
        $this->assertFalse($r->getNewState()->getBall()->isHeld());
        $this->assertFalse($r->isTurnover(), 'ř. 444: "This does not cause a turnover"');
    }
}
