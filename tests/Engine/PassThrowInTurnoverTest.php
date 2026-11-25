<?php

declare(strict_types=1);

namespace App\Tests\Engine;

use App\DTO\ActionResult;
use App\Engine\ActionResolver;
use App\Engine\FixedDiceRoller;
use App\Enum\ActionType;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use PHPUnit\Framework\TestCase;

/**
 * P89 (audit parity — C++ opraveno 07.10.2026 `eedd266a`, PHP jen čteno): přihrávka, která vyletí z hřiště, je
 * turnover až podle toho, kdo míč nakonec drží. `rules_bb2016.txt` ř. 881-891: turnover, když míč „finally comes to
 * rest“ a nechytí ho hráč týmu na tahu — i po vhození („thrown back into an empty square, and as long as it was
 * caught by a player from the moving team then the turnover would be avoided“). Parita s C++
 * `PassHandler.P89ThrowIn*`. Scénář jako `MissedPassScatterTest::testHailMaryOffPitch…`: Hail Mary 4, rozptyl
 * (20,0) → (20,-1) mimo, vhození od (20,0) kolmo 1+2 = 3 pole, první je (20,0) samo (FAQ ř. 9307-9314) ⇒ (20,2), chytá 6.
 */
final class PassThrowInTurnoverTest extends TestCase
{
    private function prihravka(TeamSide $chytajici): ActionResult
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 1, 5, agility: 3, skills: [SkillName::HailMaryPass], id: 1)
            ->addPlayer(TeamSide::HOME, 20, 1, agility: 3, id: 2)
            ->addPlayer($chytajici, 20, 2, agility: 3, id: 13)
            ->withBallCarried(1)
            ->build();

        return (new ActionResolver(new FixedDiceRoller([4, 1, 1, 3, 1, 2, 6])))->resolve($state, ActionType::PASS, [
            'playerId' => 1, 'targetX' => 20, 'targetY' => 1,
        ]);
    }

    public function testVhozenyMicChytilNasHracNeniTurnover(): void
    {
        $r = $this->prihravka(TeamSide::HOME);

        $this->assertSame(13, $r->getNewState()->getBall()->getCarrierId(), 'fixtura: po vhození chytil hráč 13');
        $this->assertFalse($r->isTurnover());
    }

    public function testVhozenyMicChytilSouperJeTurnover(): void
    {
        $r = $this->prihravka(TeamSide::AWAY);

        $this->assertSame(13, $r->getNewState()->getBall()->getCarrierId());
        $this->assertTrue($r->isTurnover());
    }
}
