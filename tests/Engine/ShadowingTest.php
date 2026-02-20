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
    public function testShadowingFollowsOnHighRoll(): void
    {
        // Mover at (5,5) dodges to (5,4), shadower at (6,5) with equal MA
        // Shadower needs roll + MA - MA >= 6, so roll >= 6
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, movement: 6, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, movement: 6, skills: [SkillName::Shadowing], id: 2)
            ->withBallOffPitch()
            ->build();

        // Dodge target = 3+ (1 TZ from shadower leaving), roll 4 → success
        // Shadowing: roll 6 + 6 - 6 = 6 ≥ 6 → follows
        $dice = new FixedDiceRoller([4, 6]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::MOVE, ['playerId' => 1, 'x' => 5, 'y' => 4]);

        $this->assertFalse($result->isTurnover());
        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('shadowing', $types);

        // Shadower moved to vacated square (5,5)
        $shadower = $result->getNewState()->getPlayer(2);
        $this->assertEquals(5, $shadower->getPosition()->getX());
        $this->assertEquals(5, $shadower->getPosition()->getY());
    }

    public function testShadowingFailsOnLowRoll(): void
    {
        // Same setup, but roll 5 → 5 + 6 - 6 = 5 < 6 → fails
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, movement: 6, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, movement: 6, skills: [SkillName::Shadowing], id: 2)
            ->withBallOffPitch()
            ->build();

        $dice = new FixedDiceRoller([4, 5]); // dodge success, shadowing fails
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::MOVE, ['playerId' => 1, 'x' => 5, 'y' => 4]);

        $this->assertFalse($result->isTurnover());
        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('shadowing', $types);

        // Shadower stays at original position
        $shadower = $result->getNewState()->getPlayer(2);
        $this->assertEquals(6, $shadower->getPosition()->getX());
        $this->assertEquals(5, $shadower->getPosition()->getY());
    }

    public function testShadowingHigherMAHelps(): void
    {
        // Shadower MA 7, mover MA 5 → need roll + 7 - 5 >= 6 → roll >= 4
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, movement: 5, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, movement: 7, skills: [SkillName::Shadowing], id: 2)
            ->withBallOffPitch()
            ->build();

        // Dodge roll 4 → success, shadowing roll 4 + 7 - 5 = 6 ≥ 6 → follows
        $dice = new FixedDiceRoller([4, 4]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::MOVE, ['playerId' => 1, 'x' => 5, 'y' => 4]);

        $shadower = $result->getNewState()->getPlayer(2);
        $this->assertEquals(5, $shadower->getPosition()->getX());
        $this->assertEquals(5, $shadower->getPosition()->getY());
    }

    public function testShadowingLowerMAMakesItHarder(): void
    {
        // Shadower MA 5, mover MA 7 → need roll + 5 - 7 >= 6 → roll >= 8 (impossible on D6)
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, movement: 7, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, movement: 5, skills: [SkillName::Shadowing], id: 2)
            ->withBallOffPitch()
            ->build();

        // Dodge roll 4 → success, shadowing roll 6 + 5 - 7 = 4 < 6 → fails
        $dice = new FixedDiceRoller([4, 6]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::MOVE, ['playerId' => 1, 'x' => 5, 'y' => 4]);

        $shadower = $result->getNewState()->getPlayer(2);
        $this->assertEquals(6, $shadower->getPosition()->getX());
        $this->assertEquals(5, $shadower->getPosition()->getY());
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
        $dice = new FixedDiceRoller([1, 2, 2]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::MOVE, ['playerId' => 1, 'x' => 5, 'y' => 4]);

        $this->assertTrue($result->isTurnover());
        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertNotContains('shadowing', $types);
    }
}
