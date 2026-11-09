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
        $this->assertSame(5, $this->resolver()->getInterceptionTarget($s, $s->requirePlayer(10)));
    }

    public function testKazdaSouperovaZonaNaZachycujicimPridavaJedna(): void
    {
        $s = (new GameStateBuilder())
            ->addPlayer(TeamSide::AWAY, 7, 5, agility: 4, id: 10)
            ->addPlayer(TeamSide::HOME, 7, 4, id: 1)
            ->build();

        $this->assertSame(6, $this->resolver()->getInterceptionTarget($s, $s->requirePlayer(10)));
    }

    public function testNervesOfSteelZonyIgnoruje(): void
    {
        $s = (new GameStateBuilder())
            ->addPlayer(TeamSide::AWAY, 7, 5, agility: 4, skills: [SkillName::NervesOfSteel], id: 10)
            ->addPlayer(TeamSide::HOME, 7, 4, id: 1)
            ->build();

        $this->assertSame(5, $this->resolver()->getInterceptionTarget($s, $s->requirePlayer(10)));
    }

    public function testDestPridavaJedna(): void
    {
        $s = (new GameStateBuilder())
            ->withWeather(Weather::POURING_RAIN)
            ->addPlayer(TeamSide::AWAY, 7, 5, agility: 4, id: 10)
            ->build();

        $this->assertSame(6, $this->resolver()->getInterceptionTarget($s, $s->requirePlayer(10)));
    }

    public function testDisturbingPresenceDoTriPoliPridavaJedna(): void
    {
        $s = (new GameStateBuilder())
            ->addPlayer(TeamSide::AWAY, 7, 5, agility: 4, id: 10)
            ->addPlayer(TeamSide::HOME, 7, 8, skills: [SkillName::DisturbingPresence], id: 1)
            ->build();

        $this->assertSame(6, $this->resolver()->getInterceptionTarget($s, $s->requirePlayer(10)));
    }

    public function testExtraArmsUbiraJedna(): void
    {
        $s = (new GameStateBuilder())
            ->addPlayer(TeamSide::AWAY, 7, 5, agility: 3, skills: [SkillName::ExtraArms], id: 10)
            ->build();

        // AG3 = 4+, -2 = 6+, Extra Arms +1 k hodu = 5+
        $this->assertSame(5, $this->resolver()->getInterceptionTarget($s, $s->requirePlayer(10)));
    }

    public function testVeryLongLegsUbiraJedna(): void
    {
        $s = (new GameStateBuilder())
            ->addPlayer(TeamSide::AWAY, 7, 5, agility: 3, skills: [SkillName::VeryLongLegs], id: 10)
            ->build();

        $this->assertSame(5, $this->resolver()->getInterceptionTarget($s, $s->requirePlayer(10)));
    }

    public function testZachycujeTenSNejvyssiSanciNePrvniNaDraze(): void
    {
        // r. 1771-1773: "only one player can attempt an interception" -- a vybira
        // ho souperuv kouc. Uzivatel 15.09.: zachycuje ten s NEJVYSSI SANCI.
        // Prvni na draze (6,5) ma AG1 = cil 6+, druhy (8,5) AG4 = cil 5+.
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, agility: 3, id: 1)
            ->addPlayer(TeamSide::HOME, 10, 5, agility: 3, id: 2)
            ->addPlayer(TeamSide::AWAY, 6, 5, agility: 1, id: 10)
            ->addPlayer(TeamSide::AWAY, 8, 5, agility: 4, id: 11)
            ->withBallCarried(1)
            ->build();

        // Hod 5: zachycujici AG4 (5+) USPEJE; prvni na draze (6+) by neuspel.
        $result = (new ActionResolver(new FixedDiceRoller([5, 6, 6])))->resolve($state, ActionType::PASS, [
            'playerId' => 1, 'targetX' => 10, 'targetY' => 5,
        ]);

        $zachyceni = array_values(array_filter($result->getEvents(), static fn($e) => $e->getType() === 'interception'));
        $this->assertCount(1, $zachyceni, 'pokus o intercepci je jen jeden');
        $this->assertSame(11, $zachyceni[0]->getData()['playerId']);
        $this->assertSame(11, $result->getNewState()->getBall()->getCarrierId());
    }

    public function testHracBezZonyNezachycuje(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, agility: 3, id: 1)
            ->addPlayer(TeamSide::HOME, 10, 5, agility: 3, id: 2)
            ->addPlayer(TeamSide::AWAY, 7, 5, agility: 6, id: 10)
            ->withBallCarried(1)
            ->build();
        $state = $state->withPlayer($state->requirePlayer(10)->withLostTacklezones(true));

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

    // ───── P271 (9.11.2026, nalezeno při P264): parita s C++ pass_handler.cpp (opraveno tam 10.08.) ─────
    // r. 1764-1769: zachycovat smí hráč, přes jehož pole přejde (široké) pravítko, a který je blíž k házeči i k cíli,
    //   než je házeč od cíle. PHP bralo jen pole Bresenhamovy čáry. Catch přehodí nepovedené zachycení (r. 7935-7938
    //   „fails to make an interception“) — PHP nepřehazovalo.

    /**
     * @param list<SkillName> $skilly
     * @param list<int> $kostky
     * @return array{0: \App\DTO\GameState, 1: ActionResolver}
     */
    private function prihravka(int $zx, int $zy, array $skilly, array $kostky): array
    {
        $s = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, id: 1)
            ->addPlayer(TeamSide::HOME, 11, 5, id: 2)
            ->addPlayer(TeamSide::AWAY, $zx, $zy, agility: 3, skills: $skilly, id: 10)
            ->withBallCarried(1)
            ->build();
        return [$s, new ActionResolver(new FixedDiceRoller($kostky))];
    }

    /** @return list<string> */
    private function typy(\App\DTO\ActionResult $r): array
    {
        return array_map(fn($e) => $e->getType(), $r->getEvents());
    }

    public function testP271ZachycujeIHracVedleCary(): void
    {
        // (8,6) je jedno pole vedle čáry (5,5)→(11,5): pravítko přes něj přejde; Bresenham ne. Zachycení 7-3+2 = 6.
        [$s, $r] = $this->prihravka(8, 6, [], [6, 6, 6, 6]);
        $v = $r->resolve($s, ActionType::PASS, ['playerId' => 1, 'targetX' => 11, 'targetY' => 5]);
        $this->assertContains('interception', $this->typy($v));
        $this->assertTrue($v->isTurnover());
        $this->assertSame(10, $v->getNewState()->getBall()->getCarrierId());
    }

    public function testP271ZaCilemNezachycuje(): void
    {
        // (12,6): od házeče 7 ≥ délka hodu 6 ⇒ nesmí (r. 1766-1767)
        [$s, $r] = $this->prihravka(12, 6, [], [6, 6, 6, 6]);
        $v = $r->resolve($s, ActionType::PASS, ['playerId' => 1, 'targetX' => 11, 'targetY' => 5]);
        $this->assertNotContains('interception', $this->typy($v));
    }

    public function testP271CatchPrehodiNepovedeneZachyceni(): void
    {
        [$s, $r] = $this->prihravka(8, 5, [SkillName::Catch], [1, 6, 6, 6, 6]);
        $v = $r->resolve($s, ActionType::PASS, ['playerId' => 1, 'targetX' => 11, 'targetY' => 5]);
        $this->assertTrue($v->isTurnover());
        $this->assertSame(10, $v->getNewState()->getBall()->getCarrierId());
    }
}
