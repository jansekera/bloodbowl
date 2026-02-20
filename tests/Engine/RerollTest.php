<?php

declare(strict_types=1);

namespace App\Tests\Engine;

use App\DTO\TeamStateDTO;
use App\Engine\ActionResolver;
use App\Engine\BallResolver;
use App\Engine\FixedDiceRoller;
use App\Engine\ScatterCalculator;
use App\Engine\TacklezoneCalculator;
use App\Enum\ActionType;
use App\Enum\PlayerState;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use PHPUnit\Framework\TestCase;

final class RerollTest extends TestCase
{
    // === Dodge Rerolls ===

    public function testDodgeSkillRerollOnFailedDodge(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, movement: 6, agility: 3, skills: [SkillName::Dodge], id: 1)
            ->addPlayer(TeamSide::AWAY, 5, 4) // creates tackle zone
            ->build();

        // Dodge target: 7-3-1(Dodge skill)=3+. Roll 2=fail, Dodge reroll 4=success
        $dice = new FixedDiceRoller([2, 4]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::MOVE, [
            'playerId' => 1, 'x' => 5, 'y' => 6,
        ]);

        $this->assertTrue($result->isSuccess());
        $player = $result->getNewState()->getPlayer(1);
        $this->assertNotNull($player);
        $this->assertEquals(5, $player->getPosition()?->getX());
        $this->assertEquals(6, $player->getPosition()?->getY());

