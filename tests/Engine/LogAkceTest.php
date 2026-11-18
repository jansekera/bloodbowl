<?php

declare(strict_types=1);

namespace App\Tests\Engine;

use App\DTO\GameEvent;
use App\Engine\ActionResolver;
use App\Engine\FixedDiceRoller;
use App\Enum\ActionType;
use App\Enum\TeamSide;
use PHPUnit\Framework\TestCase;

/**
 * P306 A1 (18.11.2026): otázka kouči uprostřed akce ukáže průběh celé akce (log podle vzniku událostí), i když ji
 * položí hod hluboko v `BallResolver`. OPRAVENO — dřív otázka nesla jen události, které znal kód, jenž ji položil
 * (a ručně přidané obalem `sPredchozimi` na 8 místech); u chytání odrazu po bloku kouč viděl jen „Catch failed“
 * bez bloku, pádu a odrazu.
 */
final class LogAkceTest extends TestCase
{
    /** @param list<int> $kostky */
    private function kouc(array $kostky): ActionResolver
    {
        $r = new ActionResolver(new FixedDiceRoller($kostky));
        $r->setHumanSides([TeamSide::HOME]);

        return $r;
    }

    /** Blok srazí nosiče, míč odskočí ke spoluhráči útočníka a ten ho nechytí (ř. 929-931: přehoz smí na každý hod). */
    private function blokNaNosice(): \App\DTO\GameState
    {
        return (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, id: 2)
            ->addPlayer(TeamSide::HOME, 8, 4, agility: 3, id: 3)
            ->withBallCarried(2)
            ->build();
    }

    /**
     * @param list<GameEvent> $e
     * @return list<string>
     */
    private static function typy(array $e): array
    {
        return array_map(static fn(GameEvent $x): string => $x->getType(), $e);
    }

    public function testOtazkaNaChytaniOdrazuPoBlokuUkazeCelyPrubeh(): void
    {
        // POW, brnění 1+1 drží, odraz na východ (8,4), chytání 1
        $q = $this->kouc([6, 1, 1, 3, 1])->resolve($this->blokNaNosice(), ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);

        $this->assertSame('catch', $q->getNewState()->getPendingReroll()?->getRollType(), 'fixtura: otázka na chytání');
        $this->assertSame(
            ['block', 'push', 'follow_up', 'player_fell', 'armour_roll', 'ball_bounce', 'catch'],
            self::typy($q->getEvents()),
            'kouč u otázky vidí, proč míč letí k jeho hráči',
        );
    }

    public function testPoOdpovediSeNicNeopakujeANicNechybi(): void
    {
        $q = $this->kouc([6, 1, 1, 3, 1])->resolve($this->blokNaNosice(), ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);
        $r = $this->kouc([6])->resolve($q->getNewState(), ActionType::RESOLVE_REROLL, ['choice' => 'team_reroll']);

        $this->assertSame(['reroll', 'catch'], self::typy($r->getEvents()));
        $this->assertSame(3, $r->getNewState()->getBall()->getCarrierId(), 'přehoz chytil');
    }

    public function testLogBereUdalostiVPoradiVznikuABezVypoctuNanecisto(): void
    {
        GameEvent::zacitLog();
        try {
            $a = GameEvent::turnover('a');
            GameEvent::bezLogu(static fn(): GameEvent => GameEvent::turnover('nanečisto'));
            $b = GameEvent::turnover('b');
            $this->assertSame([$a, $b], GameEvent::log());
        } finally {
            GameEvent::ukoncitLog();
        }
    }

    public function testAkceSeDoAkceNevnoruje(): void
    {
        GameEvent::zacitLog();
        try {
            $this->expectException(\LogicException::class);
            GameEvent::zacitLog();
        } finally {
            GameEvent::ukoncitLog();
        }
    }
}
