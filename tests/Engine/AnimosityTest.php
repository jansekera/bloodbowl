<?php
declare(strict_types=1);

namespace App\Tests\Engine;

use App\DTO\TeamStateDTO;
use App\Engine\ActionResolver;
use App\Engine\FixedDiceRoller;
use App\Enum\ActionType;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use PHPUnit\Framework\TestCase;

final class AnimosityTest extends TestCase
{
    public function testPassToSameRaceNoAnimosityCheck(): void
    {
        // Both players have same raceName = 'Skaven', Animosity not triggered
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, agility: 3, skills: [SkillName::Animosity, SkillName::SureHands], id: 1, raceName: 'Skaven')
            ->addPlayer(TeamSide::HOME, 7, 5, agility: 3, id: 2, raceName: 'Skaven')
            ->withBallCarried(1)
            ->build();

        // Accuracy: 7-3-1(quick) = 3+, roll 4 = accurate
        // Catch: 3+, roll 4 = success
        $dice = new FixedDiceRoller([4, 4]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::PASS, [
            'playerId' => 1,
            'targetX' => 7,
            'targetY' => 5,
        ]);

        $this->assertTrue($result->isSuccess());
        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertNotContains('animosity', $types);
        $this->assertContains('pass', $types);
    }

    public function testPassToDifferentRaceSucceeds(): void
    {
        // Passer = Skaven, Receiver = Goblin → different race, Animosity triggers
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, agility: 3, skills: [SkillName::Animosity, SkillName::SureHands], id: 1, raceName: 'Skaven')
            ->addPlayer(TeamSide::HOME, 7, 5, agility: 3, id: 2, raceName: 'Goblin')
            ->withBallCarried(1)
            ->build();

        // Animosity roll = 2 → passes
        // Accuracy: 7-3-1(quick) = 3+, roll 4 = accurate
        // Catch: 3+, roll 4 = success
        $dice = new FixedDiceRoller([2, 4, 4]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::PASS, [
            'playerId' => 1,
            'targetX' => 7,
            'targetY' => 5,
        ]);

        $this->assertTrue($result->isSuccess());
        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('animosity', $types);
        $this->assertContains('pass', $types);

        // Verify animosity event was successful
        foreach ($result->getEvents() as $event) {
            if ($event->getType() === 'animosity') {
                $this->assertTrue($event->getData()['success']);
            }
        }
    }

    public function testPassToDifferentRaceFailsOnOne(): void
    {
        // Animosity roll = 1 → fail, ball stays with passer
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, agility: 3, skills: [SkillName::Animosity, SkillName::SureHands], id: 1, raceName: 'Skaven')
            ->addPlayer(TeamSide::HOME, 7, 5, agility: 3, id: 2, raceName: 'Goblin')
            ->withBallCarried(1)
            ->build();

        // Animosity roll = 1 → fails
        $dice = new FixedDiceRoller([1]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::PASS, [
            'playerId' => 1,
            'targetX' => 7,
            'targetY' => 5,
        ]);

        // Not a turnover — ball stays with passer
        $this->assertTrue($result->isSuccess());
        $this->assertFalse($result->isTurnover());

        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('animosity', $types);
        $this->assertNotContains('pass', $types);

        // Ball still held by player 1
        $this->assertTrue($result->getNewState()->getBall()->isHeld());
        $this->assertEquals(1, $result->getNewState()->getBall()->getCarrierId());
    }

    public function testHandOffToSameRaceNoCheck(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, skills: [SkillName::Animosity], id: 1, raceName: 'Skaven')
            ->addPlayer(TeamSide::HOME, 6, 5, agility: 3, id: 2, raceName: 'Skaven')
            ->withBallCarried(1)
            ->build();

        // Catch: 7-3 = 4+, +1 = 3+, roll 4 = success
        $dice = new FixedDiceRoller([4]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::HAND_OFF, [
            'playerId' => 1,
            'targetId' => 2,
        ]);

        $this->assertTrue($result->isSuccess());
        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertNotContains('animosity', $types);
    }

    public function testHandOffToDifferentRaceFailsOnOne(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, skills: [SkillName::Animosity], id: 1, raceName: 'Skaven')
            ->addPlayer(TeamSide::HOME, 6, 5, agility: 3, id: 2, raceName: 'Goblin')
            ->withBallCarried(1)
            ->build();

        // Animosity roll = 1 → fails
        $dice = new FixedDiceRoller([1]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::HAND_OFF, [
            'playerId' => 1,
            'targetId' => 2,
        ]);

        $this->assertTrue($result->isSuccess());
        $this->assertFalse($result->isTurnover());

        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('animosity', $types);

        // Ball still with player 1
        $this->assertTrue($result->getNewState()->getBall()->isHeld());
        $this->assertEquals(1, $result->getNewState()->getBall()->getCarrierId());
    }

    public function testHandOffToDifferentRaceSucceeds(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, skills: [SkillName::Animosity], id: 1, raceName: 'Skaven')
            ->addPlayer(TeamSide::HOME, 6, 5, agility: 3, id: 2, raceName: 'Goblin')
            ->withBallCarried(1)
            ->build();

        // Animosity roll = 3 → passes
        // Catch: 3+, roll 4 = success
        $dice = new FixedDiceRoller([3, 4]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::HAND_OFF, [
            'playerId' => 1,
            'targetId' => 2,
        ]);

        $this->assertTrue($result->isSuccess());
        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('animosity', $types);
        $this->assertContains('hand_off', $types);

        // Ball carried by receiver
        $this->assertEquals(2, $result->getNewState()->getBall()->getCarrierId());
    }

    public function testWithoutAnimositySkillNoCheck(): void
    {
        // Player without Animosity: no check even with different raceName
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, agility: 3, id: 1, raceName: 'Skaven')
            ->addPlayer(TeamSide::HOME, 7, 5, agility: 3, id: 2, raceName: 'Goblin')
            ->withBallCarried(1)
            ->build();

        // Accuracy: 3+, roll 4 = accurate; Catch: 3+, roll 4 = success
        $dice = new FixedDiceRoller([4, 4]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::PASS, [
            'playerId' => 1,
            'targetX' => 7,
            'targetY' => 5,
        ]);

        $this->assertTrue($result->isSuccess());
        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertNotContains('animosity', $types);
    }

    public function testBothNullRaceNameSameRace(): void
    {
        // Both raceName null = same race, no Animosity check
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, agility: 3, skills: [SkillName::Animosity], id: 1)
            ->addPlayer(TeamSide::HOME, 7, 5, agility: 3, id: 2)
            ->withBallCarried(1)
            ->build();

        // Accuracy: 3+, roll 4 = accurate; Catch: 3+, roll 4 = success
        $dice = new FixedDiceRoller([4, 4]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::PASS, [
            'playerId' => 1,
            'targetX' => 7,
            'targetY' => 5,
        ]);

        $this->assertTrue($result->isSuccess());
        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertNotContains('animosity', $types);
    }

    public function testAnimosityPasserNullReceiverSetDifferent(): void
    {
        // Passer raceName null, receiver 'Goblin' → different race
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, agility: 3, skills: [SkillName::Animosity], id: 1)
            ->addPlayer(TeamSide::HOME, 7, 5, agility: 3, id: 2, raceName: 'Goblin')
            ->withBallCarried(1)
            ->build();

        // Animosity roll = 1 → fails
        $dice = new FixedDiceRoller([1]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::PASS, [
            'playerId' => 1,
            'targetX' => 7,
            'targetY' => 5,
        ]);

        // Animosity triggered and failed
        $this->assertTrue($result->isSuccess());
        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('animosity', $types);
        $this->assertNotContains('pass', $types);
    }

    public function testAnimosityPassToEmptySquareNoCheck(): void
    {
        // Passing to empty square: no receiver, no Animosity check
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, agility: 3, skills: [SkillName::Animosity], id: 1, raceName: 'Skaven')
            ->withBallCarried(1)
            ->build();

        // Accuracy: 3+, roll 4 = accurate; No catcher → ball bounces: D8 = 3
        $dice = new FixedDiceRoller([4, 3]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::PASS, [
            'playerId' => 1,
            'targetX' => 7,
            'targetY' => 5,
        ]);

        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertNotContains('animosity', $types);
    }
}
