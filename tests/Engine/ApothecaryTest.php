<?php

declare(strict_types=1);

namespace App\Tests\Engine;

use App\Engine\ActionResolver;
use App\Engine\FixedDiceRoller;
use App\Engine\InjuryResolver;
use App\DTO\TeamStateDTO;
use App\Enum\ActionType;
use App\Enum\PlayerState;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use App\ValueObject\PlayerStats;
use App\ValueObject\Position;
use PHPUnit\Framework\TestCase;

final class ApothecaryTest extends TestCase
{
    public function testApothecaryUsedOnCasualty(): void
    {
        // ST4 attacker blocks ST3 defender: 2 dice attacker chooses
        // Roll: 6 (Defender Down) - defender knocked down, armor broken, casualty
        // Apothecary re-rolls injury: gets stunned (better)
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, strength: 4, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 7, strength: 3, armour: 7, id: 2)
            ->build();

        // Block: 2 dice, rolls 6,6 = both Defender Down, choose first
        // Armor: 5+4=9 > 7 (broken)
        // Injury: 5+5=10 (casualty)
        // Apothecary re-roll: 3+3=6 (stunned, better!)
        $dice = new FixedDiceRoller([6, 6, 5, 4, 5, 5, 3, 3]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::BLOCK, [
            'playerId' => 1, 'targetId' => 2,
        ]);

        // Defender should be stunned (apothecary saved from casualty)
        $defender = $result->getNewState()->getPlayer(2);
        $this->assertEquals(PlayerState::STUNNED, $defender->getState());

        // Apothecary should be used
        $this->assertTrue($result->getNewState()->getAwayTeam()->isApothecaryUsed());

        // Should have apothecary event
        $apoEvents = array_filter(
            $result->getEvents(),
            fn($e) => $e->getType() === 'apothecary',
        );
        $this->assertNotEmpty($apoEvents);
    }

    public function testApothecaryKeepsOriginalIfRerollWorse(): void
    {
        // Casualty → apothecary re-roll → also casualty (no improvement)
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, strength: 4, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 7, strength: 3, armour: 7, id: 2)
            ->build();

        // Block: 6,6 (Defender Down)
        // Armor: 5+4=9 > 7 (broken)
        // Injury: 5+5=10 (casualty)
        // Apothecary re-roll: 6+5=11 (casualty again, worse or same)
        // Keep original (casualty)
        $dice = new FixedDiceRoller([6, 6, 5, 4, 5, 5, 6, 5]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::BLOCK, [
            'playerId' => 1, 'targetId' => 2,
        ]);

        $defender = $result->getNewState()->getPlayer(2);
        $this->assertEquals(PlayerState::INJURED, $defender->getState());
        $this->assertTrue($result->getNewState()->getAwayTeam()->isApothecaryUsed());
    }

    public function testApothecaryNotUsedOnKO(): void
    {
        // KO result: apothecary NOT used (only on casualty per plan)
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, strength: 4, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 7, strength: 3, armour: 7, id: 2)
            ->build();

        // Block: 6,6 (Defender Down)
        // Armor: 5+4=9 > 7 (broken)
        // Injury: 4+4=8 (KO)
        $dice = new FixedDiceRoller([6, 6, 5, 4, 4, 4]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::BLOCK, [
            'playerId' => 1, 'targetId' => 2,
        ]);

        $defender = $result->getNewState()->getPlayer(2);
        $this->assertEquals(PlayerState::KO, $defender->getState());
        // Apothecary NOT used
        $this->assertFalse($result->getNewState()->getAwayTeam()->isApothecaryUsed());
    }

    public function testApothecaryCanOnlyBeUsedOnce(): void
    {
        $awayTeam = TeamStateDTO::create(2, 'Away', 'Orc', TeamSide::AWAY, 2)
            ->withApothecaryUsed();

        $state = (new GameStateBuilder())
            ->withAwayTeam($awayTeam)
            ->addPlayer(TeamSide::HOME, 5, 7, strength: 4, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 7, strength: 3, armour: 7, id: 2)
            ->build();

        // Block: 6,6 (Defender Down)
        // Armor: 5+4=9 > 7 (broken)
        // Injury: 5+5=10 (casualty)
        // No apothecary (already used)
        $dice = new FixedDiceRoller([6, 6, 5, 4, 5, 5]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::BLOCK, [
            'playerId' => 1, 'targetId' => 2,
        ]);

        $defender = $result->getNewState()->getPlayer(2);
        $this->assertEquals(PlayerState::INJURED, $defender->getState());
    }

    public function testApothecaryRerollGetsKOStillBetterThanCasualty(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, strength: 4, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 7, strength: 3, armour: 7, id: 2)
            ->build();

        // Block: 6,6 (Defender Down)
        // Armor: 5+4=9 > 7 (broken)
        // Injury: 5+5=10 (casualty)
        // Apothecary re-roll: 4+4=8 (KO, better than casualty)
        $dice = new FixedDiceRoller([6, 6, 5, 4, 5, 5, 4, 4]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::BLOCK, [
            'playerId' => 1, 'targetId' => 2,
        ]);

        $defender = $result->getNewState()->getPlayer(2);
        $this->assertEquals(PlayerState::KO, $defender->getState());
        $this->assertTrue($result->getNewState()->getAwayTeam()->isApothecaryUsed());
    }

    public function testTeamStateDTOApothecaryDefaults(): void
    {
        $team = TeamStateDTO::create(1, 'Test', 'Human', TeamSide::HOME, 3);
        $this->assertTrue($team->hasApothecary());
        $this->assertFalse($team->isApothecaryUsed());
        $this->assertTrue($team->canUseApothecary());
    }

    public function testTeamStateDTOApothecaryUsed(): void
    {
        $team = TeamStateDTO::create(1, 'Test', 'Human', TeamSide::HOME, 3)
            ->withApothecaryUsed();
        $this->assertTrue($team->hasApothecary());
        $this->assertTrue($team->isApothecaryUsed());
        $this->assertFalse($team->canUseApothecary());
    }

    public function testTeamStateDTOSerialization(): void
    {
        $team = TeamStateDTO::create(1, 'Test', 'Human', TeamSide::HOME, 3)
            ->withApothecaryUsed();

        $arr = $team->toArray();
        $this->assertTrue($arr['hasApothecary']);
        $this->assertTrue($arr['apothecaryUsed']);

        $restored = TeamStateDTO::fromArray($arr);
        $this->assertTrue($restored->hasApothecary());
        $this->assertTrue($restored->isApothecaryUsed());
    }
}
