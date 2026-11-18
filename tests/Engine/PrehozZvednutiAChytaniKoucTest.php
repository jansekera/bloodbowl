<?php

declare(strict_types=1);

namespace App\Tests\Engine;

use App\Engine\ActionResolver;
use App\Engine\FixedDiceRoller;
use App\Enum\ActionType;
use App\Enum\TeamSide;
use PHPUnit\Framework\TestCase;

/**
 * P294 krok 5d: týmový přehoz smí přehodit kterýkoli hod a je to volba kouče (`rules_bb2016.txt` ř. 929-931).
 * OPRAVENO 17.11.2026 — zvednutí a chytání přehazovala hra sama i za kouče-člověka (dialog 'pickup' se nikdy
 * nenabídl). Teď otázka přehráním jako u úhybu: stav v okamžiku hodu, pole hodu; po odpovědi akce doběhne.
 */
final class PrehozZvednutiAChytaniKoucTest extends TestCase
{
    /** @param list<int> $kostky */
    private function kouc(array $kostky): ActionResolver
    {
        $r = new ActionResolver(new FixedDiceRoller($kostky));
        $r->setHumanSides([TeamSide::HOME]);

        return $r;
    }

    private function micNaZemi(): \App\DTO\GameState
    {
        return (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, agility: 3, id: 1)
            ->withBallOnGround(6, 5)
            ->build();
    }

    public function testNepovedeneZvednutiSeZeptaKouce(): void
    {
        $r = $this->kouc([1])->resolve($this->micNaZemi(), ActionType::MOVE, ['playerId' => 1, 'x' => 6, 'y' => 5]);

        $q = $r->getNewState()->getPendingReroll();
        $this->assertNotNull($q, 'kouč dostane otázku');
        $this->assertSame('pickup', $q->getRollType()->value);
        $this->assertSame([6, 5], [$q->getTargetX(), $q->getTargetY()]);
        $this->assertSame(6, $r->getNewState()->requirePlayer(1)->requirePosition()->getX(), 'u otázky stojí na míči');
        $this->assertFalse($r->isTurnover(), 'neúspěch ještě neplatí');
        $this->assertSame($this->micNaZemi()->getHomeTeam()->getRerolls(), $r->getNewState()->getHomeTeam()->getRerolls(), 'přehoz zatím nespotřebován');
    }

    /** Kouč u otázky vidí průběh ve správném pořadí (krok, pak hod) a po odpovědi se nic z toho neopakuje. */
    public function testOtazkaNeseKrokPredHodemAOdpovedHoNeopakuje(): void
    {
        $q = $this->kouc([1])->resolve($this->micNaZemi(), ActionType::MOVE, ['playerId' => 1, 'x' => 6, 'y' => 5]);
        $typy = array_map(static fn($e) => $e->getType(), $q->getEvents());
        $this->assertSame(['player_move', 'pickup'], $typy);

        $r = $this->kouc([6])->resolve($q->getNewState(), ActionType::RESOLVE_REROLL, ['choice' => 'team_reroll']);
        $this->assertSame(['reroll', 'pickup'], array_map(static fn($e) => $e->getType(), $r->getEvents()));
    }

    public function testOdmitnutiMicOdskociATahKonci(): void
    {
        $s = $this->kouc([1])->resolve($this->micNaZemi(), ActionType::MOVE, ['playerId' => 1, 'x' => 6, 'y' => 5])->getNewState();
        $r = $this->kouc([1, 1, 1])->resolve($s, ActionType::RESOLVE_REROLL, ['choice' => 'decline']);

        $this->assertTrue($r->isTurnover());
        $this->assertFalse($r->getNewState()->getBall()->isHeld());
        $this->assertSame($s->getHomeTeam()->getRerolls(), $r->getNewState()->getHomeTeam()->getRerolls(), 'přehoz zůstal');
    }

    public function testTymovyPrehozZvedne(): void
    {
        $s = $this->kouc([1])->resolve($this->micNaZemi(), ActionType::MOVE, ['playerId' => 1, 'x' => 6, 'y' => 5])->getNewState();
        $r = $this->kouc([6])->resolve($s, ActionType::RESOLVE_REROLL, ['choice' => 'team_reroll']);

        $this->assertFalse($r->isTurnover());
        $this->assertSame(1, $r->getNewState()->getBall()->getCarrierId());
        $this->assertSame($s->getHomeTeam()->getRerolls() - 1, $r->getNewState()->getHomeTeam()->getRerolls());
    }

    public function testNepovedeneChyceniPredavkySeZeptaKouce(): void
    {
        $s = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, agility: 3, id: 1)
            ->addPlayer(TeamSide::HOME, 6, 5, agility: 3, id: 2)
            ->withBallCarried(1)
            ->build();
        $r = $this->kouc([1])->resolve($s, ActionType::HAND_OFF, ['playerId' => 1, 'targetId' => 2]);

        $q = $r->getNewState()->getPendingReroll();
        $this->assertNotNull($q);
        $this->assertSame('catch', $q->getRollType()->value);
        $this->assertSame([6, 5], [$q->getTargetX(), $q->getTargetY()]);

        $r2 = $this->kouc([6])->resolve($r->getNewState(), ActionType::RESOLVE_REROLL, ['choice' => 'team_reroll']);
        $this->assertSame(2, $r2->getNewState()->getBall()->getCarrierId());
        $this->assertFalse($r2->isTurnover());
    }
}
