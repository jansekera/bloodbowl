<?php

declare(strict_types=1);

namespace App\Tests\Engine;

use App\Engine\FixedDiceRoller;
use App\Engine\GameFlowResolver;
use App\Enum\PlayerState;
use App\Enum\TeamSide;
use PHPUnit\Framework\TestCase;

/**
 * ⭐ HOD NA NAVRAT KO HRACU SE DELA I PO TOUCHDOWNU (16.09.2026).
 *
 * `rules_bb2016.txt` r. 1007-1012: "After a touchdown has been scored, AND at the start
 * of the second half, play is restarted ... Before the kick-off however each coach should
 * roll one D6 for each KO'd player on his team. On a roll of 4, 5 or 6 the player is fit
 * enough to return to play."
 * ⛔ Engine to delal JEN o polocase (`resolveHalfTime`), po touchdownu ne.
 */
final class KoRecoveryAfterTouchdownTest extends TestCase
{
    private function stavSKo(): \App\DTO\GameState
    {
        $s = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, id: 1)
            ->addPlayer(TeamSide::AWAY, 20, 7, id: 2)
            ->addPlayer(TeamSide::HOME, 6, 5, id: 3)
            ->build();

        return $s->withPlayer($s->getPlayer(3)->withState(PlayerState::KO)->withPosition(null));
    }

    public function testPoTouchdownuSeHaziNaNavratKoHracu(): void
    {
        $r = (new GameFlowResolver(new FixedDiceRoller([5])))->resolvePostTouchdown($this->stavSKo());

        $typy = array_map(static fn($e) => $e->getType(), $r['events']);
        $this->assertContains('ko_recovery', $typy, 'po touchdownu se hazi na navrat KO hracu');
        $this->assertSame(PlayerState::OFF_PITCH, $r['state']->getPlayer(3)->getState(), 'hod 5 = vraci se do rezerv');
    }

    public function testHodTriHraceNevraci(): void
    {
        $r = (new GameFlowResolver(new FixedDiceRoller([3])))->resolvePostTouchdown($this->stavSKo());

        $this->assertSame(PlayerState::KO, $r['state']->getPlayer(3)->getState(), 'hod 3 = zustava v KO');
    }

    public function testPozitivniKontrolaPolocasHaziDal(): void
    {
        // Tataz fixtura o polocase -- tam se hazelo uz driv, musi to platit dal.
        $r = (new GameFlowResolver(new FixedDiceRoller([5])))->resolveHalfTime($this->stavSKo());

        $typy = array_map(static fn($e) => $e->getType(), $r['events']);
        $this->assertContains('ko_recovery', $typy);
        $this->assertSame(PlayerState::OFF_PITCH, $r['state']->getPlayer(3)->getState());
    }
}
