<?php
declare(strict_types=1);

namespace App\Tests\Engine;

use App\Engine\FixedDiceRoller;
use App\Engine\InjuryResolver;
use App\Enum\PlayerState;
use App\Enum\TeamSide;
use App\ValueObject\PlayerStats;
use App\ValueObject\Position;
use App\DTO\MatchPlayerDTO;
use PHPUnit\Framework\TestCase;

final class InjuryResolverTest extends TestCase
{
    private InjuryResolver $resolver;

    protected function setUp(): void
    {
        $this->resolver = new InjuryResolver();
    }

    private function makePlayer(int $armour = 8, int $id = 1): MatchPlayerDTO
    {
        return MatchPlayerDTO::create(
            id: $id,
            playerId: $id,
            name: "Player {$id}",
            number: $id,
            positionalName: 'Lineman',
            stats: new PlayerStats(6, 3, 3, $armour),
            skills: [],
            teamSide: TeamSide::HOME,
            position: new Position(5, 5),
        );
    }

    public function testArmourHolds(): void
    {
        $player = $this->makePlayer(armour: 8);
        // Roll 4+3=7 vs AV8, does not break (need >8)
        $dice = new FixedDiceRoller([4, 3]);

        $result = $this->resolver->resolve($player, $dice);

        $this->assertSame(PlayerState::STANDING, $result['player']->getState());
        $this->assertCount(1, $result['events']); // just armour roll
    }

    public function testArmourBrokenStunned(): void
    {
        $player = $this->makePlayer(armour: 7);
        // Armour: 4+4=8 > AV7, breaks
        // Injury: 3+3=6 <= 7, stunned
        $dice = new FixedDiceRoller([4, 4, 3, 3]);

        $result = $this->resolver->resolve($player, $dice);

        $this->assertSame(PlayerState::STUNNED, $result['player']->getState());
        $this->assertCount(2, $result['events']); // armour + injury
    }

    public function testArmourBrokenKO(): void
    {
        $player = $this->makePlayer(armour: 7);
        // Armour: 5+4=9 > AV7, breaks
        // Injury: 4+4=8, KO (8-9)
        $dice = new FixedDiceRoller([5, 4, 4, 4]);

        $result = $this->resolver->resolve($player, $dice);

        $this->assertSame(PlayerState::KO, $result['player']->getState());
        $this->assertNull($result['player']->getPosition()); // removed from pitch
    }

    public function testArmourBrokenCasualty(): void
    {
        $player = $this->makePlayer(armour: 7);
        // Armour: 5+4=9 > AV7, breaks
        // Injury: 5+5=10, casualty (10+)
        $dice = new FixedDiceRoller([5, 4, 5, 5]);

        $result = $this->resolver->resolve($player, $dice);

        $this->assertSame(PlayerState::INJURED, $result['player']->getState());
        $this->assertNull($result['player']->getPosition());
    }

    public function testArmourModifierBreaksArmour(): void
    {
        $player = $this->makePlayer(armour: 8);
        // Roll 4+4=8, with +1 modifier = 9 > AV8, breaks
        // Injury: 2+2=4, stunned
        $dice = new FixedDiceRoller([4, 4, 2, 2]);

        $result = $this->resolver->resolve($player, $dice, armourModifier: 1);

        $this->assertSame(PlayerState::STUNNED, $result['player']->getState());
    }

    public function testInjuryModifierUpgradesSeverity(): void
    {
        $player = $this->makePlayer(armour: 7);
        // Armour: 5+4=9 > AV7, breaks
        // Injury: 5+4=9, with +1 = 10, casualty instead of KO
        $dice = new FixedDiceRoller([5, 4, 5, 4]);

        $result = $this->resolver->resolve($player, $dice, injuryModifier: 1);

        $this->assertSame(PlayerState::INJURED, $result['player']->getState());
    }

    public function testCrowdSurfSkipsArmour(): void
    {
        $player = $this->makePlayer(armour: 10); // High AV doesn't matter
        // Injury: 3+3=6 +1(crowd) = 7, stunned
        $dice = new FixedDiceRoller([3, 3]);

        $result = $this->resolver->resolveCrowdSurf($player, $dice);

        // No armour roll event, just injury
        $this->assertSame(PlayerState::STUNNED, $result['player']->getState());
        $this->assertCount(1, $result['events']); // only injury roll
    }

    public function testCrowdSurfCanCauseKO(): void
    {
        $player = $this->makePlayer(armour: 10);
        // Injury: 4+3=7 +1(crowd) = 8, KO
        $dice = new FixedDiceRoller([4, 3]);

        $result = $this->resolver->resolveCrowdSurf($player, $dice);

        $this->assertSame(PlayerState::KO, $result['player']->getState());
    }

    public function testCrowdSurfCanCauseCasualty(): void
    {
        $player = $this->makePlayer(armour: 10);
        // Injury: 5+4=9 +1(crowd) = 10, casualty
        $dice = new FixedDiceRoller([5, 4]);

        $result = $this->resolver->resolveCrowdSurf($player, $dice);

        $this->assertSame(PlayerState::INJURED, $result['player']->getState());
    }
}
