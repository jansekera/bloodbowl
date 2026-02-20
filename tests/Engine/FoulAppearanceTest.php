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

final class FoulAppearanceTest extends TestCase
{
    public function testFoulAppearanceFailBlockerLosesAction(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, skills: [SkillName::FoulAppearance], id: 2)
            ->withBallOffPitch()
            ->build();

        // FA roll = 1 → fails
        $dice = new FixedDiceRoller([1]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);

        $this->assertFalse($result->isTurnover());
        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('foul_appearance', $types);
        $this->assertNotContains('block', $types);

        // Attacker marked as acted
        $attacker = $result->getNewState()->getPlayer(1);
        $this->assertTrue($attacker->hasActed());

        // Defender untouched
        $defender = $result->getNewState()->getPlayer(2);
        $this->assertSame(PlayerState::STANDING, $defender->getState());
    }

    public function testFoulAppearancePassBlockProceeds(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 4, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, skills: [SkillName::FoulAppearance], armour: 7, id: 2)
            ->withBallOffPitch()
            ->build();

        // FA roll = 2 → passes
        // Block die: 6 (Defender Down), 5+4 armor = 9 > AV7 broken, injury 3+3=6 stunned
        $dice = new FixedDiceRoller([2, 6, 5, 4, 3, 3]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);

        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('foul_appearance', $types);
        $this->assertContains('block', $types);
    }

    public function testFoulAppearanceNotTurnover(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, skills: [SkillName::FoulAppearance], id: 2)
            ->withBallOffPitch()
            ->build();

        // FA roll = 1 → fails — NOT a turnover
        $dice = new FixedDiceRoller([1]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);

        $this->assertFalse($result->isTurnover());
    }

    public function testFoulAppearanceCheckedBeforeChainsaw(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, skills: [SkillName::Chainsaw], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, skills: [SkillName::FoulAppearance], id: 2)
            ->withBallOffPitch()
            ->build();

        // FA roll = 1 → fails, chainsaw never triggers
        $dice = new FixedDiceRoller([1]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);

        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('foul_appearance', $types);
        $this->assertNotContains('chainsaw', $types);
        $this->assertFalse($result->isTurnover());
    }

    public function testFoulAppearanceCheckedBeforeStab(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, skills: [SkillName::Stab], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, skills: [SkillName::FoulAppearance], id: 2)
            ->withBallOffPitch()
            ->build();

        // FA roll = 1 → fails, stab never triggers
        $dice = new FixedDiceRoller([1]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);

        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('foul_appearance', $types);
        $this->assertNotContains('stab', $types);
        $this->assertFalse($result->isTurnover());
    }

    public function testFoulAppearanceRoll2Succeeds(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 4, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, skills: [SkillName::FoulAppearance], armour: 9, id: 2)
            ->withBallOffPitch()
            ->build();

        // FA roll = 2 → passes (minimum success)
        // ST4 vs ST3: 2 block dice, attacker chooses
        // Block dice: 6 (Defender Down), 3 (Pushed) → chosen DD → pushed + knocked down
        // Armor roll: 3+3 = 6 ≤ AV9 → holds (just prone)
        $dice = new FixedDiceRoller([2, 6, 3, 3, 3]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);

        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('foul_appearance', $types);
        $this->assertContains('block', $types);
    }
}
