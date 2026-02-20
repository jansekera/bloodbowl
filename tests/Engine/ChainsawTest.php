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

final class ChainsawTest extends TestCase
{
    public function testChainsawArmorBrokenCausesInjury(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, skills: [SkillName::Chainsaw], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, armour: 7, id: 2)
            ->withBallOffPitch()
            ->build();

        // Chainsaw D6 = 2 → attacks defender
        // Armor roll 2D6: 5+4 = 9 > AV7 → broken
        // Injury roll 2D6: 3+3 = 6 → stunned (≤7)
        $dice = new FixedDiceRoller([2, 5, 4, 3, 3]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);

        $this->assertFalse($result->isTurnover());
        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('chainsaw', $types);
        $this->assertContains('armour_roll', $types);
        $this->assertContains('injury_roll', $types);
        $this->assertNotContains('block', $types);

        $defender = $result->getNewState()->getPlayer(2);
        $this->assertSame(PlayerState::STUNNED, $defender->getState());
    }

    public function testChainsawArmorHoldsNoEffect(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, skills: [SkillName::Chainsaw], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, armour: 9, id: 2)
            ->withBallOffPitch()
            ->build();

        // Chainsaw D6 = 3 → attacks defender
        // Armor roll 2D6: 3+3 = 6 ≤ AV9 → holds
        $dice = new FixedDiceRoller([3, 3, 3]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);

        $this->assertFalse($result->isTurnover());
        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('chainsaw', $types);
        $this->assertContains('armour_roll', $types);
        $this->assertNotContains('injury_roll', $types);

        $defender = $result->getNewState()->getPlayer(2);
        $this->assertSame(PlayerState::STANDING, $defender->getState());
    }

    public function testChainsawNeverCausesTurnover(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, skills: [SkillName::Chainsaw], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, armour: 7, id: 2)
            ->withBallOffPitch()
            ->build();

        // Chainsaw D6 = 4, armor 6+6=12 > 7 broken, injury 4+5=9 → KO
        $dice = new FixedDiceRoller([4, 6, 6, 4, 5]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);
        $this->assertFalse($result->isTurnover());
    }

    public function testChainsawNoPushback(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, skills: [SkillName::Chainsaw], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, armour: 9, id: 2)
            ->withBallOffPitch()
            ->build();

        // Chainsaw D6 = 5, armor holds (3+3=6 ≤ AV9)
        $dice = new FixedDiceRoller([5, 3, 3]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);

        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertNotContains('push', $types);
        $this->assertNotContains('follow_up', $types);

        // Defender still at original position
        $defender = $result->getNewState()->getPlayer(2);
        $this->assertNotNull($defender->getPosition());
        $this->assertEquals(6, $defender->getPosition()->getX());
        $this->assertEquals(5, $defender->getPosition()->getY());
    }

    public function testChainsawKickbackOnRoll1(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, skills: [SkillName::Chainsaw], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, armour: 7, id: 2)
            ->withBallOffPitch()
            ->build();

        // Chainsaw D6 = 1 → kickback!
        // Armor roll on attacker: 5+4 = 9 > AV8 → broken
        // Injury: 3+3 = 6 → stunned
        $dice = new FixedDiceRoller([1, 5, 4, 3, 3]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);

        $this->assertFalse($result->isTurnover()); // Chainsaw never causes turnover
        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('chainsaw', $types);
        $this->assertContains('chainsaw_kickback', $types);

        // Attacker is injured
        $attacker = $result->getNewState()->getPlayer(1);
        $this->assertSame(PlayerState::STUNNED, $attacker->getState());

        // Defender untouched
        $defender = $result->getNewState()->getPlayer(2);
        $this->assertSame(PlayerState::STANDING, $defender->getState());
    }

    public function testChainsawKickbackArmorHolds(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, skills: [SkillName::Chainsaw], armour: 9, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, armour: 7, id: 2)
            ->withBallOffPitch()
            ->build();

        // Chainsaw D6 = 1 → kickback!
        // Armor roll on attacker: 2+3 = 5 ≤ AV9 → holds
        $dice = new FixedDiceRoller([1, 2, 3]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);

        $this->assertFalse($result->isTurnover());
        $attacker = $result->getNewState()->getPlayer(1);
        $this->assertSame(PlayerState::STANDING, $attacker->getState());
    }

    public function testChainsawDropsBallOnDefenderKnockdown(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, skills: [SkillName::Chainsaw], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, armour: 7, id: 2)
            ->withBallCarried(2)
            ->build();

        // Chainsaw D6 = 2, armor 5+4=9 > AV7 broken, injury 4+5=9 → KO (off pitch)
        // Ball drops at defender pos, bounces: D8=3
        $dice = new FixedDiceRoller([2, 5, 4, 4, 5, 3]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);

        $this->assertFalse($result->isTurnover());
        $newState = $result->getNewState();
        $this->assertSame(PlayerState::KO, $newState->getPlayer(2)->getState());
        $this->assertFalse($newState->getBall()->isHeld());
    }

    public function testChainsawAttackerMarkedAsActed(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, skills: [SkillName::Chainsaw], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, armour: 9, id: 2)
            ->withBallOffPitch()
            ->build();

        // Chainsaw D6 = 3, armor holds
        $dice = new FixedDiceRoller([3, 3, 3]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);

        $attacker = $result->getNewState()->getPlayer(1);
        $this->assertTrue($attacker->hasActed());
        $this->assertTrue($attacker->hasMoved());
    }
}
