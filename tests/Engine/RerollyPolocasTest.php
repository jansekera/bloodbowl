<?php

declare(strict_types=1);

namespace App\Tests\Engine;

use App\Engine\FixedDiceRoller;
use App\Engine\GameFlowResolver;
use App\Enum\TeamSide;
use PHPUnit\Framework\TestCase;

/**
 * ⭐ TYMOVE PREHOZY SE O POLOCASE OBNOVUJI (16.09.2026).
 *
 * `rules_bb2016.txt` r. 941-943: "When there are no markers left the coach may not use
 * any more team re-rolls that half. AT HALF TIME the two teams get a chance to rest and
 * recuperate, and so their team re-rolls are RESTORED TO THEIR STARTING LEVEL."
 * ⛔ Engine je neobnovoval -- tym, ktery je vycerpal v prvnim polocase, hral druhy bez nich.
 */
final class RerollyPolocasTest extends TestCase
{
    private function stav(int $zbyva): \App\DTO\GameState
    {
        $s = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, id: 1)
            ->addPlayer(TeamSide::AWAY, 20, 7, id: 2)
            ->build();

        return $s
            ->withTeamState(TeamSide::HOME, $s->getTeamState(TeamSide::HOME)->withRerolls($zbyva))
            ->withTeamState(TeamSide::AWAY, $s->getTeamState(TeamSide::AWAY)->withRerolls($zbyva));
    }

    public function testPolocasVratiPrehozyNaVychoziPocet(): void
    {
        // ⚠️ Fixtura ma ruzne pocty: domaci 3, hoste 2 -- kazdy se vraci na SVUJ vychozi.
        $r = (new GameFlowResolver(new FixedDiceRoller([])))->resolveHalfTime($this->stav(0));

        foreach ([TeamSide::HOME, TeamSide::AWAY] as $strana) {
            $tym = $r['state']->getTeamState($strana);
            $this->assertSame($tym->getRerollsStart(), $tym->getRerolls(), $strana->value);
            $this->assertGreaterThan(0, $tym->getRerolls(), $strana->value);
        }
    }

    public function testPoTouchdownuSePrehozyNEobnovuji(): void
    {
        // r. 941: obnova je vyslovne "at half time", ne po kazdem touchdownu.
        $r = (new GameFlowResolver(new FixedDiceRoller([])))->resolvePostTouchdown($this->stav(0));

        $this->assertSame(0, $r['state']->getTeamState(TeamSide::HOME)->getRerolls());
    }
}
