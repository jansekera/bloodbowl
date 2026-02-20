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

final class NurglesRotTest extends TestCase
{
    public function testNurglesRotEventOnCasualty(): void
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
        $dice = new FixedDiceRoller([5, 4, 6, 5]);
        $result = $injuryResolver->resolve($player, $dice, hasNurglesRot: true);

        $this->assertSame(PlayerState::INJURED, $result['player']->getState());
        $types = array_map(fn($e) => $e->getType(), $result['events']);
        $this->assertContains('nurgles_rot', $types);
    }

    public function testNoNurglesRotEventWithoutSkill(): void
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
        $dice = new FixedDiceRoller([5, 4, 6, 5]);
        $result = $injuryResolver->resolve($player, $dice, hasNurglesRot: false);

        $this->assertSame(PlayerState::INJURED, $result['player']->getState());
        $types = array_map(fn($e) => $e->getType(), $result['events']);
        $this->assertNotContains('nurgles_rot', $types);
    }

    public function testNurglesRotNotOnNonCasualty(): void
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
        // Injury: 3+3=6 (stunned)
        $dice = new FixedDiceRoller([5, 4, 3, 3]);
        $result = $injuryResolver->resolve($player, $dice, hasNurglesRot: true);

        $this->assertSame(PlayerState::STUNNED, $result['player']->getState());
        $types = array_map(fn($e) => $e->getType(), $result['events']);
        $this->assertNotContains('nurgles_rot', $types);
    }
}
