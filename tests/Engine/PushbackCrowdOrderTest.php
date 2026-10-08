<?php

declare(strict_types=1);

namespace App\Tests\Engine;

use App\DTO\ActionResult;
use App\DTO\GameState;
use App\Engine\ActionResolver;
use App\Engine\FixedDiceRoller;
use App\Enum\ActionType;
use App\Enum\PlayerState;
use App\Enum\TeamSide;
use PHPUnit\Framework\TestCase;

/**
 * DO DAVU JEN TEHDY, KDYŽ NA HŘIŠTI NENÍ VOLNÉ POLE ODTLAČENÍ (audit parity 08.10.2026, nález 3a).
 *
 * `rules_bb2016.txt` ř. 639: "The player **must be pushed back into an empty square if
 * possible**." ř. 641-644: "If all such squares are occupied by other players, then the
 * player is pushed into an occupied square ..." ř. 650-651: "Players must be pushed off the
 * pitch if there are **no eligible empty squares on the pitch**."
 *
 * Stará mechanika: bylo-li kterékoli ze tří polí odtlačení mimo hřiště, šel hráč do davu,
 * i když vedle bylo volné pole na hřišti ⇒ každé odtlačení podél lajny = crowd surf.
 *
 * Kostky: 3 = Pushed (1 kostka, stejná síla); další dvě trojky jsou zranění od davu, které
 * spotřebuje jen stará mechanika (a větev, kde dav podle pravidel opravdu je).
 */
final class PushbackCrowdOrderTest extends TestCase
{
    private function blok(GameState $state): ActionResult
    {
        return (new ActionResolver(new FixedDiceRoller([3, 3, 3])))
            ->resolve($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);
    }

    /** @return list<string> */
    private function typy(ActionResult $r): array
    {
        return array_map(fn($e) => $e->getType(), $r->getEvents());
    }

    public function testPodelLajnyJdeOdtlacenyNaVolnePoleNeDoDavu(): void
    {
        // Útočník (9,0) → obránce (10,0): pole odtlačení (11,-1) mimo, (11,0) a (11,1) volná.
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 9, 0, id: 1)
            ->addPlayer(TeamSide::AWAY, 10, 0, id: 2)
            ->build();

        $r = $this->blok($state);

        $this->assertNotContains('crowd_surf', $this->typy($r), 'ř. 639: volné pole na hřišti má přednost před davem');
        $obrance = $r->getNewState()->requirePlayer(2);
        $this->assertSame(PlayerState::STANDING, $obrance->getState());
        $this->assertSame(11, $obrance->requirePosition()->getX());
        $this->assertContains($obrance->requirePosition()->getY(), [0, 1]);
    }

    public function testSikmoKLajneJdeOdtlacenyNaJedineVolnePole(): void
    {
        // Útočník (5,1) → obránce (6,0): (7,-1) a (6,-1) mimo, (7,0) volné.
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 1, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 0, id: 2)
            ->build();

        $r = $this->blok($state);

        $this->assertNotContains('crowd_surf', $this->typy($r), 'ř. 639: volné pole na hřišti má přednost před davem');
        $pos = $r->getNewState()->requirePlayer(2)->requirePosition();
        $this->assertSame([7, 0], [$pos->getX(), $pos->getY()]);
    }

    public function testBezVolnehoPoleNaHristiJdeOdtlacenyDoDavuNeDoRetezu(): void
    {
        // ř. 650-651: žádné volné pole na hřišti ⇒ dav. Obě pole na hřišti jsou obsazená,
        // třetí je mimo hřiště.
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 9, 0, id: 1)
            ->addPlayer(TeamSide::AWAY, 10, 0, id: 2)
            ->addPlayer(TeamSide::AWAY, 11, 0, id: 3)
            ->addPlayer(TeamSide::AWAY, 11, 1, id: 4)
            ->build();

        $r = $this->blok($state);

        $this->assertContains('crowd_surf', $this->typy($r));
        $this->assertNull($r->getNewState()->requirePlayer(2)->getPosition());
        $this->assertNotContains('chain_push', $this->typy($r));
    }
}
