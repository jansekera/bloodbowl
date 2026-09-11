<?php
declare(strict_types=1);

namespace App\Tests\Engine;

use App\Engine\ActionResolver;
use App\Engine\FixedDiceRoller;
use App\Enum\ActionType;
use App\Enum\PlayerState;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use PHPUnit\Framework\TestCase;

/**
 * ⛔ VADA (11.09.2026, PHP15c): bonus +2 k hodu Really Stupid dával
 *    JAKÝKOLI spoluhráč vedle — `hasAdjacentTeammate` nekontrolovalo nic.
 *
 * ⭐ `rules_bb2016.txt` r. 8393-8395: „If there are one or more players from
 *    the same team **STANDING** adjacent to the Really Stupid player's
 *    square, **and who aren't Really Stupid**, then add 2 to the D6 roll."
 *    ⇒ ležící soused se nepočítá, a soused, který je sám Really Stupid,
 *    taky ne — jinak se dva Really Stupid navzájem podpírají.
 *
 * ⚠️ C++ to má správně (`big_guy_handler.cpp:44-62`).
 */
final class ReallyStupidSupportTest extends TestCase
{
    /** Hod 3: projde při prahu 2+ (s bonusem), padne při 4+ (bez bonusu). */
    private const ROLL_BETWEEN = 3;

    private function move(\App\DTO\GameState $state): \App\DTO\ActionResult
    {
        $resolver = new ActionResolver(new FixedDiceRoller([self::ROLL_BETWEEN]));

        return $resolver->resolve($state, ActionType::MOVE, [
            'playerId' => 1, 'x' => 5, 'y' => 8,
        ]);
    }

    private function base(): GameStateBuilder
    {
        return (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, movement: 6, id: 1,
                        skills: [SkillName::ReallyStupid])
            ->addPlayer(TeamSide::AWAY, 20, 7, id: 2);
    }

    public function testStandingOrdinaryTeammateGivesTheBonus(): void
    {
        // Kontrolní bod: takhle bonus vzniknout MÁ.
        $state = $this->base()
            ->addPlayer(TeamSide::HOME, 6, 7, id: 3)
            ->withBallOffPitch()
            ->build();

        $this->assertTrue($state->getPlayer(3)->getState()->canAct(),
            'fixtura je vadná: soused nestojí');

        $result = $this->move($state);

        // Cíl pohybu je (5,8) -- úspěch se pozná tím, že tam hráč stojí.
        $this->assertSame(8, $result->getNewState()->getPlayer(1)->getPosition()->getY(),
            'trojka s bonusem (práh 2+) projít MĚLA — hráč se měl pohnout');
        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertNotContains('really_stupid', $types,
            's bonusem se kontrola ozvat nemá');
    }

    public function testProneTeammateGivesNoBonus(): void
    {
        $state = $this->base()
            ->addPlayer(TeamSide::HOME, 6, 7, id: 3)
            ->withBallOffPitch()
            ->build();
        $state = $state->withPlayer($state->getPlayer(3)->withState(PlayerState::PRONE));

        $this->assertFalse($state->getPlayer(3)->getState()->canAct(),
            'fixtura je vadná: soused pořád stojí');

        $result = $this->move($state);

        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('really_stupid', $types,
            'ležící soused bonus dávat NESMÍ — trojka měla padnout (r. 8393-8395)');
    }

    public function testReallyStupidTeammateGivesNoBonus(): void
    {
        // ⭐ Tohle je ten případ, kvůli kterému to C++ opravovalo:
        //    dva Really Stupid se navzájem podpírali.
        $state = $this->base()
            ->addPlayer(TeamSide::HOME, 6, 7, id: 3, skills: [SkillName::ReallyStupid])
            ->withBallOffPitch()
            ->build();

        $this->assertTrue($state->getPlayer(3)->hasSkill(SkillName::ReallyStupid),
            'fixtura je vadná: soused není Really Stupid');
        $this->assertTrue($state->getPlayer(3)->getState()->canAct(),
            'fixtura je vadná: soused nestojí, padlo by to z jiného důvodu');

        $result = $this->move($state);

        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('really_stupid', $types,
            'Really Stupid soused bonus dávat NESMÍ — dva se nepodpírají');
    }
}
