<?php

declare(strict_types=1);

namespace App\Tests\Engine;

use App\Engine\ActionResolver;
use App\Engine\BallResolver;
use App\Engine\FixedDiceRoller;
use App\Engine\PassResolver;
use App\Engine\ScatterCalculator;
use App\Engine\TacklezoneCalculator;
use App\Enum\ActionType;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use App\Enum\Weather;
use PHPUnit\Framework\TestCase;

/**
 * ⭐⭐ INTERCEPCE PODLE PRAVIDEL (15.09.2026).
 *
 * `rules_bb2016.txt`:
 *   r. 1761-1767  zachycovat smi hrac, ktery MA ZONU ZACHYCENI
 *   r. 1800-1802  -2 za pokus, -1 za kazdou souperovu zonu na zachycujicim
 *   r. 1488       Pouring Rain: -1 k intercepci
 *   r. 8050-8058  Disturbing Presence: -1 za kazdeho takoveho soupere do 3 poli
 *   r. 8104-8106  Extra Arms: +1 k intercepci
 *   r. 8315-8317  Nerves of Steel: ignoruje zony i pri intercepci
 *   r. 8655-8657  Very Long Legs: +1 k intercepci
 *
 * ⛔ Do 15.09. engine znal jen -2 a Very Long Legs. Zony, dest, DP, Extra Arms
 *   ani Nerves of Steel se nepocitaly a zachycovat mohl i hrac bez zony.
 */
final class InterceptionRulesTest extends TestCase
{
    private function resolver(): PassResolver
    {
        $dice = new FixedDiceRoller([]);
        $tz = new TacklezoneCalculator();

        return new PassResolver($dice, $tz, new ScatterCalculator(), new BallResolver($dice, $tz, new ScatterCalculator()));
    }

    public function testZakladniCilJeAgPlusDva(): void
    {
        $s = (new GameStateBuilder())
            ->addPlayer(TeamSide::AWAY, 7, 5, agility: 4, id: 10)
            ->build();

        // AG4 = 3+, -2 za pokus = 5+
        $this->assertSame(5, $this->resolver()->getInterceptionTarget($s, $s->getPlayer(10)));
    }

    public function testKazdaSouperovaZonaNaZachycujicimPridavaJedna(): void
    {
        $s = (new GameStateBuilder())
            ->addPlayer(TeamSide::AWAY, 7, 5, agility: 4, id: 10)
            ->addPlayer(TeamSide::HOME, 7, 4, id: 1)
            ->build();

        $this->assertSame(6, $this->resolver()->getInterceptionTarget($s, $s->getPlayer(10)));
    }

    public function testNervesOfSteelZonyIgnoruje(): void
    {
        $s = (new GameStateBuilder())
            ->addPlayer(TeamSide::AWAY, 7, 5, agility: 4, skills: [SkillName::NervesOfSteel], id: 10)
            ->addPlayer(TeamSide::HOME, 7, 4, id: 1)
            ->build();

        $this->assertSame(5, $this->resolver()->getInterceptionTarget($s, $s->getPlayer(10)));
    }

    public function testDestPridavaJedna(): void
    {
        $s = (new GameStateBuilder())
            ->withWeather(Weather::POURING_RAIN)
            ->addPlayer(TeamSide::AWAY, 7, 5, agility: 4, id: 10)
            ->build();

        $this->assertSame(6, $this->resolver()->getInterceptionTarget($s, $s->getPlayer(10)));
    }

    public function testDisturbingPresenceDoTriPoliPridavaJedna(): void
    {
        $s = (new GameStateBuilder())
            ->addPlayer(TeamSide::AWAY, 7, 5, agility: 4, id: 10)
            ->addPlayer(TeamSide::HOME, 7, 8, skills: [SkillName::DisturbingPresence], id: 1)
            ->build();

        $this->assertSame(6, $this->resolver()->getInterceptionTarget($s, $s->getPlayer(10)));
    }

    public function testExtraArmsUbiraJedna(): void
    {
        $s = (new GameStateBuilder())
            ->addPlayer(TeamSide::AWAY, 7, 5, agility: 3, skills: [SkillName::ExtraArms], id: 10)
            ->build();

        // AG3 = 4+, -2 = 6+, Extra Arms +1 k hodu = 5+
        $this->assertSame(5, $this->resolver()->getInterceptionTarget($s, $s->getPlayer(10)));
    }

    public function testVeryLongLegsUbiraJedna(): void
    {
        $s = (new GameStateBuilder())
            ->addPlayer(TeamSide::AWAY, 7, 5, agility: 3, skills: [SkillName::VeryLongLegs], id: 10)
            ->build();

        $this->assertSame(5, $this->resolver()->getInterceptionTarget($s, $s->getPlayer(10)));
    }

    public function testHracBezZonyNezachycuje(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, agility: 3, id: 1)
            ->addPlayer(TeamSide::HOME, 10, 5, agility: 3, id: 2)
            ->addPlayer(TeamSide::AWAY, 7, 5, agility: 6, id: 10)
            ->withBallCarried(1)
            ->build();
        $state = $state->withPlayer($state->getPlayer(10)->withLostTacklezones(true));

        // Kdyby zachycoval, prvni kostka (6) by byla jeho intercepce.
        // Bez nej: presnost 6 = presne, chyt 6 = chyceno.
        $result = (new ActionResolver(new FixedDiceRoller([6, 6])))->resolve($state, ActionType::PASS, [
            'playerId' => 1, 'targetX' => 10, 'targetY' => 5,
        ]);

        $typy = array_map(static fn($e) => $e->getType(), $result->getEvents());
        $this->assertNotContains('interception', $typy, 'hrac bez zony zachycovat nesmi');
        $this->assertSame(2, $result->getNewState()->getBall()->getCarrierId());
    }

    public function testPozitivniKontrolaHracSeZonouZachycovatZkousi(): void
    {
        // Tataz fixtura bez ztracene zony -- tady intercepce NASTAT MUSI,
        // jinak by predchozi test prosel i pri rozbitem mereni.
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, agility: 3, id: 1)
            ->addPlayer(TeamSide::HOME, 10, 5, agility: 3, id: 2)
            ->addPlayer(TeamSide::AWAY, 7, 5, agility: 6, id: 10)
            ->withBallCarried(1)
            ->build();

        $result = (new ActionResolver(new FixedDiceRoller([6, 6])))->resolve($state, ActionType::PASS, [
            'playerId' => 1, 'targetX' => 10, 'targetY' => 5,
        ]);

        $typy = array_map(static fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('interception', $typy);
        $this->assertSame(10, $result->getNewState()->getBall()->getCarrierId());
    }
}
