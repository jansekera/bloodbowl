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

final class ThrowTeamMateTest extends TestCase
{
    public function testAccurateThrowWithSuccessfulLanding(): void
    {
        // Thrower AG3, target at (8,5) short range. Projectile AG3.
        // Accuracy: 7-3-1(short)=3+. Roll 4=accurate.
        // Landing: 7-3=4+. Roll 4=success.
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 5, skills: [SkillName::ThrowTeamMate], id: 1)
            ->addPlayer(TeamSide::HOME, 6, 5, skills: [SkillName::RightStuff], id: 2)
            ->withBallOffPitch()
            ->build();

        $dice = new FixedDiceRoller([4, 4]); // accuracy, landing
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::THROW_TEAM_MATE, [
            'playerId' => 1, 'targetId' => 2, 'targetX' => 8, 'targetY' => 5,
        ]);

        $this->assertFalse($result->isTurnover());
        $newState = $result->getNewState();
        $landed = $newState->getPlayer(2);
        $this->assertSame(8, $landed->getPosition()->getX());
        $this->assertSame(5, $landed->getPosition()->getY());
        $this->assertSame(PlayerState::STANDING, $landed->getState());
    }

    public function testInaccurateThrowScattersAndLands(): void
    {
        // Inaccurate: scatters 1 square from target
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 5, skills: [SkillName::ThrowTeamMate], id: 1)
            ->addPlayer(TeamSide::HOME, 6, 5, skills: [SkillName::RightStuff], id: 2)
            ->withBallOffPitch()
            ->build();

        // Accuracy: 7-3-1=3+. Roll 2=inaccurate.
        // Scatter D8=3 (East), landing at (9,5).
        // Landing: 7-3=4+. Roll 4=success.
        $dice = new FixedDiceRoller([2, 3, 4]); // accuracy, scatter D8, landing
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::THROW_TEAM_MATE, [
            'playerId' => 1, 'targetId' => 2, 'targetX' => 8, 'targetY' => 5,
        ]);

        $this->assertFalse($result->isTurnover());
        $landed = $result->getNewState()->getPlayer(2);
        // Scattered East from (8,5) → (9,5)
        $this->assertSame(9, $landed->getPosition()->getX());
    }

    public function testFumbleScattersFromThrower(): void
    {
        // Fumble (roll=1): scatter 1 from thrower
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 5, skills: [SkillName::ThrowTeamMate], id: 1)
            ->addPlayer(TeamSide::HOME, 6, 5, skills: [SkillName::RightStuff], id: 2)
            ->withBallOffPitch()
            ->build();

        // Fumble roll=1, scatter D8=3 (East)→(6,5), landing: 7-3=4+, roll=1=fail, armor
        $dice = new FixedDiceRoller([1, 3, 1, 2, 1]); // fumble, scatter, landing fail, armor die1, die2
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::THROW_TEAM_MATE, [
            'playerId' => 1, 'targetId' => 2, 'targetX' => 8, 'targetY' => 5,
        ]);

        $this->assertFalse($result->isTurnover()); // TTM itself is not a turnover

        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('throw_team_mate', $types);
        $this->assertContains('ttm_landing', $types);
    }

    public function testFailedLandingCausesProneAndArmor(): void
    {
        // Failed landing roll → prone + armor check
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 5, skills: [SkillName::ThrowTeamMate], id: 1)
            ->addPlayer(TeamSide::HOME, 6, 5, skills: [SkillName::RightStuff], id: 2)
            ->withBallOffPitch()
            ->build();

        // Accurate roll=5, landing: 7-3=4+, roll=2=fail, armor 3+3=6 not > 8
        $dice = new FixedDiceRoller([5, 2, 3, 3]); // accuracy, landing, armor die1, die2
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::THROW_TEAM_MATE, [
            'playerId' => 1, 'targetId' => 2, 'targetX' => 8, 'targetY' => 5,
        ]);

        $landed = $result->getNewState()->getPlayer(2);
        $this->assertSame(PlayerState::PRONE, $landed->getState());

        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('armour_roll', $types);
    }

    public function testThrownPlayerWithBallSuccessful(): void
    {
        // Thrown player carries ball, lands successfully
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 5, skills: [SkillName::ThrowTeamMate], id: 1)
            ->addPlayer(TeamSide::HOME, 6, 5, skills: [SkillName::RightStuff], id: 2)
            ->withBallCarried(2)
            ->build();

        // Accurate roll=4, landing 4+, roll=4
        $dice = new FixedDiceRoller([4, 4]); // accuracy, landing
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::THROW_TEAM_MATE, [
            'playerId' => 1, 'targetId' => 2, 'targetX' => 8, 'targetY' => 5,
        ]);

        $this->assertFalse($result->isTurnover());
        $ball = $result->getNewState()->getBall();
        $this->assertTrue($ball->isHeld());
        $this->assertSame(2, $ball->getCarrierId());
    }

    public function testFailLandingWithBallBounce(): void
    {
        // Fail landing with ball → ball bounces
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 5, skills: [SkillName::ThrowTeamMate], id: 1)
            ->addPlayer(TeamSide::HOME, 6, 5, skills: [SkillName::RightStuff], id: 2)
            ->withBallCarried(2)
            ->build();

        // Accurate roll=4, landing fail roll=1, armor 2+2=4 not > 8, bounce D8=3
        $dice = new FixedDiceRoller([4, 1, 2, 2, 3]); // accuracy, landing, armor, armor, bounce
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::THROW_TEAM_MATE, [
            'playerId' => 1, 'targetId' => 2, 'targetX' => 8, 'targetY' => 5,
        ]);

        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('ball_bounce', $types);
    }

    public function testLandingOnOccupiedSquareScatters(): void
    {
        // Landing on occupied square → scatter to next
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 5, skills: [SkillName::ThrowTeamMate], id: 1)
            ->addPlayer(TeamSide::HOME, 6, 5, skills: [SkillName::RightStuff], id: 2)
            ->addPlayer(TeamSide::AWAY, 8, 5, id: 3) // occupying landing target
            ->withBallOffPitch()
            ->build();

        // Accurate roll=4 → lands on (8,5) occupied → scatter D8=3 (East) → (9,5)
        // Landing: 7-3+1(TZ from player3 at 8,5)=5+, roll=5
        $dice = new FixedDiceRoller([4, 3, 5]); // accuracy, scatter from occupied, landing
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::THROW_TEAM_MATE, [
            'playerId' => 1, 'targetId' => 2, 'targetX' => 8, 'targetY' => 5,
        ]);

        $landed = $result->getNewState()->getPlayer(2);
        $this->assertSame(9, $landed->getPosition()->getX());
    }

    public function testOffPitchCrowdSurf(): void
    {
        // Inaccurate scatter goes off pitch → crowd surf
        // Thrower near edge, target near edge
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 1, 0, strength: 5, skills: [SkillName::ThrowTeamMate], id: 1)
            ->addPlayer(TeamSide::HOME, 2, 0, skills: [SkillName::RightStuff], id: 2)
            ->withBallOffPitch()
            ->build();

        // Inaccurate: roll=2, scatter D8=1 (North) → off pitch
        // Crowd surf injury dice
        $dice = new FixedDiceRoller([2, 1, 3, 3]); // accuracy, scatter D8=N, injury die1, die2
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::THROW_TEAM_MATE, [
            'playerId' => 1, 'targetId' => 2, 'targetX' => 4, 'targetY' => 0,
        ]);

        $this->assertTrue($result->isTurnover());
        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('crowd_surf', $types);
    }

    public function testValidationTargetWithoutRightStuff(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 5, skills: [SkillName::ThrowTeamMate], id: 1)
            ->addPlayer(TeamSide::HOME, 6, 5, id: 2) // no RightStuff
            ->withBallOffPitch()
            ->build();

        $this->expectException(\InvalidArgumentException::class);

        $dice = new FixedDiceRoller([4, 4]);
        $resolver = new ActionResolver($dice);
        $resolver->resolve($state, ActionType::THROW_TEAM_MATE, [
            'playerId' => 1, 'targetId' => 2, 'targetX' => 8, 'targetY' => 5,
        ]);
    }

    public function testValidationNotAdjacent(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 5, skills: [SkillName::ThrowTeamMate], id: 1)
            ->addPlayer(TeamSide::HOME, 8, 5, skills: [SkillName::RightStuff], id: 2) // not adjacent
            ->withBallOffPitch()
            ->build();

        $this->expectException(\InvalidArgumentException::class);

        $dice = new FixedDiceRoller([4, 4]);
        $resolver = new ActionResolver($dice);
        $resolver->resolve($state, ActionType::THROW_TEAM_MATE, [
            'playerId' => 1, 'targetId' => 2, 'targetX' => 10, 'targetY' => 5,
        ]);
    }
}
