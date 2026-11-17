<?php

declare(strict_types=1);

namespace App\Tests\Engine;

use App\Engine\ActionResolver;
use App\Engine\FixedDiceRoller;
use App\Enum\ActionType;
use App\Enum\TeamSide;
use PHPUnit\Framework\TestCase;

/**
 * P294 krok 5d (nález 17.11.2026 při porovnání her se semínkem): „A coach may not use more than one Re-roll counter
 * per turn“ (`rules_bb2016.txt` ř. 934-935). Dřív `BallResolver::resolvePickup` týmový přehoz zvednutí ve stavu
 * neodečetl (dělal to až volající, po odrazu) ⇒ míč odražený ke spoluhráči chytal s DRUHÝM týmovým přehozem.
 */
final class JedenPrehozZaTahZvednutiTest extends TestCase
{
    public function testPoPrehozenemZvednutiChytaniOdrazuUzNeprehazuje(): void
    {
        $s = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, agility: 3, id: 1)
            ->addPlayer(TeamSide::HOME, 6, 6, agility: 3, id: 2)
            ->withBallOnGround(6, 5)
            ->build();
        $pred = $s->getHomeTeam()->getRerolls();
        // zvednutí 1, týmový přehoz 1, odraz D8 5 = (0,+1) na hráče 2, chytání 1, odraz 1, zbytek výplň
        $r = (new ActionResolver(new FixedDiceRoller([1, 1, 5, 1, 1, 1, 1, 1, 1, 1])))
            ->resolve($s, ActionType::MOVE, ['playerId' => 1, 'x' => 6, 'y' => 5]);

        $this->assertTrue($r->isTurnover());
        $this->assertSame($pred - 1, $r->getNewState()->getHomeTeam()->getRerolls(), 'jeden týmový přehoz za tah');
        $tymove = array_filter($r->getEvents(), static fn($e) => $e->getType() === 'reroll' && ($e->getData()['source'] ?? '') === 'Team Reroll');
        $this->assertCount(1, $tymove);
    }

    /** Týž princip u předávky: nechycená po týmovém přehozu, odraz ke spoluhráči ⇒ ten chytá bez přehozu. */
    public function testPoPrehozenemChytaniPredavkyOdrazBezDruhehoPrehozu(): void
    {
        $s = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, agility: 3, id: 1)
            ->addPlayer(TeamSide::HOME, 6, 5, agility: 3, id: 2)
            ->addPlayer(TeamSide::HOME, 6, 6, agility: 3, id: 3)
            ->withBallCarried(1)
            ->build();
        $pred = $s->getHomeTeam()->getRerolls();
        // chytání 1, týmový přehoz 1, odraz D8 5 = (0,+1) na hráče 3, chytání 1, odraz 1, výplň
        $r = (new ActionResolver(new FixedDiceRoller([1, 1, 5, 1, 1, 1, 1, 1, 1, 1])))
            ->resolve($s, ActionType::HAND_OFF, ['playerId' => 1, 'targetId' => 2]);

        $this->assertSame($pred - 1, $r->getNewState()->getHomeTeam()->getRerolls(), 'jeden týmový přehoz za tah');
        $tymove = array_filter($r->getEvents(), static fn($e) => $e->getType() === 'reroll' && ($e->getData()['source'] ?? '') === 'Team Reroll');
        $this->assertCount(1, $tymove);
    }
}
