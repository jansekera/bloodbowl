<?php
declare(strict_types=1);

namespace App\Tests\Engine;

use App\Engine\FixedDiceRoller;
use App\Engine\InjuryResolver;
use App\Enum\PlayerState;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use App\ValueObject\PlayerStats;
use App\ValueObject\Position;
use App\DTO\MatchPlayerDTO;
use PHPUnit\Framework\TestCase;

final class StakesTest extends TestCase
{
    public function testStakesBlocksRegeneration(): void
    {
        $player = MatchPlayerDTO::create(
            id: 1, playerId: 1, name: 'Wight', number: 1,
            positionalName: 'Wight',
            stats: new PlayerStats(6, 3, 3, 8),
            skills: [SkillName::Regeneration],
            teamSide: TeamSide::AWAY,
            position: new Position(5, 5),
        )->withState(PlayerState::PRONE);

        $injuryResolver = new InjuryResolver();
        // Armor: 5+4=9 > 8 (broken)
        // Injury: 6+5=11 (casualty)
        // No regen roll because Stakes blocks it
        $dice = new FixedDiceRoller([5, 4, 6, 5]);
        $result = $injuryResolver->resolve($player, $dice, hasStakes: true);

        $this->assertSame(PlayerState::INJURED, $result['player']->getState());
        $types = array_map(fn($e) => $e->getType(), $result['events']);
        $this->assertContains('stakes_block_regen', $types);
        $this->assertNotContains('regeneration', $types);
    }

    public function testWithoutStakesRegenerationWorks(): void
    {
        $player = MatchPlayerDTO::create(
            id: 1, playerId: 1, name: 'Wight', number: 1,
            positionalName: 'Wight',
            stats: new PlayerStats(6, 3, 3, 8),
            skills: [SkillName::Regeneration],
            teamSide: TeamSide::AWAY,
            position: new Position(5, 5),
        )->withState(PlayerState::PRONE);

        $injuryResolver = new InjuryResolver();
        // Armor: 5+4=9 > 8 (broken)
        // Injury: 6+5=11 (casualty)
        // Regen: 4 (>= 4, success)
        $dice = new FixedDiceRoller([5, 4, 6, 5, 4]);
        $result = $injuryResolver->resolve($player, $dice, hasStakes: false);

        $this->assertSame(PlayerState::OFF_PITCH, $result['player']->getState());
        $types = array_map(fn($e) => $e->getType(), $result['events']);
        $this->assertContains('regeneration', $types);
        $this->assertNotContains('stakes_block_regen', $types);
    }

    public function testStakesNoEffectWithoutRegeneration(): void
    {
        $player = MatchPlayerDTO::create(
            id: 1, playerId: 1, name: 'Lineman', number: 1,
            positionalName: 'Lineman',
            stats: new PlayerStats(6, 3, 3, 8),
            skills: [],
            teamSide: TeamSide::AWAY,
            position: new Position(5, 5),
        )->withState(PlayerState::PRONE);

        $injuryResolver = new InjuryResolver();
        // Armor: 5+4=9 > 8 (broken)
        // Injury: 6+5=11 (casualty)
        // No regen skill, stakes irrelevant
        $dice = new FixedDiceRoller([5, 4, 6, 5]);
        $result = $injuryResolver->resolve($player, $dice, hasStakes: true);

        $this->assertSame(PlayerState::INJURED, $result['player']->getState());
        $types = array_map(fn($e) => $e->getType(), $result['events']);
        $this->assertNotContains('stakes_block_regen', $types);
        $this->assertNotContains('regeneration', $types);
    }

    public function testStakesInBlockContext(): void
    {
        // Full block: attacker with Stakes knocks down defender with Regeneration
        // ST4 vs ST3 = 2 dice, attacker chooses
        // Disable apothecary on away team to simplify dice sequence
        $awayTeam = \App\DTO\TeamStateDTO::create(2, 'Away', 'Orc', TeamSide::AWAY, 2, hasApothecary: false);
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 4, skills: [SkillName::Stakes], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, skills: [SkillName::Regeneration], armour: 7, id: 2)
            ->withAwayTeam($awayTeam)
            ->withBallOffPitch()
            ->build();

        // 2 block dice D6: [6,6] → both POW, choose POW → defender pushed + down
        // Pushback: no dice needed (auto picks empty square)
        // Armor roll 2D6: [5,4]=9 > 7 → broken
        // Injury roll 2D6: [6,5]=11 → casualty
        // Stakes blocks regen → no regen roll needed
        $dice = new FixedDiceRoller([6, 6, 5, 4, 6, 5]);
        $resolver = new \App\Engine\ActionResolver($dice);
        $result = $resolver->resolve($state, \App\Enum\ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);

        $defender = $result->getNewState()->getPlayer(2);
        $this->assertSame(PlayerState::INJURED, $defender->getState());
        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('stakes_block_regen', $types);
        $this->assertNotContains('regeneration', $types);
    }
}
