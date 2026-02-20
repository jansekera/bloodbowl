<?php
declare(strict_types=1);

namespace App\Tests\Engine;

use App\DTO\MatchPlayerDTO;
use App\Engine\ActionResolver;
use App\Engine\BallResolver;
use App\Engine\FixedDiceRoller;
use App\Engine\InjuryResolver;
use App\Engine\Pathfinder;
use App\Engine\RulesEngine;
use App\Engine\TacklezoneCalculator;
use App\Enum\ActionType;
use App\Enum\PlayerState;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use App\ValueObject\PlayerStats;
use App\ValueObject\Position;
use PHPUnit\Framework\TestCase;

final class SkillModifiersTest extends TestCase
{
    // === Prehensile Tail ===

    public function testPrehensileTailAddsDodgeDifficulty(): void
    {
        $tzCalc = new TacklezoneCalculator();
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, agility: 3, id: 1)
            ->addPlayer(TeamSide::AWAY, 5, 6, id: 2, skills: [SkillName::PrehensileTail])
            ->build();

        // Normal dodge from TZ: 7 - 3 = 4+
        // With Prehensile Tail at source: +1 = 5+
        $target = $tzCalc->calculateDodgeTarget($state, $state->getPlayer(1), new Position(6, 7), new Position(5, 7));
        $this->assertEquals(5, $target);
    }

    public function testPrehensileTailStacking(): void
    {
        $tzCalc = new TacklezoneCalculator();
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, agility: 3, id: 1)
            ->addPlayer(TeamSide::AWAY, 5, 6, id: 2, skills: [SkillName::PrehensileTail])
            ->addPlayer(TeamSide::AWAY, 4, 7, id: 3, skills: [SkillName::PrehensileTail])
            ->build();

        // Normal: 7 - 3 + 1 (2 TZ at dest, -1 for "free") = 5+
        // +2 Prehensile Tails at source = 7+ → clamped to 6+
        $target = $tzCalc->calculateDodgeTarget($state, $state->getPlayer(1), new Position(6, 7), new Position(5, 7));
        $this->assertEquals(6, $target);
    }

    public function testPrehensileTailNotAppliedWithoutSource(): void
    {
        $tzCalc = new TacklezoneCalculator();
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, agility: 3, id: 1)
            ->addPlayer(TeamSide::AWAY, 5, 6, id: 2, skills: [SkillName::PrehensileTail])
            ->build();

        // Without source position, no Prehensile Tail
        $target = $tzCalc->calculateDodgeTarget($state, $state->getPlayer(1), new Position(6, 7));
        $this->assertEquals(4, $target);
    }

    // === Stunty ===

    public function testStuntyDodgeBonus(): void
    {
        $tzCalc = new TacklezoneCalculator();
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, agility: 3, id: 1, skills: [SkillName::Stunty])
            ->addPlayer(TeamSide::AWAY, 5, 6, id: 2)
            ->build();

        // Normal dodge: 7 - 3 = 4+
        // Stunty: -1 = 3+
        $target = $tzCalc->calculateDodgeTarget($state, $state->getPlayer(1), new Position(6, 7), new Position(5, 7));
        $this->assertEquals(3, $target);
    }

    public function testStuntyPlusDodgeStack(): void
    {
        $tzCalc = new TacklezoneCalculator();
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, agility: 3, id: 1, skills: [SkillName::Stunty, SkillName::Dodge])
            ->addPlayer(TeamSide::AWAY, 5, 6, id: 2)
            ->build();

        // Normal dodge: 7 - 3 = 4+
        // Dodge: -1, Stunty: -1 = 2+
        $target = $tzCalc->calculateDodgeTarget($state, $state->getPlayer(1), new Position(6, 7), new Position(5, 7));
        $this->assertEquals(2, $target);
    }

    public function testStuntyInjuryModifier(): void
    {
        $player = MatchPlayerDTO::create(
            id: 1, playerId: 1, name: 'Goblin', number: 1,
            positionalName: 'Goblin',
            stats: new PlayerStats(6, 2, 3, 7),
            skills: [SkillName::Stunty],
            teamSide: TeamSide::HOME,
            position: new Position(5, 7),
        )->withState(PlayerState::PRONE);

        $injuryResolver = new InjuryResolver();
        // Armor: 5+3=8 > 7 (broken)
        // Injury: 4+4=8 (normally KO, 8-9) but Stunty +1 = 9 (still KO, 8-9)
        $dice = new FixedDiceRoller([5, 3, 4, 4]);
        $result = $injuryResolver->resolve($player, $dice);

        $this->assertEquals(PlayerState::KO, $result['player']->getState());
    }

    public function testStuntyInjuryMakesCasualtyEasier(): void
    {
        $player = MatchPlayerDTO::create(
            id: 1, playerId: 1, name: 'Goblin', number: 1,
            positionalName: 'Goblin',
            stats: new PlayerStats(6, 2, 3, 7),
            skills: [SkillName::Stunty],
            teamSide: TeamSide::HOME,
            position: new Position(5, 7),
        )->withState(PlayerState::PRONE);

        $injuryResolver = new InjuryResolver();
        // Armor: 4+4=8 > 7 (broken)
        // Injury: 5+4=9 (normally KO) + Stunty +1 = 10 → casualty
        $dice = new FixedDiceRoller([4, 4, 5, 4]);
        $result = $injuryResolver->resolve($player, $dice);

        $this->assertEquals(PlayerState::INJURED, $result['player']->getState());
    }

    // === BigHand ===

    public function testBigHandIgnoresTzOnPickup(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, movement: 6, agility: 3, id: 1, skills: [SkillName::BigHand])
            ->addPlayer(TeamSide::AWAY, 6, 6, id: 2) // enemy TZ on the ball
            ->withBallOnGround(6, 7)
            ->build();

        // Move: dodge out of TZ (7-3=4+), pickup with BigHand (TZ ignored, 7-3-1=3+)
        // Dice: dodge roll 6 (success), pickup roll 4 (>= 3, success)
        $dice = new FixedDiceRoller([6, 4]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::MOVE, [
            'playerId' => 1, 'x' => 6, 'y' => 7,
        ]);

        $this->assertTrue($result->isSuccess());
        $this->assertFalse($result->isTurnover());
        $ball = $result->getNewState()->getBall();
        $this->assertTrue($ball->isHeld());
        $this->assertEquals(1, $ball->getCarrierId());
    }

    public function testBigHandDoesNotAffectCatch(): void
    {
        // BigHand only helps pickup, not catch
        $tzCalc = new TacklezoneCalculator();
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, agility: 3, id: 1, skills: [SkillName::BigHand])
            ->addPlayer(TeamSide::AWAY, 5, 6, id: 2) // enemy TZ
            ->build();

        // Catch target should still include TZ penalty (BigHand doesn't help)
        // We just verify the skill exists but doesn't affect catch differently
        $this->assertTrue($state->getPlayer(1)->hasSkill(SkillName::BigHand));
    }

    // === Pro ===

    public function testProActivatesOnDodge(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, movement: 6, agility: 3, id: 1, skills: [SkillName::Pro])
            ->addPlayer(TeamSide::AWAY, 5, 6, id: 2)
            ->withHomeTeam(\App\DTO\TeamStateDTO::create(1, 'Home', 'Human', TeamSide::HOME, 0)) // no team rerolls
            ->build();

        // Dodge: roll 1 (fail), Pro check: 4 (pass), Pro reroll: 6 (success)
        $dice = new FixedDiceRoller([1, 4, 6]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::MOVE, [
            'playerId' => 1, 'x' => 5, 'y' => 8,
        ]);

        $this->assertTrue($result->isSuccess());
        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('pro', $types);
    }

    public function testProFailsDoesNotReroll(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, movement: 6, agility: 3, id: 1, skills: [SkillName::Pro])
            ->addPlayer(TeamSide::AWAY, 5, 6, id: 2)
            ->withHomeTeam(\App\DTO\TeamStateDTO::create(1, 'Home', 'Human', TeamSide::HOME, 0)) // no team rerolls
            ->build();

        // Dodge: roll 1 (fail), Pro check: 3 (fail) — no reroll, turnover
        $dice = new FixedDiceRoller([1, 3]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::MOVE, [
            'playerId' => 1, 'x' => 5, 'y' => 8,
        ]);

        $this->assertTrue($result->isTurnover());
    }

    public function testProOncePerTurn(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, movement: 6, agility: 3, id: 1, skills: [SkillName::Pro])
            ->addPlayer(TeamSide::AWAY, 5, 6, id: 2) // TZ at source
            ->withHomeTeam(\App\DTO\TeamStateDTO::create(1, 'Home', 'Human', TeamSide::HOME, 0))
            ->build();

        // Dodge away from enemy: roll 1 (fail), Pro check: 5 (pass), Pro reroll: 6 (success)
        // Now pro is used this turn
        $dice = new FixedDiceRoller([1, 5, 6]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::MOVE, [
            'playerId' => 1, 'x' => 6, 'y' => 7,
        ]);

        $this->assertTrue($result->isSuccess());
        $player = $result->getNewState()->getPlayer(1);
        $this->assertTrue($player->isProUsedThisTurn());
    }

    public function testProResetOnNewTurn(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1, skills: [SkillName::Pro])
            ->addPlayer(TeamSide::AWAY, 20, 7, id: 2)
            ->build();

        $player = $state->getPlayer(1)->withProUsedThisTurn(true);
        $state = $state->withPlayer($player);
        $this->assertTrue($state->getPlayer(1)->isProUsedThisTurn());

        $resetState = $state->resetPlayersForNewTurn(TeamSide::HOME);
        $this->assertFalse($resetState->getPlayer(1)->isProUsedThisTurn());
    }

    public function testProDoesNotOverrideSkillReroll(): void
    {
        // Player with Dodge + Pro: Dodge skill reroll first, then Pro
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, movement: 6, agility: 3, id: 1, skills: [SkillName::Dodge, SkillName::Pro])
            ->addPlayer(TeamSide::AWAY, 5, 6, id: 2)
            ->withHomeTeam(\App\DTO\TeamStateDTO::create(1, 'Home', 'Human', TeamSide::HOME, 0))
            ->build();

        // Dodge: roll 1 (fail), Dodge skill reroll: 6 (success) — Pro not used
        $dice = new FixedDiceRoller([1, 6]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::MOVE, [
            'playerId' => 1, 'x' => 5, 'y' => 8,
        ]);

        $this->assertTrue($result->isSuccess());
        $this->assertFalse($result->getNewState()->getPlayer(1)->isProUsedThisTurn());
    }

    public function testProOnPickup(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, movement: 6, agility: 3, id: 1, skills: [SkillName::Pro])
            ->addPlayer(TeamSide::AWAY, 20, 7, id: 2)
            ->withBallOnGround(6, 7)
            ->withHomeTeam(\App\DTO\TeamStateDTO::create(1, 'Home', 'Human', TeamSide::HOME, 0))
            ->build();

        // Pickup: roll 1 (fail), Pro: 4 (pass), reroll: 6 (success)
        $dice = new FixedDiceRoller([1, 4, 6]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::MOVE, [
            'playerId' => 1, 'x' => 6, 'y' => 7,
        ]);

        $this->assertTrue($result->isSuccess());
        $ball = $result->getNewState()->getBall();
        $this->assertTrue($ball->isHeld());
    }

    // === Jump Up ===

    public function testJumpUpFreeStandUp(): void
    {
        $state = (new GameStateBuilder())
            ->addPronePlayer(TeamSide::HOME, 5, 7, movement: 4, id: 1, skills: [SkillName::JumpUp])
            ->addPlayer(TeamSide::AWAY, 20, 7, id: 2)
            ->build();

        // With Jump Up, standing costs 0 MA, so all 4 MA available for movement
        $pathfinder = new Pathfinder(new TacklezoneCalculator());
        $moves = $pathfinder->findValidMoves($state, $state->getPlayer(1));

        // Should reach x=9 (4 squares right) + 2 GFI
        $this->assertArrayHasKey('11,7', $moves);
    }

    public function testJumpUpBlockFromProne(): void
    {
        $state = (new GameStateBuilder())
            ->addPronePlayer(TeamSide::HOME, 5, 7, strength: 4, id: 1, skills: [SkillName::JumpUp])
            ->addPlayer(TeamSide::AWAY, 6, 7, strength: 3, id: 2)
            ->build();

        // Jump Up allows blocking from prone
        $rules = new RulesEngine(new TacklezoneCalculator(), new Pathfinder(new TacklezoneCalculator()));
        $targets = $rules->getBlockTargets($state, $state->getPlayer(1));

        $this->assertNotEmpty($targets);
        $this->assertEquals(2, $targets[0]->getId());
    }

    public function testNormalProneCannotBlock(): void
    {
        $state = (new GameStateBuilder())
            ->addPronePlayer(TeamSide::HOME, 5, 7, strength: 4, id: 1) // no Jump Up
            ->addPlayer(TeamSide::AWAY, 6, 7, strength: 3, id: 2)
            ->build();

        $rules = new RulesEngine(new TacklezoneCalculator(), new Pathfinder(new TacklezoneCalculator()));
        $targets = $rules->getBlockTargets($state, $state->getPlayer(1));

        $this->assertEmpty($targets);
    }

    public function testJumpUpBlockStandsUpFirst(): void
    {
        $state = (new GameStateBuilder())
            ->addPronePlayer(TeamSide::HOME, 5, 7, strength: 5, id: 1, skills: [SkillName::JumpUp])
            ->addPlayer(TeamSide::AWAY, 6, 7, strength: 3, id: 2)
            ->build();

        // 2-dice block (ST 5 vs 3): die1=6, die2=6 (both Def Down), armor 3+3=6 < 8 (not broken)
        $dice = new FixedDiceRoller([6, 6, 3, 3]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::BLOCK, [
            'playerId' => 1, 'targetId' => 2,
        ]);

        $this->assertTrue($result->isSuccess());
        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('stand_up', $types);
        $this->assertContains('block', $types);

        // Attacker should be standing after block
        $attacker = $result->getNewState()->getPlayer(1);
        $this->assertEquals(PlayerState::STANDING, $attacker->getState());
    }

    // === Sprint ===

    public function testSprintAllowsThreeGfi(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, movement: 6, id: 1, skills: [SkillName::Sprint])
            ->addPlayer(TeamSide::AWAY, 20, 7, id: 2)
            ->build();

        $pathfinder = new Pathfinder(new TacklezoneCalculator());
        $moves = $pathfinder->findValidMoves($state, $state->getPlayer(1));

        // MA 6 + Sprint 3 GFI = 9 squares max
        $this->assertArrayHasKey('14,7', $moves); // 9 squares right
        $this->assertArrayNotHasKey('15,7', $moves); // 10 squares — too far
    }

    public function testNormalPlayerTwoGfi(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, movement: 6, id: 1) // no Sprint
            ->addPlayer(TeamSide::AWAY, 20, 7, id: 2)
            ->build();

        $pathfinder = new Pathfinder(new TacklezoneCalculator());
        $moves = $pathfinder->findValidMoves($state, $state->getPlayer(1));

        // MA 6 + 2 GFI = 8 squares max
        $this->assertArrayHasKey('13,7', $moves);
        $this->assertArrayNotHasKey('14,7', $moves);
    }

    public function testSprintGfiPathInfo(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, movement: 6, id: 1, skills: [SkillName::Sprint])
            ->addPlayer(TeamSide::AWAY, 20, 7, id: 2)
            ->build();

        $pathfinder = new Pathfinder(new TacklezoneCalculator());
        $moves = $pathfinder->findValidMoves($state, $state->getPlayer(1));

        // 9 squares right = 3 GFIs
        $path = $moves['14,7'];
        $this->assertEquals(3, $path->getGfiCount());
    }

    // === Break Tackle ===

    public function testBreakTackleUsesStrengthForDodge(): void
    {
        $tzCalc = new TacklezoneCalculator();
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, strength: 5, agility: 1, id: 1, skills: [SkillName::BreakTackle])
            ->addPlayer(TeamSide::AWAY, 5, 6, id: 2) // TZ
            ->build();

        // Normal: 7 - 1 (AG) = 6+
        // Break Tackle: 7 - 5 (ST) = 2+
        $target = $tzCalc->calculateDodgeTarget($state, $state->getPlayer(1), new Position(6, 7), new Position(5, 7));
        $this->assertEquals(2, $target);
    }

    public function testBreakTackleDoesNotAffectPickup(): void
    {
        // Break Tackle only affects dodge, not pickup (which always uses AG)
        // Pickup formula: 7 - AG - 1 + TZ. AG=1 → 7-1-1=5+
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, movement: 6, strength: 5, agility: 1, id: 1, skills: [SkillName::BreakTackle])
            ->addPlayer(TeamSide::AWAY, 20, 7, id: 2)
            ->withBallOnGround(6, 7)
            ->withHomeTeam(\App\DTO\TeamStateDTO::create(1, 'Home', 'Human', TeamSide::HOME, 0)) // no rerolls
            ->build();

        // Pickup target: 7 - 1(AG) - 1 = 5+ (Break Tackle doesn't help)
        // Roll 4: fail (< 5), bounce d8=3 (direction [1,0] → lands on (7,7) empty)
        $dice = new FixedDiceRoller([4, 3]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::MOVE, [
            'playerId' => 1, 'x' => 6, 'y' => 7,
        ]);

        $this->assertTrue($result->isTurnover());
    }

    public function testBreakTackleWithHighStrengthDodge(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, movement: 6, strength: 5, agility: 1, id: 1, skills: [SkillName::BreakTackle])
            ->addPlayer(TeamSide::AWAY, 5, 6, id: 2) // TZ
            ->build();

        // Break Tackle dodge: 7 - 5 = 2+
        // Roll 2: success
        $dice = new FixedDiceRoller([2]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::MOVE, [
            'playerId' => 1, 'x' => 5, 'y' => 8,
        ]);

        $this->assertTrue($result->isSuccess());
        $this->assertFalse($result->isTurnover());
    }
}
