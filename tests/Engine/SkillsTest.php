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

final class SkillsTest extends TestCase
{
    // === Tackle ===

    public function testTackleNegatesDodgeOnDefenderStumbles(): void
    {
        // Attacker with Tackle vs Defender with Dodge
        // DEFENDER_STUMBLES should knock down despite Dodge
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 3, skills: [SkillName::Tackle], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, strength: 3, skills: [SkillName::Dodge], id: 2)
            ->build();

        // Roll 5 = DEFENDER_STUMBLES
        // Armour: 3+3=6 vs AV8 - holds
        $dice = new FixedDiceRoller([5, 3, 3]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::BLOCK, [
            'playerId' => 1,
            'targetId' => 2,
        ]);

        $this->assertTrue($result->isSuccess());
        $this->assertFalse($result->isTurnover());

        // Defender should be PRONE (Tackle negates Dodge protection)
        $defender = $result->getNewState()->getPlayer(2);
        $this->assertNotNull($defender);
        $this->assertSame(PlayerState::PRONE, $defender->getState());
    }

    public function testTackleNegatesDodgeSkillReroll(): void
    {
        // Player with Dodge leaves TZ of enemy with Tackle
        // Dodge reroll should NOT be available, team reroll fails too
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, movement: 6, agility: 3, skills: [SkillName::Dodge], id: 1)
            ->addPlayer(TeamSide::AWAY, 5, 4, skills: [SkillName::Tackle], id: 2)
            ->build();

        // Dodge target: 7-3 + 0 -1(Dodge) = 3+
        // Roll 1 (fail), no Dodge reroll due to Tackle, team reroll 1 (fail)
        $dice = new FixedDiceRoller([1, 1]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::MOVE, [
            'playerId' => 1,
            'x' => 5,
            'y' => 6,
        ]);

        $this->assertTrue($result->isTurnover());

        // Verify no Dodge reroll event (only Team Reroll)
        $rerollEvents = array_filter(
            $result->getEvents(),
            fn($e) => $e->getType() === 'reroll',
        );
        foreach ($rerollEvents as $event) {
            $this->assertNotEquals('Dodge', $event->getData()['source']);
        }
    }

    public function testTackleBlocksDodgeRerollButTeamRerollSucceeds(): void
    {
        // Dodge reroll negated by Tackle, but team reroll still works
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, movement: 6, agility: 3, skills: [SkillName::Dodge], id: 1)
            ->addPlayer(TeamSide::AWAY, 5, 4, skills: [SkillName::Tackle], id: 2)
            ->build();

        // Dodge target: 3+. Roll 1 (fail), team reroll 4 (success)
        $dice = new FixedDiceRoller([1, 4]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::MOVE, [
            'playerId' => 1,
            'x' => 5,
            'y' => 6,
        ]);

        $this->assertTrue($result->isSuccess());
        $this->assertFalse($result->isTurnover());

        // Should have Team Reroll event but no Dodge reroll
        $rerollEvents = array_values(array_filter(
            $result->getEvents(),
            fn($e) => $e->getType() === 'reroll',
        ));
        $this->assertCount(1, $rerollEvents);
        $this->assertEquals('Team Reroll', $rerollEvents[0]->getData()['source']);
    }

    // === Frenzy ===

    public function testFrenzySecondBlockAfterPush(): void
    {
        // Attacker with Frenzy: after push, second block happens
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 3, skills: [SkillName::Frenzy], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, strength: 3, id: 2)
            ->build();

        // First block: roll 3 = PUSHED → defender to (7,5), attacker follows to (6,5)
        // Frenzy: second block → roll 6 = DEFENDER_DOWN
        // Armour: 3+3=6 vs AV8 → holds
        $dice = new FixedDiceRoller([3, 6, 3, 3]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::BLOCK, [
            'playerId' => 1,
            'targetId' => 2,
        ]);

        $this->assertTrue($result->isSuccess());
        $this->assertFalse($result->isTurnover());

        // Defender should be knocked down from second block
        $defender = $result->getNewState()->getPlayer(2);
        $this->assertNotNull($defender);
        $this->assertSame(PlayerState::PRONE, $defender->getState());

        // Should have frenzy event
        $frenzyEvents = array_filter($result->getEvents(), fn($e) => $e->getType() === 'frenzy');
        $this->assertCount(1, $frenzyEvents);

        // Should have 2 block events
        $blockEvents = array_filter($result->getEvents(), fn($e) => $e->getType() === 'block');
        $this->assertCount(2, $blockEvents);
    }

    public function testFrenzyNoSecondBlockWhenDefenderDown(): void
    {
        // Frenzy doesn't trigger when defender is knocked down by first block
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 3, skills: [SkillName::Frenzy], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, strength: 3, id: 2)
            ->build();

        // Roll 6 = DEFENDER_DOWN → knocked down, no Frenzy second block
        // Armour: 3+3=6 vs AV8 → holds
        $dice = new FixedDiceRoller([6, 3, 3]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::BLOCK, [
            'playerId' => 1,
            'targetId' => 2,
        ]);

        // No frenzy event (defender is down, can't do second block)
        $frenzyEvents = array_filter($result->getEvents(), fn($e) => $e->getType() === 'frenzy');
        $this->assertCount(0, $frenzyEvents);

        // Only 1 block event
        $blockEvents = array_filter($result->getEvents(), fn($e) => $e->getType() === 'block');
        $this->assertCount(1, $blockEvents);
    }

    public function testFrenzySecondBlockCausesTurnover(): void
    {
        // Frenzy second block results in ATTACKER_DOWN → turnover
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 3, skills: [SkillName::Frenzy], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, strength: 3, id: 2)
            ->build();

        // First block: roll 3 = PUSHED → defender to (7,5), attacker follows to (6,5)
        // Frenzy second block: roll 1 = ATTACKER_DOWN → turnover
        // Attacker armour: 3+3=6 vs AV8 → holds
        $dice = new FixedDiceRoller([3, 1, 3, 3]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::BLOCK, [
            'playerId' => 1,
            'targetId' => 2,
        ]);

        $this->assertTrue($result->isTurnover());

        $attacker = $result->getNewState()->getPlayer(1);
        $this->assertNotNull($attacker);
        $this->assertSame(PlayerState::PRONE, $attacker->getState());
    }

    // === Stand Firm ===

    public function testStandFirmPreventsPushback(): void
    {
        // PUSHED result but defender has Stand Firm → no push, no follow-up
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 3, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, strength: 3, skills: [SkillName::StandFirm], id: 2)
            ->build();

        // Roll 3 = PUSHED → Stand Firm prevents push
        $dice = new FixedDiceRoller([3]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::BLOCK, [
            'playerId' => 1,
            'targetId' => 2,
        ]);

        $this->assertTrue($result->isSuccess());

        // Defender stays at original position
        $defender = $result->getNewState()->getPlayer(2);
        $this->assertNotNull($defender);
        $this->assertSame(PlayerState::STANDING, $defender->getState());
        $this->assertNotNull($defender->getPosition());
        $this->assertSame(6, $defender->getPosition()->getX());
        $this->assertSame(5, $defender->getPosition()->getY());

        // Attacker stays at original position (no follow-up)
        $attacker = $result->getNewState()->getPlayer(1);
        $this->assertNotNull($attacker);
        $this->assertNotNull($attacker->getPosition());
        $this->assertSame(5, $attacker->getPosition()->getX());

        // No push event
        $pushEvents = array_filter($result->getEvents(), fn($e) => $e->getType() === 'push');
        $this->assertCount(0, $pushEvents);
    }

    public function testStandFirmStillAllowsKnockdown(): void
    {
        // DEFENDER_DOWN + Stand Firm → no push but still knocked down in place
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 3, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, strength: 3, skills: [SkillName::StandFirm], id: 2)
            ->build();

        // Roll 6 = DEFENDER_DOWN → Stand Firm prevents push, but knockdown happens
        // Armour: 3+3=6 vs AV8 → holds
        $dice = new FixedDiceRoller([6, 3, 3]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::BLOCK, [
            'playerId' => 1,
            'targetId' => 2,
        ]);

        $this->assertTrue($result->isSuccess());

        // Defender knocked down at original position
        $defender = $result->getNewState()->getPlayer(2);
        $this->assertNotNull($defender);
        $this->assertSame(PlayerState::PRONE, $defender->getState());
        $this->assertNotNull($defender->getPosition());
        $this->assertSame(6, $defender->getPosition()->getX());

        // Attacker stays at (5,5) - no follow-up
        $attacker = $result->getNewState()->getPlayer(1);
        $this->assertNotNull($attacker);
        $this->assertNotNull($attacker->getPosition());
        $this->assertSame(5, $attacker->getPosition()->getX());
    }

    public function testStandFirmWithFrenzy(): void
    {
        // Frenzy + Stand Firm: PUSHED but no actual push.
        // Both still adjacent → Frenzy triggers second block!
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 3, skills: [SkillName::Frenzy], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, strength: 3, skills: [SkillName::StandFirm], id: 2)
            ->build();

        // First block: roll 3 = PUSHED → Stand Firm, no push
        // Both still standing at (5,5) and (6,5), adjacent → Frenzy triggers
        // Second block: roll 6 = DEFENDER_DOWN → Stand Firm prevents push, but knockdown
        // Armour: 3+3=6 vs AV8 → holds
        $dice = new FixedDiceRoller([3, 6, 3, 3]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::BLOCK, [
            'playerId' => 1,
            'targetId' => 2,
        ]);

        $this->assertTrue($result->isSuccess());

        // Frenzy triggered
        $frenzyEvents = array_filter($result->getEvents(), fn($e) => $e->getType() === 'frenzy');
        $this->assertCount(1, $frenzyEvents);

        // Defender knocked down at (6,5) from second block
        $defender = $result->getNewState()->getPlayer(2);
        $this->assertNotNull($defender);
        $this->assertSame(PlayerState::PRONE, $defender->getState());
        $this->assertNotNull($defender->getPosition());
        $this->assertSame(6, $defender->getPosition()->getX());
    }

    // === Strip Ball ===

    public function testStripBallDropsBallOnPush(): void
    {
        // Attacker with Strip Ball pushes ball carrier → ball stripped
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 3, skills: [SkillName::StripBall], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, strength: 3, id: 2)
            ->withBallCarried(2)
            ->build();

        // Roll 3 = PUSHED → defender to (7,5), ball stripped there
        // D8=1 (North) for ball bounce from (7,5) to (7,4)
        $dice = new FixedDiceRoller([3, 1]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::BLOCK, [
            'playerId' => 1,
            'targetId' => 2,
        ]);

        $this->assertTrue($result->isSuccess());

        // Ball should not be held by defender
        $ball = $result->getNewState()->getBall();
        $this->assertNotEquals(2, $ball->getCarrierId());

        // Should have strip_ball event
        $stripEvents = array_filter($result->getEvents(), fn($e) => $e->getType() === 'strip_ball');
        $this->assertCount(1, $stripEvents);
    }

    public function testStripBallNoEffectWithoutBall(): void
    {
        // Attacker has Strip Ball but defender doesn't carry ball → normal push
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 3, skills: [SkillName::StripBall], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, strength: 3, id: 2)
            ->build();

        // Roll 3 = PUSHED
        $dice = new FixedDiceRoller([3]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::BLOCK, [
            'playerId' => 1,
            'targetId' => 2,
        ]);

        $this->assertTrue($result->isSuccess());

        // No strip_ball event
        $stripEvents = array_filter($result->getEvents(), fn($e) => $e->getType() === 'strip_ball');
        $this->assertCount(0, $stripEvents);

        // Defender pushed normally
        $defender = $result->getNewState()->getPlayer(2);
        $this->assertNotNull($defender);
        $this->assertNotNull($defender->getPosition());
        $this->assertSame(7, $defender->getPosition()->getX());
    }

    // === Side Step ===

    public function testSideStepPrefersSaferSquare(): void
    {
        // Defender with Side Step should be pushed to the square with fewer enemy TZs
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 3, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, strength: 3, skills: [SkillName::SideStep], id: 2)
            ->addPlayer(TeamSide::HOME, 8, 4, id: 3) // creates TZ at (7,5) and (7,4), but NOT (7,6)
            ->build();

        // Push direction: attacker at (5,5), defender at (6,5) → push to right
        // Push squares: (7,5) direct, (7,4) upper, (7,6) lower
        // TZ from player 3 at (8,4): adjacent to (7,5)=1TZ, (7,4)=1TZ, (7,6) distance=max(1,2)=2 → 0TZ
        // Side Step picks (7,6) with 0 TZ

        // Roll 3 = PUSHED
        $dice = new FixedDiceRoller([3]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::BLOCK, [
            'playerId' => 1,
            'targetId' => 2,
        ]);

        $this->assertTrue($result->isSuccess());

        $defender = $result->getNewState()->getPlayer(2);
        $this->assertNotNull($defender);
        $this->assertNotNull($defender->getPosition());
        // Should be at (7,6) - safest square
        $this->assertSame(7, $defender->getPosition()->getX());
        $this->assertSame(6, $defender->getPosition()->getY());
    }

    public function testWithoutSideStepPicksSmartSquare(): void
    {
        // Same setup but without Side Step → smart push picks worst for defender
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 3, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, strength: 3, id: 2) // no Side Step
            ->addPlayer(TeamSide::HOME, 8, 4, id: 3)
            ->build();

        // Roll 3 = PUSHED
        $dice = new FixedDiceRoller([3]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::BLOCK, [
            'playerId' => 1,
            'targetId' => 2,
        ]);

        $defender = $result->getNewState()->getPlayer(2);
        $this->assertNotNull($defender);
        $this->assertNotNull($defender->getPosition());
        // Smart push: (7,4) and (7,5) both have 1 TZ from player 3; (7,4) is closer to sideline
        $this->assertSame(7, $defender->getPosition()->getX());
        $this->assertSame(4, $defender->getPosition()->getY());
    }
}
