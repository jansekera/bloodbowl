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
        // ⛔ 21.09.2026: i presny hod se resi jako nepresny (r. 8609-8611),
        //   takze pribyly TRI rozptyly (D8=3 vychod).
        // Landing roll = 5 → lands safely
        $dice = new FixedDiceRoller([2, 5, 3, 3, 3, 5]);
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

        // Always Hungry 1 → pokus sezrat; druhy hod 1 → snezen (r. 7786-7792)
        $dice = new FixedDiceRoller([1, 1]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::THROW_TEAM_MATE, [
            'playerId' => 1,
            'targetId' => 2,
            'targetX' => 8,
            'targetY' => 7,
        ]);

        $this->assertFalse($result->isTurnover()); // bez mice to turnover neni (r. 382-384)

        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('always_hungry', $types);
        $this->assertContains('always_hungry_eat', $types);
        $this->assertNotContains('throw_team_mate', $types);

        // Snezeny = MRTVY, bez lekarnika a Regeneration
        $projectile = $result->getNewState()->requirePlayer(2);
        $this->assertSame(PlayerState::DEAD, $projectile->getState());
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
        // Accuracy roll = 5 → accurate, pak 3x rozptyl (r. 8609-8611)
        // Landing roll = 5 → lands
        $dice = new FixedDiceRoller([1, 3, 5, 3, 3, 3, 5]);
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
        $projectile = $result->getNewState()->requirePlayer(2);
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
        // Team reroll: new roll = 1 → pokus sezrat; druhy hod 1 → snezen
        $dice = new FixedDiceRoller([1, 1, 1]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::THROW_TEAM_MATE, [
            'playerId' => 1,
            'targetId' => 2,
            'targetX' => 8,
            'targetY' => 7,
        ]);

        $this->assertFalse($result->isTurnover());
        $projectile = $result->getNewState()->requirePlayer(2);
        $this->assertSame(PlayerState::DEAD, $projectile->getState());
    }

    public function testAlwaysHungryEatsBallCarrier(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, strength: 5, skills: [SkillName::ThrowTeamMate, SkillName::AlwaysHungry], id: 1)
            ->addPlayer(TeamSide::HOME, 6, 7, strength: 2, skills: [SkillName::RightStuff, SkillName::Stunty], id: 2)
            ->withHomeTeam(TeamStateDTO::create(1, 'Home', 'Human', TeamSide::HOME, 0))
            ->withBallCarried(2)
            ->build();

        // Always Hungry 1 → pokus; druhy hod 1 → snezen i s micem.
        // Mic se rozptyli JEDNOU z pole snezeneho (6,7), ne hazece: D8 = 3
        $dice = new FixedDiceRoller([1, 1, 3]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::THROW_TEAM_MATE, [
            'playerId' => 1,
            'targetId' => 2,
            'targetX' => 8,
            'targetY' => 7,
        ]);

        // Mic spadl a odskocil o jedno pole od snezeneho; hozeny s micem neprisel = turnover (r. 382-384)
        $ball = $result->getNewState()->getBall();
        $this->assertFalse($ball->isHeld());
        $this->assertSame(1, $ball->requirePosition()->distanceTo(new \App\ValueObject\Position(6, 7)));
        $this->assertTrue($result->isTurnover());
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
        // => pokus sezrat; druhy hod 1 → snezen
        $dice = new FixedDiceRoller([1, 3, 1]);
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

        $projectile = $result->getNewState()->requirePlayer(2);
        $this->assertSame(PlayerState::DEAD, $projectile->getState());
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
        // Accuracy = 5, pak 3x rozptyl (r. 8609-8611), landing = 5
        $dice = new FixedDiceRoller([6, 5, 3, 3, 3, 5]);
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

    /** Druhy hod 2-6: vysmekne se, hod je fumble -- dopada na sve puvodni pole (r. 7792-7795, 8613). */
    public function testAlwaysHungrySquirmFreeIsFumble(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, strength: 5, skills: [SkillName::ThrowTeamMate, SkillName::AlwaysHungry], id: 1)
            ->addPlayer(TeamSide::HOME, 6, 7, strength: 2, skills: [SkillName::RightStuff, SkillName::Stunty], id: 2)
            ->withHomeTeam(TeamStateDTO::create(1, 'Home', 'Human', TeamSide::HOME, 0))
            ->withBallOffPitch()
            ->build();

        // 1 → pokus sezrat; 4 → vysmekl se; dopad 6 → na nohou
        $dice = new FixedDiceRoller([1, 4, 6]);
        $result = (new ActionResolver($dice))->resolve($state, ActionType::THROW_TEAM_MATE, [
            'playerId' => 1, 'targetId' => 2, 'targetX' => 8, 'targetY' => 7,
        ]);

        $projectile = $result->getNewState()->requirePlayer(2);
        $this->assertNotSame(PlayerState::DEAD, $projectile->getState());
        $this->assertSame([6, 7], [$projectile->requirePosition()->getX(), $projectile->requirePosition()->getY()]);
    }
}
