<?php
declare(strict_types=1);

namespace App\Tests\Engine;

use App\Engine\ActionResolver;
use App\Engine\BallResolver;
use App\Engine\FixedDiceRoller;
use App\Engine\InjuryResolver;
use App\Engine\Pathfinder;
use App\Engine\ScatterCalculator;
use App\Engine\TacklezoneCalculator;
use App\Enum\ActionType;
use App\Enum\PlayerState;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use PHPUnit\Framework\TestCase;

final class BigGuySkillsTest extends TestCase
{
    // === Bone-head ===

    public function testBoneHeadFailLosesAction(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1, skills: [SkillName::BoneHead])
            ->addPlayer(TeamSide::AWAY, 20, 7, id: 2)
            ->build();

        // Bone-head roll: 1 (fail)
        $dice = new FixedDiceRoller([1]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::MOVE, [
            'playerId' => 1, 'x' => 6, 'y' => 7,
        ]);

        $this->assertTrue($result->isSuccess()); // not a turnover, just lost action
        $player = $result->getNewState()->getPlayer(1);
        $this->assertTrue($player->hasMoved());
        $this->assertTrue($player->hasActed());
        $this->assertTrue($player->hasLostTacklezones());

        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('bone_head', $types);
    }

    public function testBoneHeadPassActionProceeds(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, movement: 6, id: 1, skills: [SkillName::BoneHead])
            ->addPlayer(TeamSide::AWAY, 20, 7, id: 2)
            ->build();

        // Bone-head roll: 2 (pass), then no more rolls needed for simple move
        $dice = new FixedDiceRoller([2]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::MOVE, [
            'playerId' => 1, 'x' => 6, 'y' => 7,
        ]);

        $this->assertTrue($result->isSuccess());
        $player = $result->getNewState()->getPlayer(1);
        $this->assertEquals(6, $player->getPosition()->getX());
        $this->assertFalse($player->hasLostTacklezones());
    }

    public function testBoneHeadLostTzResetOnNewTurn(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1, skills: [SkillName::BoneHead])
            ->addPlayer(TeamSide::AWAY, 20, 7, id: 2)
            ->build();

        // Bone-head fail
        $dice = new FixedDiceRoller([1]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::MOVE, [
            'playerId' => 1, 'x' => 6, 'y' => 7,
        ]);

        $newState = $result->getNewState();
        $this->assertTrue($newState->getPlayer(1)->hasLostTacklezones());

        // Reset for new turn
        $resetState = $newState->resetPlayersForNewTurn(TeamSide::HOME);
        $this->assertFalse($resetState->getPlayer(1)->hasLostTacklezones());
    }

    public function testBoneHeadLostTzDoesNotExertTacklezone(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1, skills: [SkillName::BoneHead])
            ->addPlayer(TeamSide::AWAY, 6, 7, id: 2) // adjacent enemy
            ->build();

        // Manually set lostTacklezones
        $player = $state->getPlayer(1)->withLostTacklezones(true);
        $state = $state->withPlayer($player);

        $tzCalc = new TacklezoneCalculator();
        $tz = $tzCalc->countTacklezones($state, $state->getPlayer(2)->getPosition(), TeamSide::AWAY);
        $this->assertEquals(0, $tz); // Bone-headed player doesn't exert TZ
    }

    // === Really Stupid ===

    public function testReallyStupidFailNoAlly(): void
    {
        // No adjacent teammate: needs 4+
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1, skills: [SkillName::ReallyStupid])
            ->addPlayer(TeamSide::AWAY, 20, 7, id: 2)
            ->build();

        // Roll 3 (< 4, fail)
        $dice = new FixedDiceRoller([3]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::MOVE, [
            'playerId' => 1, 'x' => 6, 'y' => 7,
        ]);

        $this->assertTrue($result->isSuccess());
        $player = $result->getNewState()->getPlayer(1);
        $this->assertTrue($player->hasActed());
        $this->assertTrue($player->hasLostTacklezones());
    }

    public function testReallyStupidPassNoAlly(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, movement: 6, id: 1, skills: [SkillName::ReallyStupid])
            ->addPlayer(TeamSide::AWAY, 20, 7, id: 2)
            ->build();

        // Roll 4 (>= 4, pass)
        $dice = new FixedDiceRoller([4]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::MOVE, [
            'playerId' => 1, 'x' => 6, 'y' => 7,
        ]);

        $this->assertTrue($result->isSuccess());
        $player = $result->getNewState()->getPlayer(1);
        $this->assertEquals(6, $player->getPosition()->getX());
    }

    public function testReallyStupidFailWithAlly(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1, skills: [SkillName::ReallyStupid])
            ->addPlayer(TeamSide::HOME, 5, 6, id: 3) // adjacent ally
            ->addPlayer(TeamSide::AWAY, 20, 7, id: 2)
            ->build();

        // Roll 1 (< 2, fail even with ally)
        $dice = new FixedDiceRoller([1]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::MOVE, [
            'playerId' => 1, 'x' => 6, 'y' => 7,
        ]);

        $this->assertTrue($result->isSuccess());
        $player = $result->getNewState()->getPlayer(1);
        $this->assertTrue($player->hasActed());
    }

    public function testReallyStupidPassWithAlly(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, movement: 6, id: 1, skills: [SkillName::ReallyStupid])
            ->addPlayer(TeamSide::HOME, 5, 6, id: 3) // adjacent ally
            ->addPlayer(TeamSide::AWAY, 20, 7, id: 2)
            ->build();

        // Roll 2 (>= 2, pass with ally)
        $dice = new FixedDiceRoller([2]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::MOVE, [
            'playerId' => 1, 'x' => 6, 'y' => 7,
        ]);

        $this->assertTrue($result->isSuccess());
        $this->assertEquals(6, $result->getNewState()->getPlayer(1)->getPosition()->getX());
    }

    // === Wild Animal ===

    public function testWildAnimalFailOnMove(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1, skills: [SkillName::WildAnimal])
            ->addPlayer(TeamSide::AWAY, 20, 7, id: 2)
            ->build();

        // Roll 2 (<= 2, fail)
        $dice = new FixedDiceRoller([2]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::MOVE, [
            'playerId' => 1, 'x' => 6, 'y' => 7,
        ]);

        $this->assertTrue($result->isSuccess());
        $player = $result->getNewState()->getPlayer(1);
        $this->assertTrue($player->hasActed());
        $this->assertFalse($player->hasLostTacklezones()); // Wild Animal keeps TZ
    }

    public function testWildAnimalPassOnMove(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, movement: 6, id: 1, skills: [SkillName::WildAnimal])
            ->addPlayer(TeamSide::AWAY, 20, 7, id: 2)
            ->build();

        // Roll 3 (>= 3, pass)
        $dice = new FixedDiceRoller([3]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::MOVE, [
            'playerId' => 1, 'x' => 6, 'y' => 7,
        ]);

        $this->assertTrue($result->isSuccess());
        $this->assertEquals(6, $result->getNewState()->getPlayer(1)->getPosition()->getX());
    }

    public function testWildAnimalAutoPassBlock(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, strength: 5, id: 1, skills: [SkillName::WildAnimal])
            ->addPlayer(TeamSide::AWAY, 6, 7, strength: 3, id: 2)
            ->build();

        // No wild animal check, 2-dice block (ST 5 vs 3): die1=6, die2=6 (Def Down), armor 3+3=6 < 8
        $dice = new FixedDiceRoller([6, 6, 3, 3]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::BLOCK, [
            'playerId' => 1, 'targetId' => 2,
        ]);

        $this->assertTrue($result->isSuccess());
        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertNotContains('wild_animal', $types);
        $this->assertContains('block', $types);
    }

    public function testWildAnimalAutoPassBlitz(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, strength: 5, id: 1, skills: [SkillName::WildAnimal])
            ->addPlayer(TeamSide::AWAY, 6, 7, strength: 3, id: 2)
            ->build();

        // No wild animal check, 2-dice block (ST 5 vs 3): die1=6, die2=6 (Def Down), armor 3+3=6 < 8
        $dice = new FixedDiceRoller([6, 6, 3, 3]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::BLITZ, [
            'playerId' => 1, 'targetId' => 2,
        ]);

        $this->assertTrue($result->isSuccess());
        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertNotContains('wild_animal', $types);
    }

    // === Loner ===

    public function testLonerBlocksTeamRerollDodge(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, movement: 6, agility: 3, id: 1, skills: [SkillName::Loner])
            ->addPlayer(TeamSide::AWAY, 5, 6, id: 2)
            ->build();

        // Dodge: roll 1 (fail), Loner check: 2 (< 4, reroll blocked), falls
        $dice = new FixedDiceRoller([1, 2]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::MOVE, [
            'playerId' => 1, 'x' => 5, 'y' => 8,
        ]);

        $this->assertTrue($result->isTurnover());
        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('loner', $types);
    }

    public function testLonerAllowsTeamReroll(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, movement: 6, agility: 3, id: 1, skills: [SkillName::Loner])
            ->addPlayer(TeamSide::AWAY, 5, 6, id: 2)
            ->build();

        // Dodge: roll 1 (fail), Loner: 4 (pass), team reroll: 6 (success)
        $dice = new FixedDiceRoller([1, 4, 6]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::MOVE, [
            'playerId' => 1, 'x' => 5, 'y' => 8,
        ]);

        $this->assertTrue($result->isSuccess());
    }

    public function testLonerDoesNotAffectSkillRerolls(): void
    {
        // Player with Loner + Dodge: Dodge skill reroll should work without Loner check
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, movement: 6, agility: 3, id: 1, skills: [SkillName::Loner, SkillName::Dodge])
            ->addPlayer(TeamSide::AWAY, 5, 6, id: 2)
            ->build();

        // Dodge: roll 1 (fail), Dodge skill reroll: 6 (success) — no Loner check
        $dice = new FixedDiceRoller([1, 6]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::MOVE, [
            'playerId' => 1, 'x' => 5, 'y' => 8,
        ]);

        $this->assertTrue($result->isSuccess());
        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertNotContains('loner', $types);
    }

    public function testLonerBlocksTeamRerollGfi(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, movement: 6, id: 1, skills: [SkillName::Loner])
            ->addPlayer(TeamSide::AWAY, 20, 7, id: 2)
            ->build();

        // Move to (12,7) = 7 squares, 1 GFI
        // GFI: roll 1 (fail), Loner: 3 (fail), falls
        $dice = new FixedDiceRoller([1, 3]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::MOVE, [
            'playerId' => 1, 'x' => 12, 'y' => 7,
        ]);

        $this->assertTrue($result->isTurnover());
    }

    public function testLonerBlocksTeamRerollPickup(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, movement: 6, agility: 3, id: 1, skills: [SkillName::Loner])
            ->addPlayer(TeamSide::AWAY, 20, 7, id: 2)
            ->withBallOnGround(6, 7)
            ->build();

        // Move to ball, pickup: roll 1 (fail), Loner: 2 (fail), bounce d8=1
        $dice = new FixedDiceRoller([1, 2, 1]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::MOVE, [
            'playerId' => 1, 'x' => 6, 'y' => 7,
        ]);

        $this->assertTrue($result->isTurnover());
    }

    // === Regeneration ===

    public function testRegenerationSuccess(): void
    {
        $player = \App\DTO\MatchPlayerDTO::create(
            id: 1, playerId: 1, name: 'Troll', number: 1,
            positionalName: 'Troll',
            stats: new \App\ValueObject\PlayerStats(4, 5, 1, 9),
            skills: [SkillName::Regeneration],
            teamSide: TeamSide::HOME,
            position: new \App\ValueObject\Position(5, 7),
        )->withState(PlayerState::PRONE);

        $injuryResolver = new InjuryResolver();
        // Armor: 6+5=11 > 9 (broken)
        // Injury: 6+5=11 (casualty)
        // Regen: 4 (>= 4, success)
        $dice = new FixedDiceRoller([6, 5, 6, 5, 4]);
        $result = $injuryResolver->resolve($player, $dice);

        $this->assertEquals(PlayerState::OFF_PITCH, $result['player']->getState());
        $types = array_map(fn($e) => $e->getType(), $result['events']);
        $this->assertContains('regeneration', $types);
    }

    public function testRegenerationFailure(): void
    {
        $player = \App\DTO\MatchPlayerDTO::create(
            id: 1, playerId: 1, name: 'Troll', number: 1,
            positionalName: 'Troll',
            stats: new \App\ValueObject\PlayerStats(4, 5, 1, 9),
            skills: [SkillName::Regeneration],
            teamSide: TeamSide::HOME,
            position: new \App\ValueObject\Position(5, 7),
        )->withState(PlayerState::PRONE);

        $injuryResolver = new InjuryResolver();
        // Armor: 6+5=11 > 9 (broken)
        // Injury: 6+5=11 (casualty)
        // Regen: 3 (< 4, fail)
        $dice = new FixedDiceRoller([6, 5, 6, 5, 3]);
        $result = $injuryResolver->resolve($player, $dice);

        $this->assertEquals(PlayerState::INJURED, $result['player']->getState());
    }

    public function testRegenerationNotOnStunned(): void
    {
        $player = \App\DTO\MatchPlayerDTO::create(
            id: 1, playerId: 1, name: 'Troll', number: 1,
            positionalName: 'Troll',
            stats: new \App\ValueObject\PlayerStats(4, 5, 1, 9),
            skills: [SkillName::Regeneration],
            teamSide: TeamSide::HOME,
            position: new \App\ValueObject\Position(5, 7),
        )->withState(PlayerState::PRONE);

        $injuryResolver = new InjuryResolver();
        // Armor: 6+4=10 > 9 (broken)
        // Injury: 3+3=6 (stunned, <= 7)
        // No regen roll
        $dice = new FixedDiceRoller([6, 4, 3, 3]);
        $result = $injuryResolver->resolve($player, $dice);

        $this->assertEquals(PlayerState::STUNNED, $result['player']->getState());
    }

    public function testRegenerationNotOnKO(): void
    {
        $player = \App\DTO\MatchPlayerDTO::create(
            id: 1, playerId: 1, name: 'Troll', number: 1,
            positionalName: 'Troll',
            stats: new \App\ValueObject\PlayerStats(4, 5, 1, 9),
            skills: [SkillName::Regeneration],
            teamSide: TeamSide::HOME,
            position: new \App\ValueObject\Position(5, 7),
        )->withState(PlayerState::PRONE);

        $injuryResolver = new InjuryResolver();
        // Armor: 6+4=10 > 9 (broken)
        // Injury: 5+4=9 (KO, 8-9)
        // No regen roll
        $dice = new FixedDiceRoller([6, 4, 5, 4]);
        $result = $injuryResolver->resolve($player, $dice);

        $this->assertEquals(PlayerState::KO, $result['player']->getState());
    }
}
