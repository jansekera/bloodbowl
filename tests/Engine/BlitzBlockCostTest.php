<?php

declare(strict_types=1);

namespace App\Tests\Engine;

use App\Engine\ActionResolver;
use App\Engine\FixedDiceRoller;
use App\Enum\ActionType;
use App\Enum\PlayerState;
use App\Enum\TeamSide;
use PHPUnit\Framework\TestCase;

/**
 * P82 (30.09.2026), port C++ 366fda3e. `rules_bb2016.txt` r. 546-552:
 *   „The block may be made at any point during the move, but COSTS ONE SQUARE
 *   OF MOVEMENT for the player to make."
 *   Bez normalniho pohybu smi blitzujici na ranu GFI (2+, ve vanici 3+, pocita
 *   se do limitu GFI); bez GFI se rana nehodi vubec. Neuspesne GFI = pad ve
 *   vlastnim poli pred ranou, turnover. PHP za ranu nestrhaval nic.
 */
final class BlitzBlockCostTest extends TestCase
{
    public function testTheBlockCostsOneSquareOfMovement(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1)                 // MA 6
            ->addPlayer(TeamSide::AWAY, 6, 7, id: 2)
            ->withBallOffPitch()
            ->build();

        $resolver = new ActionResolver(new FixedDiceRoller([3, 3, 3, 3]));   // PUSHED
        $result = $resolver->resolve($state, ActionType::BLITZ, ['playerId' => 1, 'targetId' => 2]);

        $this->assertFalse($result->isTurnover());
        $this->assertSame(
            5,
            $result->getNewState()->requirePlayer(1)->getMovementRemaining(),
            'rana v blitzu nestrhla pole pohybu',
        );
    }

    public function testWithNoMovementLeftTheBlockNeedsAGfiAndAFailFallsBeforeIt(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 7, id: 2)
            ->withBallOffPitch()
            ->build();
        $state = $state->withPlayer($state->requirePlayer(1)->withMovementRemaining(0));
        $state = $state->withTeamState(TeamSide::HOME, $state->getTeamState(TeamSide::HOME)->withRerolls(0));

        // GFI 1 = pad · brneni 1+1 neprorazi
        $resolver = new ActionResolver(new FixedDiceRoller([1, 1, 1, 1, 1]));
        $result = $resolver->resolve($state, ActionType::BLITZ, ['playerId' => 1, 'targetId' => 2]);

        $this->assertTrue($result->isTurnover(), 'neuspesne GFI na ranu neni turnover');
        $this->assertSame(PlayerState::PRONE, $result->getNewState()->requirePlayer(1)->getState());
        $this->assertSame(
            6,
            $result->getNewState()->requirePlayer(2)->requirePosition()->getX(),
            'rana se hodila, ackoli blitzujici padl na GFI',
        );
        // Samotny pad by vysel i bez opravy (kostka 1 = ATTACKER DOWN) -- rozlisi
        // to az udalosti: hazelo se GFI a rana se NEHODILA.
        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('gfi', $types, 'na ranu se GFI nehazelo');
        $this->assertNotContains('block', $types, 'rana se hodila');
    }

    public function testWithNoMovementAndNoGfiLeftNoBlockIsThrown(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 7, id: 2)
            ->withBallOffPitch()
            ->build();
        // oba GFI uz padly na pohyb
        $state = $state->withPlayer($state->requirePlayer(1)->withMovementRemaining(-2));

        // Prazdna kostka: kdyby se hazela rana, test spadne na „no more rolls".
        $resolver = new ActionResolver(new FixedDiceRoller([]));
        $result = $resolver->resolve($state, ActionType::BLITZ, ['playerId' => 1, 'targetId' => 2]);

        $this->assertFalse($result->isTurnover());
        $this->assertSame(6, $result->getNewState()->requirePlayer(2)->requirePosition()->getX());
        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertNotContains('block', $types, 'rana bez zaplaceni');
    }

    public function testTheApproachLeavesASquareToPayForTheBlock(): void
    {
        // MA 3, k souperi je to presne 3 pole: dojit na 3 a ranu zaplatit GFI,
        // NEBO dojit na 3 a ... jina cesta neni. Hlida, ze se rana zaplati GFI
        // (hod 6) a ze se po ni odecetlo pole: zustane -1.
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 2, 7, movement: 3, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 7, id: 2)
            ->withBallOffPitch()
            ->build();

        // GFI na ranu 6 · kostka rany 3 = PUSHED
        $resolver = new ActionResolver(new FixedDiceRoller([6, 3, 3, 3]));
        $result = $resolver->resolve($state, ActionType::BLITZ, ['playerId' => 1, 'targetId' => 2]);

        $this->assertFalse($result->isTurnover());
        $this->assertSame(-1, $result->getNewState()->requirePlayer(1)->getMovementRemaining());
        $this->assertSame(
            7,
            $result->getNewState()->requirePlayer(2)->requirePosition()->getX(),
            'rana se nehodila',
        );
    }

    public function testAnApproachThatWouldLeaveNothingForTheBlockIsNotChosenAsTheBlitzSquare(): void
    {
        // MA 2, k sousednimu poli souperova je to 4 pole = 2 GFI. Na ranu by
        // nezbylo nic -- blitz ji tedy NESMI naplanovat jako dosazitelnou
        // (driv dosel na 2 GFI a ranu dostal zadarmo).
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 1, 7, movement: 2, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 7, id: 2)
            ->withBallOffPitch()
            ->build();

        $resolver = new ActionResolver(new FixedDiceRoller([6, 6, 6, 3, 3, 3, 3]));
        $result = $resolver->resolve($state, ActionType::BLITZ, ['playerId' => 1, 'targetId' => 2]);

        $this->assertFalse($result->isTurnover());
        $this->assertSame(
            6,
            $result->getNewState()->requirePlayer(2)->requirePosition()->getX(),
            'rana se hodila bez zaplaceni',
        );
        // Kdyz rana nebude, je GFI ciste riziko: priblizeni jde jen normalnim pohybem.
        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertNotContains('gfi', $types, 'dosel na GFI, ackoli ranu stejne nezaplati');
        $this->assertSame(3, $result->getNewState()->requirePlayer(1)->requirePosition()->getX());
    }
}
