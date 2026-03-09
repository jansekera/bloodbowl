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

final class InteractiveRerollTest extends TestCase
{
    // === Dodge Interactive Reroll ===

    public function testDodgeFailCreatesInteractivePendingReroll(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, movement: 6, agility: 3, id: 1)
            ->addPlayer(TeamSide::AWAY, 5, 4) // creates tackle zone
            ->build();

        // Dodge target 3+. Roll 2 = fail. No skill reroll available.
        $dice = new FixedDiceRoller([2]);
        $resolver = new ActionResolver($dice);
        $resolver->setInteractiveRerolls(true);

        $result = $resolver->resolve($state, ActionType::MOVE, [
            'playerId' => 1, 'x' => 5, 'y' => 6,
        ]);

        // Should NOT be a turnover — pending reroll
        $this->assertTrue($result->isSuccess());
        $this->assertFalse($result->isTurnover());

        $pending = $result->getNewState()->getPendingReroll();
        $this->assertNotNull($pending);
        $this->assertEquals('dodge', $pending->getRollType());
        $this->assertEquals(1, $pending->getPlayerId());
        $this->assertEquals(4, $pending->getTarget());
        $this->assertEquals(2, $pending->getRoll());
        $this->assertFalse($pending->isProAvailable()); // no Pro skill
        $this->assertTrue($pending->isTeamRerollAvailable());
        $this->assertEquals(5, $pending->getTargetX());
        $this->assertEquals(6, $pending->getTargetY());

