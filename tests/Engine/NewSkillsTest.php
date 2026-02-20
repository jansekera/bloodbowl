<?php

declare(strict_types=1);

namespace App\Tests\Engine;

use App\Engine\ActionResolver;
use App\Engine\BallResolver;
use App\Engine\FixedDiceRoller;
use App\Engine\InjuryResolver;
use App\Engine\ScatterCalculator;
use App\Engine\StrengthCalculator;
use App\Engine\TacklezoneCalculator;
use App\Engine\PassResolver;
use App\Enum\ActionType;
use App\Enum\PlayerState;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use PHPUnit\Framework\TestCase;

final class NewSkillsTest extends TestCase
{
    // --- Sure Feet ---

    public function testSureFeetRerollsFailedGfi(): void
    {
        // Player with Sure Feet, movement=6, moving 8 squares (2 GFI)
        // Path: (5,7) -> (7,7) movement, then GFI (8,7)
        // GFI: first roll=1 (fail), Sure Feet reroll=2 (success)
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 3, 7, movement: 6, id: 1, skills: [SkillName::SureFeet])
            ->addPlayer(TeamSide::AWAY, 20, 7, id: 2)
            ->build();

        // Move to (10,7) = 7 squares, requires 1 GFI
        // GFI rolls: 1 (fail), Sure Feet reroll: 3 (success)
        $dice = new FixedDiceRoller([1, 3]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::MOVE, [
            'playerId' => 1, 'x' => 10, 'y' => 7,
        ]);

        $this->assertTrue($result->isSuccess());
        $pos = $result->getNewState()->getPlayer(1)->getPosition();
        $this->assertEquals(10, $pos->getX());

