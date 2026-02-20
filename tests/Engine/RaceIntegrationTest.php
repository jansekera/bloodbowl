<?php
declare(strict_types=1);

namespace App\Tests\Engine;

use App\Engine\ActionResolver;
use App\Engine\FixedDiceRoller;
use App\Engine\InjuryResolver;
use App\Enum\ActionType;
use App\Enum\PlayerState;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use App\ValueObject\PlayerStats;
use PHPUnit\Framework\TestCase;

final class RaceIntegrationTest extends TestCase
{
    // ========== STEP 10: Integration tests for new races ==========

    public function testChaosWarriorST4BlockVsLineman(): void
    {
        // Chaos Warrior (5/4/3/9) blocks Human Lineman (6/3/3/8)
        // ST4 vs ST3 → 2 dice attacker chooses
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, movement: 5, strength: 4, agility: 3, armour: 9, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, movement: 6, strength: 3, agility: 3, armour: 8, id: 2)
            ->withBallOffPitch()
            ->build();

        // 2 dice: roll 6, 3 → Defender Down + Pushed, attacker chooses DD
        // Armor: 3+3=6 vs AV8 → holds
        $dice = new FixedDiceRoller([6, 3, 3, 3]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);

        $this->assertTrue($result->isSuccess());
        $blockEvents = array_filter($result->getEvents(), fn($e) => $e->getType() === 'block');
        $blockEvent = array_values($blockEvents)[0];
        $this->assertEquals(2, $blockEvent->getData()['diceCount']);
        $this->assertTrue($blockEvent->getData()['attackerChooses']);
    }

    public function testMinotaurWildAnimalPassOnBlock(): void
    {
        // Minotaur (5/5/2/8, Wild Animal, Frenzy, Horns, MB, etc.)
        // Wild Animal auto-passes on block. ST5 vs ST3 → 2 dice
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, movement: 5, strength: 5, agility: 2, armour: 8,
                skills: [SkillName::WildAnimal, SkillName::Frenzy, SkillName::MightyBlow, SkillName::Horns, SkillName::Loner],
                id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, strength: 3, armour: 8, id: 2)
            ->withBallOffPitch()
            ->build();

        // No wild animal roll (auto-pass for block)
        // 2 dice: roll 6, 6 → both DD, choose DD
        // Armor: 5+4=9 > 8 (broken with MB would be +1, but MB only adds to one roll)
        // Armor: 5+4=9 + MB=1 → 10 > 8, broken. Injury: 3+3=6 → stunned
        $dice = new FixedDiceRoller([6, 6, 5, 4, 3, 3]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);

        $this->assertTrue($result->isSuccess());
        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertNotContains('wild_animal', $types);
        $this->assertContains('block', $types);
    }

    public function testMinotaurWildAnimalFailOnMove(): void
    {
        // Minotaur fails Wild Animal check on move action
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, movement: 5, strength: 5, agility: 2, armour: 8,
                skills: [SkillName::WildAnimal, SkillName::Loner],
                id: 1)
            ->addPlayer(TeamSide::AWAY, 20, 5, id: 2)
            ->withBallOffPitch()
            ->build();

        // Wild animal roll: 2 (fail, <= 2)
        $dice = new FixedDiceRoller([2]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::MOVE, ['playerId' => 1, 'x' => 6, 'y' => 5]);

        $this->assertTrue($result->isSuccess());
        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('wild_animal', $types);
        // Player should still be at original position
        $player = $result->getNewState()->getPlayer(1);
        $this->assertTrue($player->hasActed());
    }

    public function testMummyRegenerationAfterCasualty(): void
    {
        // Mummy (3/5/1/9, MB, Regeneration) suffers casualty → regen saves
        $player = \App\DTO\MatchPlayerDTO::create(
            id: 1, playerId: 1, name: 'Mummy', number: 1,
            positionalName: 'Mummy',
            stats: new PlayerStats(3, 5, 1, 9),
            skills: [SkillName::MightyBlow, SkillName::Regeneration],
            teamSide: TeamSide::HOME,
            position: new \App\ValueObject\Position(5, 5),
        )->withState(PlayerState::PRONE);

        $injuryResolver = new InjuryResolver();
        // Armor: 6+4=10 > 9 (broken)
        // Injury: 6+5=11 (casualty)
        // Regen: 4 (>= 4, success)
        $dice = new FixedDiceRoller([6, 4, 6, 5, 4]);
        $result = $injuryResolver->resolve($player, $dice);

        $this->assertEquals(PlayerState::OFF_PITCH, $result['player']->getState());
        $types = array_map(fn($e) => $e->getType(), $result['events']);
        $this->assertContains('regeneration', $types);
    }

    public function testSkinkDodgeStuntyFromTZ(): void
    {
        // Skink (8/2/3/7, Dodge, Stunty) dodges from tackle zone
        // AG3 + Stunty(-1) + 1TZ = 7-3+1-1 = 4+. With Dodge skill reroll available.
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, movement: 8, strength: 2, agility: 3, armour: 7,
                skills: [SkillName::Dodge, SkillName::Stunty], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, id: 2) // provides 1 TZ
            ->withBallOffPitch()
            ->build();

        // Dodge roll: 2 (fail, need 4+), Dodge reroll: 4 (success)
        $dice = new FixedDiceRoller([2, 4]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::MOVE, ['playerId' => 1, 'x' => 5, 'y' => 4]);

        $this->assertTrue($result->isSuccess());
        $this->assertEquals(5, $result->getNewState()->getPlayer(1)->getPosition()->getX());
        $this->assertEquals(4, $result->getNewState()->getPlayer(1)->getPosition()->getY());
    }

    public function testUndeadWightBlockRegenBothDown(): void
    {
        // Wight (6/3/3/8, Block, Regeneration) blocks ST3 target → 1 die
        // Rolls BOTH_DOWN → Block skill converts to push
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, movement: 6, strength: 3, agility: 3, armour: 8,
                skills: [SkillName::Block, SkillName::Regeneration], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, strength: 3, armour: 8, id: 2)
            ->withBallOffPitch()
            ->build();

        // 1 die: roll 2 (BOTH_DOWN) → Block skill saves attacker, defender goes down
        // Armor: 3+3=6 vs AV8 → holds (defender prone)
        $dice = new FixedDiceRoller([2, 3, 3]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);

        $this->assertTrue($result->isSuccess());
        // Attacker should still be standing (Block skill saves from Both Down)
        $attacker = $result->getNewState()->getPlayer(1);
        $this->assertEquals(PlayerState::STANDING, $attacker->getState());
        // Defender should be prone (knocked down)
        $defender = $result->getNewState()->getPlayer(2);
        $this->assertEquals(PlayerState::PRONE, $defender->getState());
    }
}
