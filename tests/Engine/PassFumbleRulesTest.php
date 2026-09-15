<?php

declare(strict_types=1);

namespace App\Tests\Engine;

use App\DTO\GameState;
use App\Engine\ActionResolver;
use App\Engine\FixedDiceRoller;
use App\Enum\ActionType;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use PHPUnit\Framework\TestCase;

/**
 * ⭐⭐ FUMBLE PODLE PRAVIDEL (15.09.2026).
 *
 * `rules_bb2016.txt` r. 1740-1745: "if the D6 roll for a pass is 1 or less
 * BEFORE OR AFTER MODIFICATION, then the thrower has fumbled".
 * ⛔ Engine do 15.09. znal fumble JEN jako prirozenou 1 (`$roll === 1`) --
 *   dlouha bomba s hodem 2 nebo 3 byla "nepresna", ne fumble.
 *
 * r. 8440-8443 Safe Throw: "if this player fumbles a pass on any roll other
 * than a natural 1 then he manages to keep hold of the ball instead of
 * suffering a fumble and the team does not suffer a turnover."
 */
final class PassFumbleRulesTest extends TestCase
{
    /** @param list<SkillName> $skills */
    private function stav(array $skills = [], bool $dveZony = false, int $cilX = 17): GameState
    {
        $b = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, agility: 3, skills: $skills, id: 1)
            ->addPlayer(TeamSide::HOME, $cilX, 5, agility: 3, id: 2)
            ->withBallCarried(1);
        if ($dveZony) {
            $b = $b->addPlayer(TeamSide::AWAY, 5, 4, id: 10)->addPlayer(TeamSide::AWAY, 5, 6, id: 11);
        }
        $s = $b->build();

        return $s->withTeamState(TeamSide::HOME, $s->getTeamState(TeamSide::HOME)->withRerolls(0));
    }

    private function prvniVysledekHodu(array $events): ?string
    {
        foreach ($events as $e) {
            if ($e->getType() === 'pass') {
                return $e->getData()['result'];
            }
        }
        return null;
    }

    public function testDlouhaBombaHod3JeFumble(): void
    {
        // Long bomb (12 poli) = -2 k hodu. Hod 3 - 2 = 1 => FUMBLE.
        $r = (new ActionResolver(new FixedDiceRoller([3, 1, 1, 1, 1])))->resolve($this->stav(), ActionType::PASS, [
            'playerId' => 1, 'targetX' => 17, 'targetY' => 5,
        ]);

        $this->assertSame('fumble', $this->prvniVysledekHodu($r->getEvents()));
        $this->assertTrue($r->isTurnover());
    }

    public function testPozitivniKontrolaDlouhaBombaHod4NeniFumble(): void
    {
        // Hod 4 - 2 = 2 => neni fumble (jen nepresna, cil 6+).
        $r = (new ActionResolver(new FixedDiceRoller(array_fill(0, 20, 4))))->resolve($this->stav(), ActionType::PASS, [
            'playerId' => 1, 'targetX' => 17, 'targetY' => 5,
        ]);

        $this->assertSame('inaccurate', $this->prvniVysledekHodu($r->getEvents()));
    }

    public function testQuickPassSeDvemaZonamaHod2JeFumble(): void
    {
        // Quick pass +1, dve zony -2 => celkem -1. Hod 2 - 1 = 1 => FUMBLE.
        // (Souperi stoji vedle hazece, ne na draze prihravky na (7,5).)
        $r = (new ActionResolver(new FixedDiceRoller([2, 1, 1, 1, 1])))->resolve(
            $this->stav(dveZony: true, cilX: 7),
            ActionType::PASS,
            ['playerId' => 1, 'targetX' => 7, 'targetY' => 5],
        );

        $this->assertSame('fumble', $this->prvniVysledekHodu($r->getEvents()));
        $this->assertTrue($r->isTurnover());
    }

    public function testSafeThrowFumbleNaJinemNezPrirozene1MicUdrzi(): void
    {
        $r = (new ActionResolver(new FixedDiceRoller([3])))->resolve($this->stav([SkillName::SafeThrow]), ActionType::PASS, [
            'playerId' => 1, 'targetX' => 17, 'targetY' => 5,
        ]);

        $this->assertFalse($r->isTurnover(), 'Safe Throw: fumble jinak nez prirozenou 1 neni turnover');
        $ball = $r->getNewState()->getBall();
        $this->assertTrue($ball->isHeld());
        $this->assertSame(1, $ball->getCarrierId());
    }

    public function testSafeThrowPrirozena1JeFumblePorad(): void
    {
        $r = (new ActionResolver(new FixedDiceRoller([1, 1, 1, 1, 1])))->resolve($this->stav([SkillName::SafeThrow]), ActionType::PASS, [
            'playerId' => 1, 'targetX' => 17, 'targetY' => 5,
        ]);

        $this->assertTrue($r->isTurnover());
        $this->assertNotSame(1, $r->getNewState()->getBall()->getCarrierId());
    }
}
