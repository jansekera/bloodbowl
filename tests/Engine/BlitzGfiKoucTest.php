<?php

declare(strict_types=1);

namespace App\Tests\Engine;

use App\Engine\ActionResolver;
use App\Engine\FixedDiceRoller;
use App\Enum\ActionType;
use App\Enum\TeamSide;
use PHPUnit\Framework\TestCase;

/**
 * P294 /simplify dávka 3 (17.11.2026): GFI, kterým se v blitzu platí rána (ř. 1701), se kouče-člověka ptá na přehoz
 * jako GFI pohybu (ř. 929-931: přehoz je volba kouče). Dřív vlastní kopie žebříčku v `BlockHandler::payBlitzBlock`
 * přehazovala i za člověka.
 */
final class BlitzGfiKoucTest extends TestCase
{
    private function blitz(): \App\DTO\GameState
    {
        $s = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, id: 2)
            ->withBallOffPitch()
            ->build();

        return $s->withPlayer($s->requirePlayer(1)->withMovementRemaining(0));   // rána stojí GFI
    }

    /** @param list<int> $kostky */
    private function kouc(array $kostky): ActionResolver
    {
        $r = new ActionResolver(new FixedDiceRoller($kostky));
        $r->setHumanSides([TeamSide::HOME]);

        return $r;
    }

    public function testNepovedeneGfiBlitzuSeZeptaKouce(): void
    {
        $r = $this->kouc([1])->resolve($this->blitz(), ActionType::BLITZ, ['playerId' => 1, 'targetId' => 2]);

        $q = $r->getNewState()->getPendingReroll();
        $this->assertNotNull($q, 'kouč dostane otázku');
        $this->assertSame('gfi', $q->getRollType()->value);
        $this->assertSame([5, 5], [$q->getTargetX(), $q->getTargetY()]);
        $this->assertFalse($r->isTurnover(), 'pád ještě neplatí');
    }

    public function testTymovyPrehozGfiBlitzuRanaProbehne(): void
    {
        $s = $this->kouc([1])->resolve($this->blitz(), ActionType::BLITZ, ['playerId' => 1, 'targetId' => 2])->getNewState();
        // přehoz GFI 4 projde; kostka bloku a dál výplň
        $r = $this->kouc(array_merge([4], array_fill(0, 12, 3)))->resolve($s, ActionType::RESOLVE_REROLL, ['choice' => 'team_reroll']);

        $this->assertFalse($r->getNewState()->getHomeTeam()->canUseReroll());
        $this->assertNotContains('player_fell', array_map(static fn($e) => $e->getType(), array_filter($r->getEvents(), static fn($e) => ($e->getData()['playerId'] ?? null) === 1)), 'útočník nespadl');
    }
}
