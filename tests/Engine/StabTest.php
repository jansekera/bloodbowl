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

final class StabTest extends TestCase
{
    public function testStabArmorBrokenCausesInjury(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, skills: [SkillName::Stab], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, armour: 7, id: 2)
            ->withBallOffPitch()
            ->build();

        // Stab: armor roll 2D6 = 5+4 = 9 > AV7 → broken
        // Injury roll 2D6 = 3+3 = 6 → stunned (≤7)
        $dice = new FixedDiceRoller([5, 4, 3, 3]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);

        $this->assertFalse($result->isTurnover());
        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('stab', $types);
        $this->assertContains('armour_roll', $types);
        $this->assertContains('injury_roll', $types);
        $this->assertNotContains('block', $types);

        $defender = $result->getNewState()->getPlayer(2);
        $this->assertSame(PlayerState::STUNNED, $defender->getState());
    }

    public function testStabArmorHoldsNoEffect(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, skills: [SkillName::Stab], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, armour: 9, id: 2)
            ->withBallOffPitch()
            ->build();

        // Stab: armor roll 2D6 = 3+3 = 6 ≤ AV9 → holds
        $dice = new FixedDiceRoller([3, 3]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);

        $this->assertFalse($result->isTurnover());
        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('stab', $types);
        $this->assertContains('armour_roll', $types);
        $this->assertNotContains('injury_roll', $types);

        $defender = $result->getNewState()->getPlayer(2);
        $this->assertSame(PlayerState::STANDING, $defender->getState());
        $this->assertNotNull($defender->getPosition());
    }

    public function testStabNeverCausesTurnover(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, skills: [SkillName::Stab], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, armour: 7, id: 2)
            ->withBallOffPitch()
            ->build();

        // Armor holds — still no turnover
        $dice = new FixedDiceRoller([2, 2]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);
        $this->assertFalse($result->isTurnover());

        // Armor breaks, injury = KO — still no turnover
        $state2 = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, skills: [SkillName::Stab], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, armour: 7, id: 2)
            ->withBallOffPitch()
            ->build();
        $dice2 = new FixedDiceRoller([6, 6, 4, 5]); // armor 12 > 7 broken, injury 9 → KO
        $resolver2 = new ActionResolver($dice2);
        $result2 = $resolver2->resolve($state2, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);
        $this->assertFalse($result2->isTurnover());
    }

    public function testStabNoPushback(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, skills: [SkillName::Stab], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, armour: 9, id: 2)
            ->withBallOffPitch()
            ->build();

        // Armor holds
        $dice = new FixedDiceRoller([3, 3]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);

        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertNotContains('push', $types);
        $this->assertNotContains('follow_up', $types);

        // Defender stays at same position
        $defender = $result->getNewState()->getPlayer(2);
        $this->assertEquals(6, $defender->getPosition()->getX());
        $this->assertEquals(5, $defender->getPosition()->getY());
    }

    public function testStabBallDropOnInjury(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, skills: [SkillName::Stab], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, armour: 7, id: 2)
            ->withBallCarried(2)
            ->build();

        // Armor 5+4=9 > 7 → broken, injury 4+5=9 → KO (removed from pitch)
        // Ball bounces: D8 direction
        $dice = new FixedDiceRoller([5, 4, 4, 5, 3]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);

        $this->assertFalse($result->isTurnover());
        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('ball_bounce', $types);
    }

    public function testStabDuringBlitz(): void
    {
        // Attacker at (3,5) blitzes defender at (6,5) — moves to (5,5), then stab
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 3, 5, skills: [SkillName::Stab], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, armour: 9, id: 2)
            ->withBallOffPitch()
            ->build();

        // Armor holds
        $dice = new FixedDiceRoller([3, 3]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::BLITZ, ['playerId' => 1, 'targetId' => 2]);

        $this->assertFalse($result->isTurnover());
        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('stab', $types);
        $this->assertNotContains('block', $types);

        // Defender untouched
        $defender = $result->getNewState()->getPlayer(2);
        $this->assertSame(PlayerState::STANDING, $defender->getState());
    }

    public function testStabAttackerMarkedAsActed(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, skills: [SkillName::Stab], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, armour: 9, id: 2)
            ->withBallOffPitch()
            ->build();

        $dice = new FixedDiceRoller([3, 3]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);

        $attacker = $result->getNewState()->getPlayer(1);
        $this->assertTrue($attacker->hasActed());
    }
}
