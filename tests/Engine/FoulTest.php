<?php

declare(strict_types=1);

namespace App\Tests\Engine;

use App\Engine\ActionResolver;
use App\Engine\FixedDiceRoller;
use App\Engine\RulesEngine;
use App\Enum\ActionType;
use App\Enum\PlayerState;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use PHPUnit\Framework\TestCase;

final class FoulTest extends TestCase
{
    public function testFoulArmorBrokenCausesInjury(): void
    {
        // Attacker at (5,7), prone defender at (6,7) with AV=8
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1)
            ->addPronePlayer(TeamSide::AWAY, 6, 7, armour: 8, id: 2)
            ->build();

        // Armor: die1=5 + die2=4 + 1 = 10 > 8 → broken
        // Injury: 3+4 = 7 → stunned
        $dice = new FixedDiceRoller([5, 4, 3, 4]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::FOUL, [
            'playerId' => 1, 'targetId' => 2,
        ]);

        $this->assertTrue($result->isSuccess());
        $this->assertFalse($result->isTurnover());

        // Check events
        $events = $result->getEvents();
        $foulEvent = $events[0];
        $this->assertEquals('foul', $foulEvent->getType());
        $this->assertTrue($foulEvent->getData()['armourBroken']);

        // Defender should be stunned from injury
        $defender = $result->getNewState()->getPlayer(2);
        $this->assertNotNull($defender);
        $this->assertEquals(PlayerState::STUNNED, $defender->getState());
    }

    public function testFoulArmorHolds(): void
    {
        // Armor: die1=2 + die2=3 + 1 = 6, AV=8 → not broken (6 <= 8)
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1)
            ->addPronePlayer(TeamSide::AWAY, 6, 7, armour: 8, id: 2)
            ->build();

        $dice = new FixedDiceRoller([2, 3]); // total=6, not > 8
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::FOUL, [
            'playerId' => 1, 'targetId' => 2,
        ]);

        $this->assertTrue($result->isSuccess());

        $foulEvent = $result->getEvents()[0];
        $this->assertFalse($foulEvent->getData()['armourBroken']);

        // Defender stays prone (no injury)
        $defender = $result->getNewState()->getPlayer(2);
        $this->assertNotNull($defender);
        $this->assertEquals(PlayerState::PRONE, $defender->getState());
    }

    public function testFoulDoublesCausesEjection(): void
    {
        // ⛔ UPRAVENO 16.09.2026: zadny "prone bonus" (r. 1843-1850) a vylouceni
        //   JE turnover (r. 1879-1881).
        // Armor: 3+3 = 6, AV=8 → neprolomeno. Ale dublet!
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1)
            ->addPronePlayer(TeamSide::AWAY, 6, 7, armour: 8, id: 2)
            ->build();

        $dice = new FixedDiceRoller([3, 3]); // doubles, armor holds
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::FOUL, [
            'playerId' => 1, 'targetId' => 2,
        ]);

        $this->assertTrue($result->isTurnover());

        // Attacker should be ejected
        $attacker = $result->getNewState()->getPlayer(1);
        $this->assertNotNull($attacker);
        $this->assertEquals(PlayerState::EJECTED, $attacker->getState());
        $this->assertNull($attacker->getPosition());

        // Check ejection event
        $ejectionEvents = array_filter(
            $result->getEvents(),
            fn($e) => $e->getType() === 'ejection',
        );
        $this->assertCount(1, $ejectionEvents);
    }

    public function testFoulDoublesWithArmorBreak(): void
    {
        // Armor: 5+5 = 10 > 8 → broken, AND doubles → ejection (a turnover)
        // Injury: 4+4 = 8 → KO
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1)
            ->addPronePlayer(TeamSide::AWAY, 6, 7, armour: 8, id: 2)
            ->build();

        $dice = new FixedDiceRoller([5, 5, 4, 4]); // armor break + doubles + injury KO
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::FOUL, [
            'playerId' => 1, 'targetId' => 2,
        ]);

        $this->assertTrue($result->isTurnover());

        // Defender KO'd
        $defender = $result->getNewState()->getPlayer(2);
        $this->assertNotNull($defender);
        $this->assertEquals(PlayerState::KO, $defender->getState());

        // Attacker ejected
        $attacker = $result->getNewState()->getPlayer(1);
        $this->assertNotNull($attacker);
        $this->assertEquals(PlayerState::EJECTED, $attacker->getState());
    }

    public function testDubletNaZRANENIvylouciFaulujiciho(): void
    {
        // ⭐ 21.09.2026: `rules_bb2016.txt` r. 1877-1878: "if the Armour
        //   **and/or Injury** roll is a doubles (i.e., two 1s, or two 2s, etc),
        //   the referee has spotted the foul, and the player taking the Foul
        //   Action is sent off". Do ted se dublet cetl JEN na brneni --
        //   `resolveInjury` vracel pouhy soucet 2D6 a jednotlive kostky zahodil.
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1)
            ->addPronePlayer(TeamSide::AWAY, 6, 7, armour: 8, id: 2)
            ->build();

        // Brneni 6+4 = 10 > 8, NENI dublet. Zraneni 3+3 = 6 = DUBLET (a stunned).
        $dice = new FixedDiceRoller([6, 4, 3, 3]);
        $result = (new ActionResolver($dice))->resolve($state, ActionType::FOUL, [
            'playerId' => 1, 'targetId' => 2,
        ]);

        $attacker = $result->getNewState()->getPlayer(1);
        $this->assertNotNull($attacker);
        $this->assertSame(PlayerState::EJECTED, $attacker->getState(), 'r. 1877-1878: dublet na zraneni take vylucuje');
        $this->assertTrue($result->isTurnover(), 'vylouceni faulujiciho je turnover (r. 1879-1881)');
    }

    public function testBezDubletuNaBrneniANiNaZraneniSeNevylucuje(): void
    {
        // Pozitivni kontrola k testu vyse: tataz fixtura, jen zraneni 3+4 misto 3+3.
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1)
            ->addPronePlayer(TeamSide::AWAY, 6, 7, armour: 8, id: 2)
            ->build();

        $dice = new FixedDiceRoller([6, 4, 3, 4]);
        $result = (new ActionResolver($dice))->resolve($state, ActionType::FOUL, [
            'playerId' => 1, 'targetId' => 2,
        ]);

        $attacker = $result->getNewState()->getPlayer(1);
        $this->assertNotNull($attacker);
        $this->assertNotSame(PlayerState::EJECTED, $attacker->getState(), 'bez dubletu se nevylucuje');
        $this->assertFalse($result->isTurnover());
    }

    public function testDubletNaZraneniSeSneakyGitemNevylucuje(): void
    {
        // r. 8523-8525: Sneaky Git -- "is never sent off for committing a foul
        //   unless he rolls doubles for the Armour roll" ... u nas plati, ze
        //   Sneaky Git vylouceni odvraci; test hlida, ze se to nezmenilo.
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, skills: [SkillName::SneakyGit], id: 1)
            ->addPronePlayer(TeamSide::AWAY, 6, 7, armour: 8, id: 2)
            ->build();

        $dice = new FixedDiceRoller([6, 4, 3, 3]);
        $result = (new ActionResolver($dice))->resolve($state, ActionType::FOUL, [
            'playerId' => 1, 'targetId' => 2,
        ]);

        $attacker = $result->getNewState()->getPlayer(1);
        $this->assertNotNull($attacker);
        $this->assertNotSame(PlayerState::EJECTED, $attacker->getState());
    }

    // ⛔ PREPSANO 16.09.2026: faul neni turnover JEN kdyz nepadne dublet.
    //   Pri dubletu je (r. 1879-1881) -- to hlida `FoulRulesTest`.
    public function testFoulBezDubletuNeniTurnover(): void
    {
        // Even with ejection, foul is NOT a turnover
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1)
            ->addPronePlayer(TeamSide::AWAY, 6, 7, id: 2)
            ->build();

        $dice = new FixedDiceRoller([2, 3]); // bez dubletu, zbroj drzi (5 <= 8)
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::FOUL, [
            'playerId' => 1, 'targetId' => 2,
        ]);

        $this->assertTrue($result->isSuccess());
        $this->assertFalse($result->isTurnover());
    }

    public function testFoulOncePerTurn(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1)
            ->addPlayer(TeamSide::HOME, 5, 5, id: 3)
            ->addPronePlayer(TeamSide::AWAY, 6, 7, id: 2)
            ->addPronePlayer(TeamSide::AWAY, 6, 5, id: 4)
            ->build();

        // First foul succeeds (armor holds, no doubles)
        $dice = new FixedDiceRoller([2, 3]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::FOUL, [
            'playerId' => 1, 'targetId' => 2,
        ]);

        $this->assertTrue($result->isSuccess());

        // Second foul should fail validation
        $rules = new RulesEngine();
        $errors = $rules->validate($result->getNewState(), ActionType::FOUL, [
            'playerId' => 3, 'targetId' => 4,
        ]);

        $this->assertContains('Foul already used this turn', $errors);
    }

    public function testFoulTargetMustBeProne(): void
    {
        // Target is standing → invalid
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 7, id: 2) // standing
            ->build();

        $rules = new RulesEngine();
        $errors = $rules->validate($state, ActionType::FOUL, [
            'playerId' => 1, 'targetId' => 2,
        ]);

        $this->assertContains('Can only foul prone or stunned players', $errors);
    }

    public function testFoulTargetMustBeAdjacent(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1)
            ->addPronePlayer(TeamSide::AWAY, 8, 7, id: 2) // not adjacent
            ->build();

        $rules = new RulesEngine();
        $errors = $rules->validate($state, ActionType::FOUL, [
            'playerId' => 1, 'targetId' => 2,
        ]);

        $this->assertContains('Target must be adjacent to foul', $errors);
    }

    public function testFoulOnStunnedPlayer(): void
    {
        // Stunned players can also be fouled
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 7, id: 2)
            ->build();

        // Make defender stunned
        $defender = $state->getPlayer(2);
        $this->assertNotNull($defender);
        $state = $state->withPlayer($defender->withState(PlayerState::STUNNED));

        // Armor: 5+4 = 9 > 8 → broken. Injury: 3+4 = 7 → stunned.
        // ⛔ 21.09.2026: puvodne tu bylo zraneni 3+3, tedy DUBLET -- od opravy
        //   r. 1877-1878 to znamena vylouceni a turnover, coz s predmetem
        //   tohohle testu ("i omraceneho jde faulovat") nema nic spolecneho.
        $dice = new FixedDiceRoller([5, 4, 3, 4]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::FOUL, [
            'playerId' => 1, 'targetId' => 2,
        ]);

        $this->assertTrue($result->isSuccess());
    }

    public function testGetFoulTargets(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1)
            ->addPronePlayer(TeamSide::AWAY, 6, 7, id: 2) // adjacent prone
            ->addPlayer(TeamSide::AWAY, 6, 8, id: 3) // adjacent standing (not a target)
            ->addPronePlayer(TeamSide::AWAY, 10, 7, id: 4) // non-adjacent prone
            ->build();

        $rules = new RulesEngine();
        $player = $state->getPlayer(1);
        $this->assertNotNull($player);
        $targets = $rules->getFoulTargets($state, $player);

        $this->assertCount(1, $targets);
        $this->assertEquals(2, $targets[0]->getId());
    }

    public function testFoulAppearsInAvailableActions(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1)
            ->addPronePlayer(TeamSide::AWAY, 6, 7, id: 2)
            ->build();

        $rules = new RulesEngine();
        $actions = $rules->getAvailableActions($state);

        $foulActions = array_filter($actions, fn($a) => $a['type'] === 'foul');
        $this->assertNotEmpty($foulActions, 'Foul should be available when adjacent prone enemy exists');
    }

    public function testFoulNotAvailableWithoutProneEnemy(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 7, id: 2) // standing, not prone
            ->build();

        $rules = new RulesEngine();
        $actions = $rules->getAvailableActions($state);

        $foulActions = array_filter($actions, fn($a) => $a['type'] === 'foul');
        $this->assertEmpty($foulActions, 'Foul should not be available without adjacent prone/stunned enemy');
    }

    public function testFoulBallCarrierEjected(): void
    {
        // Fouler is carrying the ball and gets ejected → ball bounces
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1)
            ->addPronePlayer(TeamSide::AWAY, 6, 7, id: 2)
            ->withBallCarried(1)
            ->build();

        // Armor: 2+2 = 4 <= 8 → drzi. Dublet → vylouceni (a turnover).
        // Ball bounce: D8=1 (north)
        $dice = new FixedDiceRoller([2, 2, 1]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::FOUL, [
            'playerId' => 1, 'targetId' => 2,
        ]);

        $this->assertTrue($result->isTurnover());

        // Attacker ejected
        $attacker = $result->getNewState()->getPlayer(1);
        $this->assertNotNull($attacker);
        $this->assertEquals(PlayerState::EJECTED, $attacker->getState());

        // Ball should not be held by ejected player
        $ball = $result->getNewState()->getBall();
        $this->assertNotEquals(1, $ball->getCarrierId());
    }

    public function testFoulMarksAttackerAsActed(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1)
            ->addPronePlayer(TeamSide::AWAY, 6, 7, id: 2)
            ->build();

        $dice = new FixedDiceRoller([2, 3]); // armor holds, no doubles
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::FOUL, [
            'playerId' => 1, 'targetId' => 2,
        ]);

        $attacker = $result->getNewState()->getPlayer(1);
        $this->assertNotNull($attacker);
        $this->assertTrue($attacker->hasActed());
        $this->assertTrue($attacker->hasMoved());
    }
}
