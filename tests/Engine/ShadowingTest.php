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

final class ShadowingTest extends TestCase
{
    // ⛔ P85 (30.09.2026): testy nize driv kodovaly JEDNU kostku s obracenym
    //   znamenkem. Pravidlo (`rules_bb2016.txt` r. 8458-8464): 2D6 + MA
    //   uhybajiciho - MA stinoveho, 7 a min = nasleduje.

    private function shadowState(int $moverMa, int $shadowMa): \App\DTO\GameState
    {
        return (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, movement: $moverMa, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, movement: $shadowMa, skills: [SkillName::Shadowing], id: 2)
            ->withBallOffPitch()
            ->build();
    }

    /**
     * @param list<int> $dice
     * @return array{int, int}
     */
    private function shadowerAfter(\App\DTO\GameState $state, array $dice): array
    {
        $resolver = new ActionResolver(new FixedDiceRoller($dice));
        $result = $resolver->resolve($state, ActionType::MOVE, ['playerId' => 1, 'x' => 5, 'y' => 4]);
        $this->assertFalse($result->isTurnover());
        $pos = $result->getNewState()->requirePlayer(2)->requirePosition();
        return [$pos->getX(), $pos->getY()];
    }

    public function testShadowingFollowsOnSevenOrLess(): void
    {
        // uhyb 4 · 3+4 = 7, MA6-MA6 => 7 => nasleduje na uvolnene pole
        $this->assertSame([5, 5], $this->shadowerAfter($this->shadowState(6, 6), [4, 3, 4]));
    }

    public function testShadowingFailsOnEight(): void
    {
        // uhyb 4 · 6+2 = 8 => nenasleduje. Stara: 6 + 6 - 6 = 6 >= 6 => nasledoval.
        $this->assertSame([6, 5], $this->shadowerAfter($this->shadowState(6, 6), [4, 6, 2]));
    }

    public function testAFasterMoverIsStillFollowedOnALowRoll(): void
    {
        // MA7 proti MA5: 2+3 = 5, +7-5 = 7 => nasleduje. Stara mechanika
        // (1 kostka + MA stinoveho - MA uhybajiciho >= 6) tu nenasledovala nikdy.
        $this->assertSame([5, 5], $this->shadowerAfter($this->shadowState(7, 5), [4, 2, 3]));
    }

    public function testAFasterShadowerMissesOnAHighRoll(): void
    {
        // MA5 proti MA7: 5+5 = 10, +5-7 = 8 => nenasleduje. Stara: 5+7-5 = 7 >= 6 => nasledoval.
        $this->assertSame([6, 5], $this->shadowerAfter($this->shadowState(5, 7), [4, 5, 5]));
    }

    public function testShadowingDoesNotTriggerOnFailedDodge(): void
    {
        // Dodge fails → turnover, no shadowing event
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, movement: 6, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, movement: 6, skills: [SkillName::Shadowing], id: 2)
            ->withBallOffPitch()
            ->build();

        // Dodge roll 1 → fails (need 3+), armor 2+2=4 ≤ AV8
        $dice = new FixedDiceRoller([1, 2, 2, 1, 1 /* PHP27: hod na brneni po padu (2 = nikdy neprorazi) */]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::MOVE, ['playerId' => 1, 'x' => 5, 'y' => 4]);

        $this->assertTrue($result->isTurnover());
        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertNotContains('shadowing', $types);
    }
}
