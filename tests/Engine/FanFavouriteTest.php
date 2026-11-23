<?php

declare(strict_types=1);

namespace App\Tests\Engine;

use App\Engine\BallResolver;
use App\Engine\FixedDiceRoller;
use App\Engine\InjuryResolver;
use App\Engine\KickoffResolver;
use App\Engine\ScatterCalculator;
use App\Engine\TacklezoneCalculator;
use App\Enum\PlayerState;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use PHPUnit\Framework\TestCase;

/**
 * P321 — Fan Favourite (rules_bb2016.txt ř. 8107-8112): každý takový hráč NA HŘIŠTI dá týmu +1 FAME pro výsledky
 * tabulky výkopů. FAME bez něj engine vede jako 0 (Cheering Fans ř. 1309-1314, Throw a Rock ř. 1342-1350,
 * Pitch Invasion ř. 1351-1356).
 */
final class FanFavouriteTest extends TestCase
{
    private function kickoff(FixedDiceRoller $dice): KickoffResolver
    {
        $scatter = new ScatterCalculator();

        return new KickoffResolver($dice, $scatter, new BallResolver($dice, new TacklezoneCalculator(), $scatter), new InjuryResolver());
    }

    private function stav(int $oolGani, bool $naHristi = true): \App\DTO\GameState
    {
        $b = (new GameStateBuilder())->addPlayer(TeamSide::HOME, 5, 5, id: 1)->addPlayer(TeamSide::AWAY, 20, 7, id: 2);
        for ($i = 0; $i < $oolGani; $i++) {
            $b->addPlayer(TeamSide::HOME, 5, 7 + $i, skills: [SkillName::FanFavourite], id: 10 + $i);
        }
        $s = $b->build();
        if (!$naHristi) {
            foreach (range(10, 9 + $oolGani) as $id) {
                $s = $s->withPlayer($s->requirePlayer($id)->withState(PlayerState::OFF_PITCH)->withPosition(null));
            }
        }

        return $s;
    }

    public function testCheeringFansFanFavouriteRozhodneShodu(): void
    {
        $s = $this->stav(1);
        $home = $s->getHomeTeam()->getRerolls();
        $away = $s->getAwayTeam()->getRerolls();
        // 2+4 = 6 Cheering; D3 obou 3 — bez FAME shoda (oba), s Fan Favourite domácí 4 > 3
        $r = $this->kickoff(new FixedDiceRoller([2, 4, 5, 6]))->resolveKickoffTable($s, TeamSide::HOME);

        $this->assertSame($home + 1, $r['state']->getHomeTeam()->getRerolls());
        $this->assertSame($away, $r['state']->getAwayTeam()->getRerolls());
    }

    public function testMimoHristeSeNepocita(): void
    {
        $s = $this->stav(1, naHristi: false);
        $away = $s->getAwayTeam()->getRerolls();
        $r = $this->kickoff(new FixedDiceRoller([2, 4, 5, 6]))->resolveKickoffTable($s, TeamSide::HOME);

        $this->assertSame($away + 1, $r['state']->getAwayTeam()->getRerolls(), 'v rezervě FAME nedává ⇒ shoda');
    }

    public function testPitchInvasionFameSoupereOmraciNaPetku(): void
    {
        // 6+6 = 12 Pitch Invasion; domácí mají 1 Fan Favourite ⇒ jejich fanoušci házejí na HOSTY s +1.
        // Pořadí hodů: domácí hráči (1, 10) pak host (2). Domácím 5 a 5 (hosté FAME 0 ⇒ nic), hostovi 5 + 1 = 6.
        $s = $this->stav(1);
        $r = $this->kickoff(new FixedDiceRoller([6, 6, 5, 5, 5]))->resolveKickoffTable($s, TeamSide::HOME);

        $this->assertSame(PlayerState::STANDING, $r['state']->requirePlayer(1)->getState());
        $this->assertSame(PlayerState::STUNNED, $r['state']->requirePlayer(2)->getState());
    }

    public function testPitchInvasionJednickaNikdy(): void
    {
        // 5 Fan Favourite ⇒ FAME 5, ale 1 „before adding FAME will always have no effect“
        $s = $this->stav(5);
        $r = $this->kickoff(new FixedDiceRoller([6, 6, 5, 5, 5, 5, 5, 5, 1]))->resolveKickoffTable($s, TeamSide::HOME);

        $this->assertSame(PlayerState::STANDING, $r['state']->requirePlayer(2)->getState());
    }
}