        // Player should still be at original position
        $player = $result->getNewState()->getPlayer(1);
        $this->assertNotNull($player);
        $this->assertEquals(5, $player->getPosition()?->getX());
        $this->assertEquals(5, $player->getPosition()?->getY());
    }

    public function testDodgeInteractiveRerollAcceptTeamRerollSuccess(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, movement: 6, agility: 3, id: 1)
            ->addPlayer(TeamSide::AWAY, 5, 4)
            ->build();

        // Step 1: dodge fails → pending reroll
        $dice = new FixedDiceRoller([2, /* reroll: */ 4]);
        $resolver = new ActionResolver($dice);
        $resolver->setInteractiveRerolls(true);

        $result = $resolver->resolve($state, ActionType::MOVE, [
            'playerId' => 1, 'x' => 5, 'y' => 6,
        ]);

        $this->assertNotNull($result->getNewState()->getPendingReroll());

        // Step 2: accept team reroll → roll 4 = success (3+)
        $result2 = $resolver->resolve($result->getNewState(), ActionType::RESOLVE_REROLL, [
            'choice' => 'team_reroll',
        ]);

        $this->assertTrue($result2->isSuccess());
        $this->assertFalse($result2->isTurnover());
        $this->assertNull($result2->getNewState()->getPendingReroll());

        // Player should be at target position
        $player = $result2->getNewState()->getPlayer(1);
        $this->assertNotNull($player);
        $this->assertEquals(5, $player->getPosition()?->getX());
        $this->assertEquals(6, $player->getPosition()?->getY());

        // Team reroll should be consumed
        $this->assertTrue($result2->getNewState()->getHomeTeam()->isRerollUsedThisTurn());
    }

    public function testDodgeInteractiveRerollDeclineCausesTurnover(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, movement: 6, agility: 3, id: 1)
            ->addPlayer(TeamSide::AWAY, 5, 4)
            ->build();

        // Dodge fails
        $dice = new FixedDiceRoller([2]);
        $resolver = new ActionResolver($dice);
        $resolver->setInteractiveRerolls(true);

        $result = $resolver->resolve($state, ActionType::MOVE, [
            'playerId' => 1, 'x' => 5, 'y' => 6,
        ]);

        $this->assertNotNull($result->getNewState()->getPendingReroll());

        // Decline reroll → turnover
        $result2 = $resolver->resolve($result->getNewState(), ActionType::RESOLVE_REROLL, [
            'choice' => 'decline',
        ]);

        $this->assertTrue($result2->isTurnover());
        $player = $result2->getNewState()->getPlayer(1);
        $this->assertEquals(PlayerState::PRONE, $player?->getState());
    }

    public function testDodgeSkillRerollAutoUsedBeforeInteractive(): void
    {
        // Dodge skill reroll is auto-used. Only if it fails too, interactive kicks in.
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, movement: 6, agility: 3, skills: [SkillName::Dodge], id: 1)
            ->addPlayer(TeamSide::AWAY, 5, 4)
            ->build();

        // Dodge target 3+. Roll 2=fail, Dodge skill reroll 1=fail.
        // After skill reroll, team reroll is blocked (can't double-reroll).
        $dice = new FixedDiceRoller([2, 1]);
        $resolver = new ActionResolver($dice);
        $resolver->setInteractiveRerolls(true);

        $result = $resolver->resolve($state, ActionType::MOVE, [
            'playerId' => 1, 'x' => 5, 'y' => 6,
        ]);

        // No pending reroll because team reroll is blocked after skill reroll
        $this->assertNull($result->getNewState()->getPendingReroll());
        $this->assertTrue($result->isTurnover());
    }

    public function testDodgeWithProAvailable(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, movement: 6, agility: 3, skills: [SkillName::Pro], id: 1)
            ->addPlayer(TeamSide::AWAY, 5, 4)
            ->build();

        // Dodge target 3+. Roll 2=fail. Pro is available.
        $dice = new FixedDiceRoller([2]);
        $resolver = new ActionResolver($dice);
        $resolver->setInteractiveRerolls(true);

        $result = $resolver->resolve($state, ActionType::MOVE, [
            'playerId' => 1, 'x' => 5, 'y' => 6,
        ]);

        $pending = $result->getNewState()->getPendingReroll();
        $this->assertNotNull($pending);
        $this->assertTrue($pending->isProAvailable());
        $this->assertTrue($pending->isTeamRerollAvailable());
    }

    // === GFI Interactive Reroll ===

    public function testGfiFailCreatesInteractivePendingReroll(): void
    {
        // Player with 6 MA moves 7 squares → 1 GFI
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, movement: 6, agility: 3, id: 1)
            ->build();

        // GFI target 2+. Roll 1=fail. No Sure Feet.
        $dice = new FixedDiceRoller([1]);
        $resolver = new ActionResolver($dice);
        $resolver->setInteractiveRerolls(true);

        $result = $resolver->resolve($state, ActionType::MOVE, [
            'playerId' => 1, 'x' => 5, 'y' => 12, // 7 squares = 1 GFI
        ]);

        $pending = $result->getNewState()->getPendingReroll();
        $this->assertNotNull($pending);
        $this->assertEquals('gfi', $pending->getRollType());
        $this->assertEquals(2, $pending->getTarget());
    }

    public function testGfiInteractiveRerollSuccess(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, movement: 6, agility: 3, id: 1)
            ->build();

        // GFI fail, then team reroll succeeds
        $dice = new FixedDiceRoller([1, /* reroll: */ 5]);
        $resolver = new ActionResolver($dice);
        $resolver->setInteractiveRerolls(true);

        $result = $resolver->resolve($state, ActionType::MOVE, [
            'playerId' => 1, 'x' => 5, 'y' => 12,
        ]);

        $this->assertNotNull($result->getNewState()->getPendingReroll());

        $result2 = $resolver->resolve($result->getNewState(), ActionType::RESOLVE_REROLL, [
            'choice' => 'team_reroll',
        ]);

        $this->assertTrue($result2->isSuccess());
        $this->assertFalse($result2->isTurnover());

        $player = $result2->getNewState()->getPlayer(1);
        $this->assertEquals(5, $player?->getPosition()?->getX());
        $this->assertEquals(12, $player?->getPosition()?->getY());
    }

    // === Non-interactive mode (AI) unchanged ===

    public function testNonInteractiveModeAutoResolvesRerolls(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, movement: 6, agility: 3, id: 1)
            ->addPlayer(TeamSide::AWAY, 5, 4)
            ->build();

        // Dodge fail, team reroll succeeds
        $dice = new FixedDiceRoller([2, 4]);
        $resolver = new ActionResolver($dice);
        // interactiveRerolls defaults to false

        $result = $resolver->resolve($state, ActionType::MOVE, [
            'playerId' => 1, 'x' => 5, 'y' => 6,
        ]);

        // Should auto-resolve — no pending reroll
        $this->assertNull($result->getNewState()->getPendingReroll());
        $this->assertTrue($result->isSuccess());

        $player = $result->getNewState()->getPlayer(1);
        $this->assertEquals(5, $player?->getPosition()?->getX());
        $this->assertEquals(6, $player?->getPosition()?->getY());
    }

    // === PendingRerollDTO serialization ===

    public function testPendingRerollSerializesToArray(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, movement: 6, agility: 3, id: 1)
            ->addPlayer(TeamSide::AWAY, 5, 4)
            ->build();

        $dice = new FixedDiceRoller([2]);
        $resolver = new ActionResolver($dice);
        $resolver->setInteractiveRerolls(true);

        $result = $resolver->resolve($state, ActionType::MOVE, [
            'playerId' => 1, 'x' => 5, 'y' => 6,
        ]);

        $newState = $result->getNewState();
        $this->assertNotNull($newState->getPendingReroll());

        // Serialize and deserialize
        $array = $newState->toArray();
        $this->assertArrayHasKey('pendingReroll', $array);
        $this->assertNotNull($array['pendingReroll']);

        $restored = \App\DTO\GameState::fromArray($array);
        $restoredPending = $restored->getPendingReroll();
        $this->assertNotNull($restoredPending);
        $this->assertEquals('dodge', $restoredPending->getRollType());
        $this->assertEquals(1, $restoredPending->getPlayerId());
        $this->assertEquals(4, $restoredPending->getTarget());
    }

    // === Validation ===

    public function testCannotPerformOtherActionsWhilePendingReroll(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, movement: 6, agility: 3, id: 1)
            ->addPlayer(TeamSide::HOME, 10, 10, movement: 6, agility: 3, id: 2)
            ->addPlayer(TeamSide::AWAY, 5, 4)
            ->build();

        $dice = new FixedDiceRoller([2]);
        $resolver = new ActionResolver($dice);
        $resolver->setInteractiveRerolls(true);

        $result = $resolver->resolve($state, ActionType::MOVE, [
            'playerId' => 1, 'x' => 5, 'y' => 6,
        ]);

        $stateWithPending = $result->getNewState();
        $this->assertNotNull($stateWithPending->getPendingReroll());

        // Trying to move another player should be blocked by RulesEngine
        $rulesEngine = new \App\Engine\RulesEngine();
        $errors = $rulesEngine->validate($stateWithPending, ActionType::MOVE, [
            'playerId' => 2, 'x' => 10, 'y' => 11,
        ]);
        $this->assertNotEmpty($errors);
        $this->assertStringContainsString('pending reroll', $errors[0]);
    }

    // === Pro reroll flow ===

    public function testProRerollFailThenTeamRerollAvailable(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, movement: 6, agility: 3, skills: [SkillName::Pro], id: 1)
            ->addPlayer(TeamSide::AWAY, 5, 4)
            ->build();

        // Dodge fails
        $dice = new FixedDiceRoller([2, /* pro check: */ 2, /* team reroll: */ 5]);
        $resolver = new ActionResolver($dice);
        $resolver->setInteractiveRerolls(true);

        $result = $resolver->resolve($state, ActionType::MOVE, [
            'playerId' => 1, 'x' => 5, 'y' => 6,
        ]);

        $this->assertNotNull($result->getNewState()->getPendingReroll());

        // Choose Pro → pro check roll 2 (fail, < 4)
        $result2 = $resolver->resolve($result->getNewState(), ActionType::RESOLVE_REROLL, [
            'choice' => 'pro',
        ]);

        // Pro failed, but team reroll should still be available
        $pending2 = $result2->getNewState()->getPendingReroll();
        $this->assertNotNull($pending2, 'Should offer team reroll after failed Pro');
        $this->assertFalse($pending2->isProAvailable());
        $this->assertTrue($pending2->isTeamRerollAvailable());

        // Accept team reroll → roll 5 = success (3+)
        $result3 = $resolver->resolve($result2->getNewState(), ActionType::RESOLVE_REROLL, [
            'choice' => 'team_reroll',
        ]);

        $this->assertTrue($result3->isSuccess());
        $player = $result3->getNewState()->getPlayer(1);
        $this->assertEquals(6, $player?->getPosition()?->getY());
    }
}