        // Verify Sure Feet reroll event
        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('reroll', $types);
    }

    public function testSureFeetDoesNotPreventTeamRerollIfFailed(): void
    {
        // Sure Feet fails -> no team reroll (skill was used)
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 3, 7, movement: 6, id: 1, skills: [SkillName::SureFeet])
            ->addPlayer(TeamSide::AWAY, 20, 7, id: 2)
            ->build();

        // Move to (10,7) = 7 squares, 1 GFI
        // GFI: 1 (fail), Sure Feet: 1 (fail again) -> player falls (turnover)
        // No further team reroll since skill reroll was used
        $dice = new FixedDiceRoller([1, 1]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::MOVE, [
            'playerId' => 1, 'x' => 10, 'y' => 7,
        ]);

        $this->assertTrue($result->isTurnover());
    }

    public function testSureFeetNotUsedOnDodge(): void
    {
        // Sure Feet only works on GFI, not dodge
        // Player with SureFeet but no Dodge skill
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, movement: 6, id: 1, skills: [SkillName::SureFeet])
            ->addPlayer(TeamSide::AWAY, 6, 7, id: 2) // enemy tackle zone
            ->build();

        // Move requires dodge: roll 1 (fail), then team reroll (not skill)
        // since SureFeet doesn't apply to dodges
        $dice = new FixedDiceRoller([1, 4]); // dodge fail, team reroll success
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::MOVE, [
            'playerId' => 1, 'x' => 5, 'y' => 6,
        ]);

        // The team reroll should have been used (not Sure Feet)
        $rerollEvents = array_filter(
            $result->getEvents(),
            fn($e) => $e->getType() === 'reroll',
        );
        foreach ($rerollEvents as $e) {
            $this->assertEquals('Team Reroll', $e->getData()['source']);
        }
    }

    // --- Nerves of Steel ---

    public function testNervesOfSteelIgnoresTzForPassAccuracy(): void
    {
        // Thrower surrounded by enemies but with Nerves of Steel
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, agility: 3, id: 1, skills: [SkillName::NervesOfSteel])
            ->addPlayer(TeamSide::HOME, 9, 7, agility: 3, id: 3) // catcher
            ->addPlayer(TeamSide::AWAY, 5, 6, id: 4) // enemy in TZ
            ->addPlayer(TeamSide::AWAY, 5, 8, id: 5) // enemy in TZ
            ->withBallCarried(1)
            ->build();

        $passResolver = new PassResolver(
            new FixedDiceRoller([]),
            new TacklezoneCalculator(),
            new ScatterCalculator(),
            new BallResolver(new FixedDiceRoller([]), new TacklezoneCalculator(), new ScatterCalculator()),
        );

        $range = \App\Enum\PassRange::fromDistance(3); // quick pass, modifier +1
        $target = $passResolver->getAccuracyTarget($state, $state->getPlayer(1), $range);

        // Without NervesOfSteel: 7 - 3 + 2(TZ) - 1(quick) = 5
        // With NervesOfSteel: 7 - 3 + 0 - 1(quick) = 3
        $this->assertEquals(3, $target);
    }

    public function testNervesOfSteelIgnoresTzForCatch(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, agility: 3, id: 1, skills: [SkillName::NervesOfSteel])
            ->addPlayer(TeamSide::AWAY, 5, 6, id: 2) // enemy in TZ
            ->addPlayer(TeamSide::AWAY, 5, 8, id: 3) // enemy in TZ
            ->withBallOnGround(5, 7)
            ->build();

        $ballResolver = new BallResolver(
            new FixedDiceRoller([]),
            new TacklezoneCalculator(),
            new ScatterCalculator(),
        );

        $target = $ballResolver->getCatchTarget($state, $state->getPlayer(1));

        // Without NervesOfSteel: 7 - 3 + 2 = 6
        // With NervesOfSteel: 7 - 3 + 0 = 4
        $this->assertEquals(4, $target);
    }

    public function testNervesOfSteelIgnoresTzForPickup(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, agility: 3, id: 1, skills: [SkillName::NervesOfSteel])
            ->addPlayer(TeamSide::AWAY, 5, 6, id: 2)
            ->addPlayer(TeamSide::AWAY, 5, 8, id: 3)
            ->withBallOnGround(5, 7)
            ->build();

        $ballResolver = new BallResolver(
            new FixedDiceRoller([]),
            new TacklezoneCalculator(),
            new ScatterCalculator(),
        );

        $target = $ballResolver->getPickupTarget($state, $state->getPlayer(1));

        // Without NervesOfSteel: 7 - 3 - 1 + 2 = 5
        // With NervesOfSteel: 7 - 3 - 1 + 0 = 3
        $this->assertEquals(3, $target);
    }

    // --- Thick Skull ---

    public function testThickSkullConvertsKoToStunned(): void
    {
        $player = \App\DTO\MatchPlayerDTO::create(
            id: 1, playerId: 1, name: 'Test', number: 1,
            positionalName: 'Lineman',
            stats: new \App\ValueObject\PlayerStats(6, 3, 3, 8),
            skills: [SkillName::ThickSkull],
            teamSide: TeamSide::HOME,
            position: new \App\ValueObject\Position(5, 7),
        )->withState(PlayerState::PRONE);

        $injuryResolver = new InjuryResolver();

        // Armor broken (2D6 > 8), then injury=KO (roll 8 or 9)
        // Armor: 5+4=9 > 8 (broken)
        // Injury: 4+4=8 (KO range)
        // Thick Skull: roll 4 (>= 4, converts to stunned)
        $dice = new FixedDiceRoller([5, 4, 4, 4, 4]);
        $result = $injuryResolver->resolve($player, $dice);

        $this->assertEquals(PlayerState::STUNNED, $result['player']->getState());
    }

    public function testThickSkullFailsToConvert(): void
    {
        $player = \App\DTO\MatchPlayerDTO::create(
            id: 1, playerId: 1, name: 'Test', number: 1,
            positionalName: 'Lineman',
            stats: new \App\ValueObject\PlayerStats(6, 3, 3, 8),
            skills: [SkillName::ThickSkull],
            teamSide: TeamSide::HOME,
            position: new \App\ValueObject\Position(5, 7),
        )->withState(PlayerState::PRONE);

        $injuryResolver = new InjuryResolver();

        // Armor: 5+4=9 > 8 (broken)
        // Injury: 4+4=8 (KO)
        // Thick Skull: roll 3 (< 4, stays KO)
        $dice = new FixedDiceRoller([5, 4, 4, 4, 3]);
        $result = $injuryResolver->resolve($player, $dice);

        $this->assertEquals(PlayerState::KO, $result['player']->getState());
    }

    public function testThickSkullDoesNotApplyToCasualty(): void
    {
        $player = \App\DTO\MatchPlayerDTO::create(
            id: 1, playerId: 1, name: 'Test', number: 1,
            positionalName: 'Lineman',
            stats: new \App\ValueObject\PlayerStats(6, 3, 3, 8),
            skills: [SkillName::ThickSkull],
            teamSide: TeamSide::HOME,
            position: new \App\ValueObject\Position(5, 7),
        )->withState(PlayerState::PRONE);

        $injuryResolver = new InjuryResolver();

        // Armor: 5+4=9 > 8 (broken)
        // Injury: 6+5=11 (casualty, > 9)
        // Thick Skull doesn't activate for casualty
        $dice = new FixedDiceRoller([5, 4, 6, 5]);
        $result = $injuryResolver->resolve($player, $dice);

        $this->assertEquals(PlayerState::INJURED, $result['player']->getState());
    }

    public function testThickSkullDoesNotApplyToStunned(): void
    {
        $player = \App\DTO\MatchPlayerDTO::create(
            id: 1, playerId: 1, name: 'Test', number: 1,
            positionalName: 'Lineman',
            stats: new \App\ValueObject\PlayerStats(6, 3, 3, 8),
            skills: [SkillName::ThickSkull],
            teamSide: TeamSide::HOME,
            position: new \App\ValueObject\Position(5, 7),
        )->withState(PlayerState::PRONE);

        $injuryResolver = new InjuryResolver();

        // Armor: 5+4=9 > 8 (broken)
        // Injury: 3+3=6 (stunned, <= 7)
        // Thick Skull doesn't need to activate
        $dice = new FixedDiceRoller([5, 4, 3, 3]);
        $result = $injuryResolver->resolve($player, $dice);

        $this->assertEquals(PlayerState::STUNNED, $result['player']->getState());
    }

    // --- Horns ---

    public function testHornsGivesStrengthBonusDuringBlitz(): void
    {
        // ST3 attacker with Horns blitzes ST4 defender
        // Without Horns: ST3 vs ST4 = 2 dice defender chooses
        // With Horns: ST4 vs ST4 = 1 die
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 4, 7, strength: 3, id: 1, skills: [SkillName::Horns])
            ->addPlayer(TeamSide::AWAY, 6, 7, strength: 4, id: 2)
            ->build();

        // Block die: roll 3 = Pushed (no knockdown, so no injury rolls needed)
        $dice = new FixedDiceRoller([3]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::BLITZ, [
            'playerId' => 1, 'targetId' => 2,
        ]);

        $blockEvents = array_filter($result->getEvents(), fn($e) => $e->getType() === 'block');
        $blockEvent = array_values($blockEvents)[0] ?? null;
        $this->assertNotNull($blockEvent);
        // With Horns: ST4 vs ST4 = 1 die, attacker chooses
        $this->assertEquals(1, $blockEvent->getData()['diceCount']);
        $this->assertTrue($blockEvent->getData()['attackerChooses']);
    }

    public function testHornsDoesNotApplyToNormalBlock(): void
    {
        // ST3 attacker with Horns blocks ST4 defender (normal block, not blitz)
        // Without blitz, Horns doesn't apply: ST3 vs ST4 = 2 dice defender chooses
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, strength: 3, id: 1, skills: [SkillName::Horns])
            ->addPlayer(TeamSide::AWAY, 6, 7, strength: 4, id: 2)
            ->build();

        // 2 dice defender chooses: rolls 3,4 = both Pushed
        // Defender chooses Pushed
        $dice = new FixedDiceRoller([3, 4]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::BLOCK, [
            'playerId' => 1, 'targetId' => 2,
        ]);

        $blockEvents = array_filter($result->getEvents(), fn($e) => $e->getType() === 'block');
        $blockEvent = array_values($blockEvents)[0] ?? null;
        $this->assertNotNull($blockEvent);
        // Without Horns bonus: 2 dice, defender chooses
        $this->assertEquals(2, $blockEvent->getData()['diceCount']);
        $this->assertFalse($blockEvent->getData()['attackerChooses']);
    }

    // --- Dauntless ---

    public function testDauntlessSuccessEqualizesStrength(): void
    {
        // ST3 attacker with Dauntless blocks ST4 defender
        // Dauntless: roll D6+3, if >= 4 -> treat as equal
        // Roll 2 -> 2+3 = 5 >= 4, success! Treated as ST4 vs ST4 = 1 die
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, strength: 3, id: 1, skills: [SkillName::Dauntless])
            ->addPlayer(TeamSide::AWAY, 6, 7, strength: 4, id: 2)
            ->build();

        // Dauntless roll: 2 (2+3=5 >= 4), then 1 block die: roll 3 (Pushed, no injury)
        $dice = new FixedDiceRoller([2, 3]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::BLOCK, [
            'playerId' => 1, 'targetId' => 2,
        ]);

        $blockEvents = array_filter($result->getEvents(), fn($e) => $e->getType() === 'block');
        $blockEvent = array_values($blockEvents)[0] ?? null;
        $this->assertNotNull($blockEvent);
        // Dauntless equalized: 1 die, attacker chooses
        $this->assertEquals(1, $blockEvent->getData()['diceCount']);
        $this->assertTrue($blockEvent->getData()['attackerChooses']);
    }

    public function testDauntlessFailureKeepsDisadvantage(): void
    {
        // ST3 attacker with Dauntless blocks ST4 defender
        // Dauntless: roll D6+3, if < 4 -> no effect
        // Roll 0 is impossible on D6, but roll 1 -> 1+3 = 4 >= 4 is success
        // So we need to test the exact boundary. ST3 vs ST5:
        // Dauntless roll 1 -> 1+3=4 < 5, fail
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, strength: 3, id: 1, skills: [SkillName::Dauntless])
            ->addPlayer(TeamSide::AWAY, 6, 7, strength: 5, id: 2)
            ->build();

        // Dauntless roll: 1 (1+3=4 < 5, fail), then 2 block dice defender chooses: 3, 3 (both Pushed)
        $dice = new FixedDiceRoller([1, 3, 3]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::BLOCK, [
            'playerId' => 1, 'targetId' => 2,
        ]);

        $blockEvents = array_filter($result->getEvents(), fn($e) => $e->getType() === 'block');
        $blockEvent = array_values($blockEvents)[0] ?? null;
        $this->assertNotNull($blockEvent);
        // Dauntless failed: still 2 dice defender chooses
        $this->assertEquals(2, $blockEvent->getData()['diceCount']);
        $this->assertFalse($blockEvent->getData()['attackerChooses']);
    }

    public function testDauntlessNotUsedWhenStronger(): void
    {
        // ST4 attacker with Dauntless blocks ST3 defender - Dauntless shouldn't trigger
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, strength: 4, id: 1, skills: [SkillName::Dauntless])
            ->addPlayer(TeamSide::AWAY, 6, 7, strength: 3, id: 2)
            ->build();

        // No dauntless roll, just block: 2 dice attacker chooses
        // Roll 3, 3 = both Pushed (no injury rolls needed)
        $dice = new FixedDiceRoller([3, 3]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::BLOCK, [
            'playerId' => 1, 'targetId' => 2,
        ]);

        $blockEvents = array_filter($result->getEvents(), fn($e) => $e->getType() === 'block');
        $blockEvent = array_values($blockEvents)[0] ?? null;
        $this->assertNotNull($blockEvent);
        $this->assertEquals(2, $blockEvent->getData()['diceCount']);
        $this->assertTrue($blockEvent->getData()['attackerChooses']);
    }
}
