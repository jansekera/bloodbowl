<?php

declare(strict_types=1);

namespace App\Tests\Engine;

use App\Engine\ActionResolver;
use App\Engine\FixedDiceRoller;
use App\Enum\ActionType;
use App\Enum\TeamSide;
use PHPUnit\Framework\TestCase;

/**
 * P96 (kniha, 30.09.2026) ověřeno 19.11.2026: přehoz úhybu kouče-člověka během DOJITÍ v blitzu blitz neukončí —
 * po přehozu (P294: přehrání akce s odpovědí) hráč dojde zbytek cesty a rána padne. Ř. 347-350: Blitz = pohyb
 * a jeden blok kdykoli během pohybu; přehoz je volba kouče (ř. 929-931). Dřív `BlitzHandler` při čekajícím
 * přehozu vrátil úspěch a `RerollHandler::applySuccess` posunul hráče jen o jedno pole, rána se nekonala.
 */
final class BlitzPrehozDojitiTest extends TestCase
{
    /** @param list<int> $kostky */
    private function kouc(array $kostky): ActionResolver
    {
        $r = new ActionResolver(new FixedDiceRoller($kostky));
        $r->setHumanSides([TeamSide::HOME]);

        return $r;
    }

    public function testPoPrehozuUhybuVDojitiBlitzRanaProbehne(): void
    {
        // útočník (3,5) označený hráčem (3,4); cíl (6,5) ⇒ dojití přes (4,6) na (5,5) — první krok je úhyb
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 3, 5, id: 1)
            ->addPlayer(TeamSide::AWAY, 3, 4, id: 2)
            ->addPlayer(TeamSide::AWAY, 6, 5, id: 3)
            ->withBallOffPitch()
            ->build();

        $r1 = $this->kouc([1])->resolve($state, ActionType::BLITZ, ['playerId' => 1, 'targetId' => 3]);
        $this->assertNotNull($r1->getNewState()->getPendingReroll(), 'nepovedený úhyb ⇒ otázka na přehoz');

        // přehoz úhybu 6, kostka bloku 6 (POW), brnění 3+3
        $r2 = $this->kouc([6, 6, 3, 3])->resolve($r1->getNewState(), ActionType::RESOLVE_REROLL, ['choice' => 'team_reroll']);

        $typy = array_map(static fn($e) => $e->getType(), $r2->getEvents());
        $this->assertContains('block', $typy, 'rána blitzu po přehozu padla');
        $poz = $r2->getNewState()->requirePlayer(1)->requirePosition();
        $this->assertSame([5, 5], [$poz->getX(), $poz->getY()]);
    }
}