        // Check reroll event was emitted
        $rerollEvents = array_filter($result->getEvents(), fn($e) => $e->getType() === 'reroll');
        $this->assertCount(1, $rerollEvents);
        $rerollEvent = array_values($rerollEvents)[0];
        $this->assertEquals('Dodge', $rerollEvent->getData()['source']);
    }

    public function testDodgeSkillRerollFailNoTeamReroll(): void
    {
        // Dodge skill reroll fails → team reroll should NOT be used (can't reroll a reroll)
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, movement: 6, agility: 3, skills: [SkillName::Dodge], id: 1)
            ->addPlayer(TeamSide::AWAY, 5, 4)
            ->build();

        // Dodge target 3+. Roll 1=fail, Dodge reroll 2=fail → turnover (no team reroll)
        $dice = new FixedDiceRoller([1, 2]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::MOVE, [
            'playerId' => 1, 'x' => 5, 'y' => 6,
        ]);

        $this->assertTrue($result->isTurnover());

        // Team rerolls should NOT be decremented (was 3, still 3)
        $homeTeam = $result->getNewState()->getHomeTeam();
        $this->assertEquals(3, $homeTeam->getRerolls());
        $this->assertFalse($homeTeam->isRerollUsedThisTurn());
    }

    public function testDodgeFailTeamRerollSucceeds(): void
    {
        // No Dodge skill → team reroll available
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, movement: 6, agility: 3, id: 1)
            ->addPlayer(TeamSide::AWAY, 5, 4)
            ->build();

        // Dodge target 4+ (no Dodge skill). Roll 2=fail, team reroll 5=success
        $dice = new FixedDiceRoller([2, 5]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::MOVE, [
            'playerId' => 1, 'x' => 5, 'y' => 6,
        ]);

        $this->assertTrue($result->isSuccess());

        // Team rerolls decremented: was 3, now 2
        $homeTeam = $result->getNewState()->getHomeTeam();
        $this->assertEquals(2, $homeTeam->getRerolls());
        $this->assertTrue($homeTeam->isRerollUsedThisTurn());

        // Check reroll event
        $rerollEvents = array_filter($result->getEvents(), fn($e) => $e->getType() === 'reroll');
        $this->assertCount(1, $rerollEvents);
        $rerollEvent = array_values($rerollEvents)[0];
        $this->assertEquals('Team Reroll', $rerollEvent->getData()['source']);
    }

    public function testDodgeFailTeamRerollFails(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, movement: 6, agility: 3, id: 1)
            ->addPlayer(TeamSide::AWAY, 5, 4)
            ->build();

        // Dodge target 4+. Roll 2=fail, team reroll 3=fail → turnover
        $dice = new FixedDiceRoller([2, 3]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::MOVE, [
            'playerId' => 1, 'x' => 5, 'y' => 6,
        ]);

        $this->assertTrue($result->isTurnover());
        $player = $result->getNewState()->getPlayer(1);
        $this->assertNotNull($player);
        $this->assertEquals(PlayerState::PRONE, $player->getState());

        // Team rerolls decremented even though it failed
        $homeTeam = $result->getNewState()->getHomeTeam();
        $this->assertEquals(2, $homeTeam->getRerolls());
    }

    // === GFI Rerolls ===

    public function testGfiFailTeamRerollSucceeds(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, movement: 1, id: 1)
            ->build();

        // Move 2 squares with MA=1 → second square is GFI
        // GFI needs 2+. Roll 1=fail, team reroll 3=success
        $dice = new FixedDiceRoller([1, 3]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::MOVE, [
            'playerId' => 1, 'x' => 7, 'y' => 5,
        ]);

        $this->assertTrue($result->isSuccess());

        $homeTeam = $result->getNewState()->getHomeTeam();
        $this->assertEquals(2, $homeTeam->getRerolls());
        $this->assertTrue($homeTeam->isRerollUsedThisTurn());
    }

    public function testGfiFailTeamRerollFails(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, movement: 1, id: 1)
            ->build();

        // GFI needs 2+. Roll 1=fail, team reroll 1=fail → turnover
        $dice = new FixedDiceRoller([1, 1]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::MOVE, [
            'playerId' => 1, 'x' => 7, 'y' => 5,
        ]);

        $this->assertTrue($result->isTurnover());
        $homeTeam = $result->getNewState()->getHomeTeam();
        $this->assertEquals(2, $homeTeam->getRerolls());
    }

    // === Pickup Rerolls ===

    public function testPickupFailTeamRerollSucceeds(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, movement: 6, agility: 3, id: 1)
            ->withBallOnGround(6, 5)
            ->build();

        // Pickup target 3+. Roll 2=fail, team reroll 4=success
        $dice = new FixedDiceRoller([2, 4]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::MOVE, [
            'playerId' => 1, 'x' => 6, 'y' => 5,
        ]);

        $this->assertTrue($result->isSuccess());
        $this->assertTrue($result->getNewState()->getBall()->isHeld());
        $this->assertEquals(1, $result->getNewState()->getBall()->getCarrierId());

        $homeTeam = $result->getNewState()->getHomeTeam();
        $this->assertEquals(2, $homeTeam->getRerolls());
    }

    public function testPickupSureHandsUsedNoTeamReroll(): void
    {
        // Sure Hands used → team reroll should NOT be used
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, movement: 6, agility: 3, skills: [SkillName::SureHands], id: 1)
            ->withBallOnGround(6, 5)
            ->build();

        // Pickup target 3+. Roll 2=fail, Sure Hands reroll 1=fail, bounce D8=3
        $dice = new FixedDiceRoller([2, 1, 3]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::MOVE, [
            'playerId' => 1, 'x' => 6, 'y' => 5,
        ]);

        $this->assertTrue($result->isTurnover());

        // Team rerolls NOT used (still 3)
        $homeTeam = $result->getNewState()->getHomeTeam();
        $this->assertEquals(3, $homeTeam->getRerolls());
        $this->assertFalse($homeTeam->isRerollUsedThisTurn());
    }

    // === Catch Rerolls (Hand-Off) ===

    public function testHandOffCatchFailTeamRerollSucceeds(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, agility: 3, id: 1)
            ->addPlayer(TeamSide::HOME, 6, 5, agility: 3, id: 2)
            ->withBallCarried(1)
            ->build();

        // Hand-off catch target: 7-3-1(modifier)=3+. Roll 2=fail, team reroll 4=success
        $dice = new FixedDiceRoller([2, 4]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::HAND_OFF, [
            'playerId' => 1, 'targetId' => 2,
        ]);

        $this->assertTrue($result->isSuccess());
        $this->assertEquals(2, $result->getNewState()->getBall()->getCarrierId());

        $homeTeam = $result->getNewState()->getHomeTeam();
        $this->assertEquals(2, $homeTeam->getRerolls());
    }

    public function testCatchSkillUsedNoTeamReroll(): void
    {
        // Catch skill used → team reroll should NOT be used
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, agility: 3, id: 1)
            ->addPlayer(TeamSide::HOME, 6, 5, agility: 3, skills: [SkillName::Catch], id: 2)
            ->withBallCarried(1)
            ->build();

        // Hand-off catch target 3+. Roll 2=fail, Catch reroll 1=fail, bounce D8=3
        $dice = new FixedDiceRoller([2, 1, 3]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::HAND_OFF, [
            'playerId' => 1, 'targetId' => 2,
        ]);

        $this->assertTrue($result->isTurnover());

        // Team rerolls NOT used
        $homeTeam = $result->getNewState()->getHomeTeam();
        $this->assertEquals(3, $homeTeam->getRerolls());
    }

    // === Pass Rerolls ===

    public function testPassSkillRerollOnFailedAccuracy(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, agility: 3, skills: [SkillName::Pass], id: 1)
            ->addPlayer(TeamSide::HOME, 10, 5, agility: 3, id: 2)
            ->withBallCarried(1)
            ->build();

        // Short pass: accuracy 7-3-1(Pass skill)=3+. Roll 2=fail, Pass reroll 5=accurate
        // Catch: 7-3+0-1(accurate)=3+, roll 4=success
        $dice = new FixedDiceRoller([2, 5, 4]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::PASS, [
            'playerId' => 1, 'targetX' => 10, 'targetY' => 5,
        ]);

        $this->assertTrue($result->isSuccess());
        $this->assertEquals(2, $result->getNewState()->getBall()->getCarrierId());

        // Team rerolls NOT used (Pass skill reroll was used)
        $homeTeam = $result->getNewState()->getHomeTeam();
        $this->assertEquals(3, $homeTeam->getRerolls());
    }

    public function testPassTeamRerollOnFailedAccuracy(): void
    {
        // No Pass skill → team reroll
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, agility: 3, id: 1)
            ->addPlayer(TeamSide::HOME, 10, 5, agility: 3, id: 2)
            ->withBallCarried(1)
            ->build();

        // Short pass: accuracy 7-3=4+. Roll 3=fail, team reroll 5=accurate
        // Catch: 3+, roll 4=success
        $dice = new FixedDiceRoller([3, 5, 4]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::PASS, [
            'playerId' => 1, 'targetX' => 10, 'targetY' => 5,
        ]);

        $this->assertTrue($result->isSuccess());
        $this->assertEquals(2, $result->getNewState()->getBall()->getCarrierId());

        $homeTeam = $result->getNewState()->getHomeTeam();
        $this->assertEquals(2, $homeTeam->getRerolls());
    }

    // === Edge Cases ===

    public function testNoTeamRerollWhenZeroRerolls(): void
    {
        $state = (new GameStateBuilder())
            ->withHomeTeam(TeamStateDTO::create(1, 'Home', 'Human', TeamSide::HOME, 0))
            ->addPlayer(TeamSide::HOME, 5, 5, movement: 6, agility: 3, id: 1)
            ->addPlayer(TeamSide::AWAY, 5, 4)
            ->build();

        // Dodge target 4+. Roll 2=fail → no team reroll (0 rerolls) → turnover
        $dice = new FixedDiceRoller([2]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::MOVE, [
            'playerId' => 1, 'x' => 5, 'y' => 6,
        ]);

        $this->assertTrue($result->isTurnover());
        $homeTeam = $result->getNewState()->getHomeTeam();
        $this->assertEquals(0, $homeTeam->getRerolls());
    }

    public function testOnlyOneTeamRerollPerTurn(): void
    {
        // Use team reroll on dodge, then GFI should not get another
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, movement: 3, agility: 3, id: 1)
            ->addPlayer(TeamSide::AWAY, 5, 4) // creates TZ
            ->build();

        // Move to (9,5) = 4 steps, first requires dodge (in TZ), last is GFI (MA=3)
        // Dodge: roll 2=fail, team reroll 5=success → reroll used
        // Steps 2,3: no dodge
        // GFI: roll 1=fail → no team reroll (already used) → turnover
        $dice = new FixedDiceRoller([2, 5, 1]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::MOVE, [
            'playerId' => 1, 'x' => 9, 'y' => 5,
        ]);

        $this->assertTrue($result->isTurnover());

        // Team reroll was used (decremented from 3 to 2)
        $homeTeam = $result->getNewState()->getHomeTeam();
        $this->assertEquals(2, $homeTeam->getRerolls());
    }

    // === BallResolver Direct Tests ===

    public function testBallResolverPickupTeamReroll(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 10, 7, agility: 3, id: 1)
            ->withBallOnGround(10, 7)
            ->build();

        $player = $state->getPlayer(1);
        $this->assertNotNull($player);

        // Roll 2=fail (target 3+), team reroll 4=success
        $dice = new FixedDiceRoller([2, 4]);
        $resolver = new BallResolver($dice, new TacklezoneCalculator(), new ScatterCalculator());

        $result = $resolver->resolvePickup($state, $player, teamRerollAvailable: true);

        $this->assertTrue($result['success']);
        $this->assertTrue($result['teamRerollUsed']);
        $this->assertTrue($result['state']->getBall()->isHeld());
    }

    public function testBallResolverCatchTeamReroll(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 10, 7, agility: 3, id: 1)
            ->withBallOnGround(10, 7)
            ->build();

        $player = $state->getPlayer(1);
        $this->assertNotNull($player);

        // Catch target 4+. Roll 3=fail, team reroll 5=success
        $dice = new FixedDiceRoller([3, 5]);
        $resolver = new BallResolver($dice, new TacklezoneCalculator(), new ScatterCalculator());

        $result = $resolver->resolveCatch($state, $player, teamRerollAvailable: true);

        $this->assertTrue($result['success']);
        $this->assertTrue($result['teamRerollUsed']);
    }
}
