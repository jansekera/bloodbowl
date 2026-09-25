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
 * ⭐⭐ FAUL PODLE PRAVIDEL (16.09.2026).
 *
 * `rules_bb2016.txt` r. 1843-1853:
 *   "makes an Armour roll for him. Other players that are adjacent to the victim must
 *    assist the player making the foul, and each extra player adds 1 to the Armour roll.
 *    Defending players adjacent to the fouler must also give assists ... Each defensive
 *    assist modifies the Armour roll by -1 per assist. No player from either side may
 *    assist a foul if they are in the tackle zone of an opposing player, do not have
 *    their tackle zones, or are not standing."
 *   r. 8161 Guard: "This skill may not be used to assist a foul."
 *   r. 1878-1883: dublet ⇒ vylouceni A TURNOVER.
 *
 * ⛔ Engine do 16.09.: pausalni +1 "prone bonus", zadne asistence, vylouceni bez turnoveru.
 */
final class FoulRulesTest extends TestCase
{
    /**
     * Faulujici HOME id 1 na (5,5), obet AWAY id 2 na (6,5) lezici.
     * @param list<array{side: TeamSide, x: int, y: int, id: int}> $dalsi
     */
    private function stav(array $dalsi = []): \App\DTO\GameState
    {
        $b = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, armour: 8, id: 2);
        foreach ($dalsi as $p) {
            $b = $b->addPlayer($p['side'], $p['x'], $p['y'], id: $p['id']);
        }
        $s = $b->build();

        return $s->withPlayer($s->requirePlayer(2)->withState(PlayerState::PRONE));
    }

    /**
     * @param list<int> $kostky
     */
    private function faul(\App\DTO\GameState $s, array $kostky): \App\DTO\ActionResult
    {
        return (new ActionResolver(new FixedDiceRoller($kostky)))
            ->resolve($s, ActionType::FOUL, ['playerId' => 1, 'targetId' => 2]);
    }

    /**
     * @param list<\App\DTO\GameEvent> $events
     */
    private function zbrojProlomena(array $events): bool
    {
        foreach ($events as $e) {
            if ($e->getType() === 'foul') {
                return (bool) ($e->getData()['armourBroken'] ?? false);
            }
        }
        return false;
    }

    public function testBezAsistenciSeNepricitaNic(): void
    {
        // AV 8, hod 4+4 = 8 => NEprolomeno. S byvalym pausalnim +1 by bylo 9 => prolomeno.
        $r = $this->faul($this->stav(), [4, 4, 1, 1]);
        $this->assertFalse($this->zbrojProlomena($r->getEvents()));
    }

    public function testUtocnaAsistencePridavaJedna(): void
    {
        // Spoluhrac faulujiciho vedle OBETI, mimo zony souperu => +1 => 9 > 8.
        $r = $this->faul($this->stav([['side' => TeamSide::HOME, 'x' => 6, 'y' => 4, 'id' => 3]]), [4, 4, 1, 1]);
        $this->assertTrue($this->zbrojProlomena($r->getEvents()));
    }

    public function testObrannaAsistenceUbiraJedna(): void
    {
        // Soupere vedle FAULUJICIHO (5,4) => -1. Utocna asistence (6,4) => +1. Netto 0 => 8, neprolomeno.
        $r = $this->faul($this->stav([
            ['side' => TeamSide::HOME, 'x' => 6, 'y' => 4, 'id' => 3],
            ['side' => TeamSide::AWAY, 'x' => 5, 'y' => 4, 'id' => 4],
        ]), [4, 4, 1, 1]);
        $this->assertFalse($this->zbrojProlomena($r->getEvents()));
    }

    public function testVylouceniPriDubletuJeTurnover(): void
    {
        // r. 1880-1881: "In addition, his team suffers a turnover and their turn ends immediately."
        $r = $this->faul($this->stav(), [3, 3, 1, 1]);

        $typy = array_map(static fn($e) => $e->getType(), $r->getEvents());
        $this->assertContains('ejection', $typy);
        $this->assertTrue($r->isTurnover(), 'vylouceni faulujiciho je turnover');
    }
}
