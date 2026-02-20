<?php
declare(strict_types=1);

namespace App\Tests\Engine;

use App\DTO\TeamStateDTO;
use App\Engine\ActionResolver;
use App\Engine\FixedDiceRoller;
use App\Enum\ActionType;
use App\Enum\PlayerState;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use PHPUnit\Framework\TestCase;

final class AlwaysHungryTest extends TestCase
{
    public function testAlwaysHungryPassesAndThrowProceeds(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, strength: 5, skills: [SkillName::ThrowTeamMate, SkillName::AlwaysHungry], id: 1)
            ->addPlayer(TeamSide::HOME, 6, 7, strength: 2, skills: [SkillName::RightStuff, SkillName::Stunty], id: 2)
            ->withBallOffPitch()
            ->build();

        // Always Hungry roll = 2 → passes
        // Accuracy roll = 5 → accurate (AG default is 3, short range)
        // Landing roll = 5 → lands safely
        $dice = new FixedDiceRoller([2, 5, 5]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::THROW_TEAM_MATE, [
            'playerId' => 1,
            'targetId' => 2,
            'targetX' => 8,
            'targetY' => 7,
        ]);

        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('always_hungry', $types);
        $this->assertContains('throw_team_mate', $types);

        // Check always_hungry event was successful (not eaten)
        foreach ($result->getEvents() as $event) {
            if ($event->getType() === 'always_hungry') {
                $this->assertFalse($event->getData()['eaten']);
            }
        }
    }

    public function testAlwaysHungryEatsTeammate(): void
    {
        // Use 0 team rerolls so no auto-reroll
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, strength: 5, skills: [SkillName::ThrowTeamMate, SkillName::AlwaysHungry], id: 1)
            ->addPlayer(TeamSide::HOME, 6, 7, strength: 2, skills: [SkillName::RightStuff, SkillName::Stunty], id: 2)
            ->withHomeTeam(TeamStateDTO::create(1, 'Home', 'Human', TeamSide::HOME, 0))
            ->withBallOffPitch()
            ->build();

        // Always Hungry roll = 1 → eats teammate (no rerolls available)
        $dice = new FixedDiceRoller([1]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::THROW_TEAM_MATE, [
            'playerId' => 1,
            'targetId' => 2,
            'targetX' => 8,
            'targetY' => 7,
        ]);

        $this->assertFalse($result->isTurnover()); // Not a turnover

        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('always_hungry', $types);
        $this->assertNotContains('throw_team_mate', $types);

        // Projectile is removed (Injured)
        $projectile = $result->getNewState()->getPlayer(2);
        $this->assertSame(PlayerState::INJURED, $projectile->getState());
        $this->assertNull($projectile->getPosition());
    }

    public function testAlwaysHungryTeamRerollSaves(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, strength: 5, skills: [SkillName::ThrowTeamMate, SkillName::AlwaysHungry], id: 1)
            ->addPlayer(TeamSide::HOME, 6, 7, strength: 2, skills: [SkillName::RightStuff, SkillName::Stunty], id: 2)
            ->withBallOffPitch()
            ->build();

        // Always Hungry roll = 1 → fails
        // Team reroll: new roll = 3 → passes
        // Accuracy roll = 5 → accurate
        // Landing roll = 5 → lands
        $dice = new FixedDiceRoller([1, 3, 5, 5]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::THROW_TEAM_MATE, [
            'playerId' => 1,
            'targetId' => 2,
            'targetX' => 8,
            'targetY' => 7,
        ]);

        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('reroll', $types);
        $this->assertContains('throw_team_mate', $types);

        // Projectile survived
        $projectile = $result->getNewState()->getPlayer(2);
        $this->assertNotSame(PlayerState::INJURED, $projectile->getState());
    }

    public function testAlwaysHungryTeamRerollFailsStillEaten(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, strength: 5, skills: [SkillName::ThrowTeamMate, SkillName::AlwaysHungry], id: 1)
            ->addPlayer(TeamSide::HOME, 6, 7, strength: 2, skills: [SkillName::RightStuff, SkillName::Stunty], id: 2)
            ->withBallOffPitch()
            ->build();

        // Always Hungry roll = 1 → fails
        // Team reroll: new roll = 1 → still fails → eaten
        $dice = new FixedDiceRoller([1, 1]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::THROW_TEAM_MATE, [
            'playerId' => 1,
            'targetId' => 2,
            'targetX' => 8,
            'targetY' => 7,
        ]);

        $this->assertFalse($result->isTurnover());
        $projectile = $result->getNewState()->getPlayer(2);
        $this->assertSame(PlayerState::INJURED, $projectile->getState());
    }

    public function testAlwaysHungryEatsBallCarrier(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, strength: 5, skills: [SkillName::ThrowTeamMate, SkillName::AlwaysHungry], id: 1)
            ->addPlayer(TeamSide::HOME, 6, 7, strength: 2, skills: [SkillName::RightStuff, SkillName::Stunty], id: 2)
            ->withHomeTeam(TeamStateDTO::create(1, 'Home', 'Human', TeamSide::HOME, 0))
            ->withBallCarried(2)
            ->build();

        // Always Hungry roll = 1 → eats teammate who has ball
        // Ball bounces from thrower pos: D8 = 3
        $dice = new FixedDiceRoller([1, 3]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::THROW_TEAM_MATE, [
            'playerId' => 1,
            'targetId' => 2,
            'targetX' => 8,
            'targetY' => 7,
        ]);

        // Ball should have dropped and bounced
        $this->assertFalse($result->getNewState()->getBall()->isHeld());
    }

    public function testAlwaysHungryWithLonerReroll(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, strength: 5, skills: [SkillName::ThrowTeamMate, SkillName::AlwaysHungry, SkillName::Loner], id: 1)
            ->addPlayer(TeamSide::HOME, 6, 7, strength: 2, skills: [SkillName::RightStuff, SkillName::Stunty], id: 2)
            ->withBallOffPitch()
            ->build();

        // Always Hungry roll = 1 → fails
        // Team reroll attempt: Loner check roll = 3 → Loner blocks reroll
        // Eaten!
        $dice = new FixedDiceRoller([1, 3]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::THROW_TEAM_MATE, [
            'playerId' => 1,
            'targetId' => 2,
            'targetX' => 8,
            'targetY' => 7,
        ]);

        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('loner', $types);
        $this->assertContains('always_hungry', $types);

        $projectile = $result->getNewState()->getPlayer(2);
        $this->assertSame(PlayerState::INJURED, $projectile->getState());
    }

    public function testAlwaysHungryRoll1IsBad6IsGood(): void
    {
        // Roll of 1 = eaten, any other = safe
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, strength: 5, skills: [SkillName::ThrowTeamMate, SkillName::AlwaysHungry], id: 1)
            ->addPlayer(TeamSide::HOME, 6, 7, strength: 2, skills: [SkillName::RightStuff, SkillName::Stunty], id: 2)
            ->withBallOffPitch()
            ->build();

        // Roll = 6 → safe
        // Accuracy = 5, landing = 5
        $dice = new FixedDiceRoller([6, 5, 5]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::THROW_TEAM_MATE, [
            'playerId' => 1,
            'targetId' => 2,
            'targetX' => 8,
            'targetY' => 7,
        ]);

        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('always_hungry', $types);
        $this->assertContains('throw_team_mate', $types);
    }
}
