<?php

declare(strict_types=1);

namespace App\Tests\Engine;

use App\Engine\TacklezoneCalculator;
use App\Enum\PlayerState;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use App\ValueObject\Position;
use PHPUnit\Framework\TestCase;

/**
 * ⭐ DISTURBING PRESENCE PUSOBI I LEZICI (15.09.2026).
 *
 * `rules_bb2016.txt` r. 8054-8058: "any player must subtract 1 from the D6 when
 * they pass, intercept or catch for each opposing player with Disturbing
 * Presence that is within three squares of them, EVEN IF THE DISTURBING
 * PRESENCE PLAYER IS PRONE OR STUNNED."
 * ⛔ Do 15.09. `countDisturbingPresence()` chtel `canAct()` = jen STANDING.
 */
final class DisturbingPresenceRulesTest extends TestCase
{
    private function pocet(PlayerState $stav, int $vzdalenost): int
    {
        $s = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, id: 1)
            ->addPlayer(TeamSide::AWAY, 5 + $vzdalenost, 5, skills: [SkillName::DisturbingPresence], id: 2)
            ->build();
        $s = $s->withPlayer($s->getPlayer(2)->withState($stav));

        return (new TacklezoneCalculator())->countDisturbingPresence($s, new Position(5, 5), TeamSide::HOME);
    }

    public function testStojiciDoTriPoliSePocita(): void
    {
        $this->assertSame(1, $this->pocet(PlayerState::STANDING, 3));
    }

    public function testLeziciDoTriPoliSePocita(): void
    {
        $this->assertSame(1, $this->pocet(PlayerState::PRONE, 2));
    }

    public function testOmracenyDoTriPoliSePocita(): void
    {
        $this->assertSame(1, $this->pocet(PlayerState::STUNNED, 1));
    }

    public function testPozitivniKontrolaNaCtyriPoleSeNepocita(): void
    {
        $this->assertSame(0, $this->pocet(PlayerState::STANDING, 4));
    }
}
