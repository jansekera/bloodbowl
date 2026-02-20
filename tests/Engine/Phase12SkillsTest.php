<?php
declare(strict_types=1);

namespace App\Tests\Engine;

use App\DTO\TeamStateDTO;
use App\Engine\ActionResolver;
use App\Engine\BallResolver;
use App\Engine\FixedDiceRoller;
use App\Engine\GameFlowResolver;
use App\Engine\KickoffResolver;
use App\Engine\ScatterCalculator;
use App\Engine\TacklezoneCalculator;
use App\Enum\ActionType;
use App\Enum\GamePhase;
use App\Enum\PlayerState;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use App\ValueObject\Position;
use PHPUnit\Framework\TestCase;

final class Phase12SkillsTest extends TestCase
{
    // ========== STEP 3: Sneaky Git, Fend, Piling On ==========

    public function testSneakyGitAvoidsEjectionOnDoubles(): void
    {
        // Attacker with SneakyGit fouls, doubles → no ejection
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, skills: [SkillName::SneakyGit], id: 1)
            ->addPronePlayer(TeamSide::AWAY, 6, 5, armour: 8, id: 2)
            ->withBallOffPitch()
            ->build();

        // Die1=3, Die2=3 (doubles), total=3+3+1=7 vs AV8 → not broken
        $dice = new FixedDiceRoller([3, 3]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::FOUL, ['playerId' => 1, 'targetId' => 2]);

        $this->assertTrue($result->isSuccess());
        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        // No ejection despite doubles
        $this->assertNotContains('ejection', $types);
        // Attacker still on pitch
        $this->assertNotNull($result->getNewState()->getPlayer(1)->getPosition());
    }

    public function testFendPreventsFollowUp(): void
    {
        // Defender with Fend, push result → attacker stays on original square
        // Equal ST (3v3) → 1 die, roll 3 = PUSHED
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, skills: [SkillName::Fend], id: 2)
            ->withBallOffPitch()
            ->build();

        $dice = new FixedDiceRoller([3]); // PUSHED
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);

        $this->assertTrue($result->isSuccess());
        // Attacker should still be at (5,5) — no follow-up
        $attPos = $result->getNewState()->getPlayer(1)->getPosition();
        $this->assertEquals(5, $attPos->getX());
        $this->assertEquals(5, $attPos->getY());
        // Fend event
        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('fend', $types);
    }

    public function testFendNoEffectOnKnockdown(): void
    {
        // Defender with Fend knocked down → follow-up still happens (Fend only works when standing)
        // Equal ST → 1 die, roll 6 = DEFENDER_DOWN
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, skills: [SkillName::Fend], armour: 10, id: 2)
            ->withBallOffPitch()
            ->build();

        // DD roll, armor 2+3=5 vs AV10 → not broken
        $dice = new FixedDiceRoller([6, 2, 3]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);

        $this->assertTrue($result->isSuccess());
        // Attacker should be at (6,5) — follow-up happened
        $attPos = $result->getNewState()->getPlayer(1)->getPosition();
        $this->assertEquals(6, $attPos->getX());
        // No fend event (Fend doesn't trigger on knockdown)
        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertNotContains('fend', $types);
    }

    public function testPilingOnRerollsArmor(): void
    {
        // Attacker with Piling On, defender knocked down, armor holds → PO reroll
        // ST4 vs ST3 → 2 dice attacker chooses, roll DD (6,6) → DEFENDER_DOWN
        // Armor: 2+2=4 vs AV8 → holds, PO reroll: 5+4=9 vs AV8 → broken, injury: 3+3=6 = stunned
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 4, skills: [SkillName::PilingOn], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, armour: 8, id: 2)
            ->withBallOffPitch()
            ->build();

        $dice = new FixedDiceRoller([
            6, 6,   // 2 block dice: DD, DD → choose DEFENDER_DOWN
            2, 2,   // armor roll: 4 vs AV8 → holds
            5, 4,   // PO armor reroll: 9 vs AV8 → broken
            3, 3,   // injury roll: 6 = stunned
        ]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);

        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('piling_on', $types);
        // Attacker should be prone from Piling On
        $this->assertSame(PlayerState::PRONE, $result->getNewState()->getPlayer(1)->getState());
        // Defender stunned from PO reroll
        $this->assertSame(PlayerState::STUNNED, $result->getNewState()->getPlayer(2)->getState());
    }

    public function testPilingOnAttackerGoesProne(): void
    {
        // Same as above, verify attacker is PRONE after Piling On
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 4, skills: [SkillName::PilingOn], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, armour: 10, id: 2) // AV10 so PO also fails
            ->withBallOffPitch()
            ->build();

        $dice = new FixedDiceRoller([
            6, 6,   // 2 block dice: DD
            2, 2,   // armor: 4 vs AV10 → holds
            3, 3,   // PO reroll: 6 vs AV10 → holds
        ]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);

        // Attacker is prone from Piling On even though armor held
        $this->assertSame(PlayerState::PRONE, $result->getNewState()->getPlayer(1)->getState());
    }

    public function testDirtyPlayerBonusFoulArmor(): void
    {
        // Dirty Player: +1 to foul armor roll
        // Die1=4, Die2=2, total = 4+2+1(prone)+1(DP) = 8 vs AV7 → broken
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, skills: [SkillName::DirtyPlayer], id: 1)
            ->addPronePlayer(TeamSide::AWAY, 6, 5, armour: 7, id: 2)
            ->withBallOffPitch()
            ->build();

        // Die1=4, Die2=2 (not doubles), injury: 3+4=7 = stunned
        $dice = new FixedDiceRoller([4, 2, 3, 4]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::FOUL, ['playerId' => 1, 'targetId' => 2]);

        $this->assertTrue($result->isSuccess());
        // Defender should be injured (armor broken from DP bonus)
        $defState = $result->getNewState()->getPlayer(2)->getState();
        $this->assertContains($defState, [PlayerState::STUNNED, PlayerState::KO, PlayerState::INJURED]);
    }

    // ========== STEP 4: Kick, Kick-Off Return, Leader ==========

    public function testKickHalvesScatterDistance(): void
    {
        // Kicking team (AWAY) has a player with Kick skill
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::AWAY, 15, 7, skills: [SkillName::Kick], id: 1)
            ->addPlayer(TeamSide::HOME, 5, 7, id: 2)
            ->withPhase(GamePhase::SETUP)
            ->withBallOffPitch()
            ->build();

        // D8=1, D6=6 → Kick: ceil(6/2)=3
        // Kickoff table: 4+4=8 (Changing Weather), weather roll: 3+4=7 (Nice)
        // Ball lands on pitch, no player → bounce: D8=1
        $dice = new FixedDiceRoller([1, 6, 4, 4, 3, 4, 1]);
        $tzCalc = new TacklezoneCalculator();
        $scatterCalc = new ScatterCalculator();
        $ballResolver = new BallResolver($dice, $tzCalc, $scatterCalc);
        $kickoffResolver = new KickoffResolver($dice, $scatterCalc, $ballResolver);

        $result = $kickoffResolver->resolveKickoff($state, new Position(6, 7));

        $types = array_map(fn($e) => $e->getType(), $result['events']);
        $this->assertContains('kick_skill', $types);
    }

    public function testKickMinimumOne(): void
    {
        // D6=1, with Kick: ceil(1/2) = 1
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::AWAY, 15, 7, skills: [SkillName::Kick], id: 1)
            ->addPlayer(TeamSide::HOME, 5, 7, id: 2)
            ->withPhase(GamePhase::SETUP)
            ->withBallOffPitch()
            ->build();

        // D8=1, D6=1 → Kick: ceil(1/2)=1
        // Kickoff table: 4+4=8 (Changing Weather), weather: 3+4 (Nice)
        // Ball bounce: D8=1
        $dice = new FixedDiceRoller([1, 1, 4, 4, 3, 4, 1]);
        $tzCalc = new TacklezoneCalculator();
        $scatterCalc = new ScatterCalculator();
        $ballResolver = new BallResolver($dice, $tzCalc, $scatterCalc);
        $kickoffResolver = new KickoffResolver($dice, $scatterCalc, $ballResolver);

        $result = $kickoffResolver->resolveKickoff($state, new Position(6, 7));

        $kickEvent = null;
        foreach ($result['events'] as $e) {
            if ($e->getType() === 'kick_skill') {
                $kickEvent = $e;
                break;
            }
        }
        $this->assertNotNull($kickEvent);
        $this->assertEquals(1, $kickEvent->getData()['reducedDistance']);
    }

    public function testLeaderPlusOneReroll(): void
    {
        // Team with Leader player gets +1 reroll at half-time
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, skills: [SkillName::Leader], id: 1)
            ->addPlayer(TeamSide::AWAY, 15, 5, id: 2)
            ->withHomeTeam(TeamStateDTO::create(1, 'Home', 'Human', TeamSide::HOME, 3))
            ->withBallOffPitch()
            ->build();

        $dice = new FixedDiceRoller([]);
        $gfr = new GameFlowResolver($dice);

        $result = $gfr->resolveHalfTime($state);

        // Home should have 3+1=4 rerolls
        $this->assertEquals(4, $result['state']->getHomeTeam()->getRerolls());
        // Away should have 2 (no Leader)
        $this->assertEquals(2, $result['state']->getAwayTeam()->getRerolls());
    }

    public function testLeaderMaxOneBonus(): void
    {
        // Two Leader players → still only +1 reroll
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, skills: [SkillName::Leader], id: 1)
            ->addPlayer(TeamSide::HOME, 5, 6, skills: [SkillName::Leader], id: 2)
            ->addPlayer(TeamSide::AWAY, 15, 5, id: 3)
            ->withHomeTeam(TeamStateDTO::create(1, 'Home', 'Human', TeamSide::HOME, 3))
            ->withBallOffPitch()
            ->build();

        $dice = new FixedDiceRoller([]);
        $gfr = new GameFlowResolver($dice);

        $result = $gfr->resolveHalfTime($state);

        // Still 3+1=4, not 3+2=5
        $this->assertEquals(4, $result['state']->getHomeTeam()->getRerolls());
    }

    public function testNoKickNormalScatter(): void
    {
        // Without Kick skill, scatter distance is not halved
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::AWAY, 15, 7, id: 1) // no Kick skill
            ->addPlayer(TeamSide::HOME, 5, 7, id: 2)
            ->withPhase(GamePhase::SETUP)
            ->withBallOffPitch()
            ->build();

        // D8=1, D6=6, kickoff table: 4+4=8 (Changing Weather), weather: 3+4
        // Ball bounce: D8=1
        $dice = new FixedDiceRoller([1, 6, 4, 4, 3, 4, 1]);
        $tzCalc = new TacklezoneCalculator();
        $scatterCalc = new ScatterCalculator();
        $ballResolver = new BallResolver($dice, $tzCalc, $scatterCalc);
        $kickoffResolver = new KickoffResolver($dice, $scatterCalc, $ballResolver);

        $result = $kickoffResolver->resolveKickoff($state, new Position(6, 7));

        $types = array_map(fn($e) => $e->getType(), $result['events']);
        $this->assertNotContains('kick_skill', $types);
    }

    // ========== STEP 5: Secret Weapon, Take Root ==========

    public function testSecretWeaponEjectedAfterTouchdown(): void
    {
        // Player with SecretWeapon is ejected during post-touchdown
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, skills: [SkillName::SecretWeapon], id: 1)
            ->addPlayer(TeamSide::HOME, 10, 5, id: 2)
            ->addPlayer(TeamSide::AWAY, 15, 5, id: 3)
            ->withBallOffPitch()
            ->build();

        $dice = new FixedDiceRoller([]);
        $gfr = new GameFlowResolver($dice);

        $result = $gfr->resolvePostTouchdown($state);

        // Secret weapon player should be ejected
        $sw = $result['state']->getPlayer(1);
        $this->assertSame(PlayerState::EJECTED, $sw->getState());
        $types = array_map(fn($e) => $e->getType(), $result['events']);
        $this->assertContains('secret_weapon', $types);
    }

    public function testSecretWeaponNormalPlay(): void
    {
        // SecretWeapon player can play normally during the drive
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, skills: [SkillName::SecretWeapon], id: 1)
            ->withBallOffPitch()
            ->build();

        // Normal move
        $dice = new FixedDiceRoller([]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::MOVE, [
            'playerId' => 1, 'x' => 6, 'y' => 5,
        ]);

        $this->assertTrue($result->isSuccess());
        $this->assertSame(PlayerState::STANDING, $result->getNewState()->getPlayer(1)->getState());
    }

    public function testTakeRootFailCantMove(): void
    {
        // TakeRoot player rolls 1 on move → can't move
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, skills: [SkillName::TakeRoot], id: 1)
            ->withBallOffPitch()
            ->build();

        $dice = new FixedDiceRoller([1]); // Take Root roll = 1 → rooted
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::MOVE, [
            'playerId' => 1, 'x' => 6, 'y' => 5,
        ]);

        // Action blocked (success, not turnover — like big guy fails)
        $this->assertTrue($result->isSuccess());
        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('take_root', $types);
        // Player didn't move
        $pos = $result->getNewState()->getPlayer(1)->getPosition();
        $this->assertEquals(5, $pos->getX());
    }

    public function testTakeRootDoesNotAffectBlock(): void
    {
        // TakeRoot player can block normally (no Take Root check)
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, skills: [SkillName::TakeRoot], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, armour: 10, id: 2)
            ->withBallOffPitch()
            ->build();

        // Block: roll 3 = PUSHED, armor holds (no armor roll for push only)
        $dice = new FixedDiceRoller([3]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);

        $this->assertTrue($result->isSuccess());
        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertNotContains('take_root', $types);
    }

    // ========== STEP 6: Hail Mary Pass, Dump-Off ==========

    public function testHailMaryPassAlwaysInaccurate(): void
    {
        // HMP: distance > 13, always inaccurate, scatter 3x, no interception
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 1, 5, agility: 3, skills: [SkillName::HailMaryPass], id: 1)
            ->addPlayer(TeamSide::HOME, 20, 5, agility: 3, id: 2)
            ->withBallCarried(1)
            ->build();

        // Not fumble roll: 4, scatter D8x3: 1,1,1, then ball bounces D8: 3
        $dice = new FixedDiceRoller([4, 1, 1, 1, 3]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::PASS, [
            'playerId' => 1,
            'targetX' => 20,
            'targetY' => 5,
        ]);

        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('hail_mary_pass', $types);
    }

    public function testHailMaryPassNoInterception(): void
    {
        // HMP: enemy in path is NOT checked for interception
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 1, 5, agility: 3, skills: [SkillName::HailMaryPass], id: 1)
            ->addPlayer(TeamSide::HOME, 20, 5, agility: 3, id: 2)
            ->addPlayer(TeamSide::AWAY, 10, 5, agility: 4, id: 3) // would intercept normally
            ->withBallCarried(1)
            ->build();

        // Not fumble: 4, scatter D8x3: 1,1,1, bounce D8: 3
        $dice = new FixedDiceRoller([4, 1, 1, 1, 3]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::PASS, [
            'playerId' => 1,
            'targetX' => 20,
            'targetY' => 5,
        ]);

        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertNotContains('interception', $types);
    }

    public function testHailMaryPassFumbleOnOne(): void
    {
        // HMP fumbles on natural 1
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 1, 5, agility: 3, skills: [SkillName::HailMaryPass], id: 1)
            ->addPlayer(TeamSide::HOME, 20, 5, agility: 3, id: 2)
            ->withBallCarried(1)
            ->build();

        // Roll 1 = fumble, bounce D8: 3
        $dice = new FixedDiceRoller([1, 3]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::PASS, [
            'playerId' => 1,
            'targetX' => 20,
            'targetY' => 5,
        ]);

        $this->assertTrue($result->isTurnover());
        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('hail_mary_pass', $types);
    }

    public function testDumpOffQuickPassBeforeBlock(): void
    {
        // Defender with DumpOff and ball: quick pass before block resolves
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, id: 1) // attacker
            ->addPlayer(TeamSide::AWAY, 6, 5, agility: 3, skills: [SkillName::DumpOff], id: 2) // defender with ball
            ->addPlayer(TeamSide::AWAY, 8, 5, agility: 3, id: 3) // dump-off target
            ->withBallCarried(2)
            ->build();

        // Dump-off: accuracy roll 4 (quick pass target 3+), catch roll 4
        // Block: roll 3 = PUSHED
        $dice = new FixedDiceRoller([4, 4, 3]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);

        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('dump_off', $types);
        // Ball should now be with the dump-off target
        $this->assertEquals(3, $result->getNewState()->getBall()->getCarrierId());
    }

    public function testDumpOffFumbleBallBounces(): void
    {
        // Dump-off fumbles → ball bounces, block continues
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, agility: 3, skills: [SkillName::DumpOff], id: 2)
            ->addPlayer(TeamSide::AWAY, 8, 5, agility: 3, id: 3)
            ->withBallCarried(2)
            ->build();

        // Dump-off: accuracy roll 1 (fumble), bounce D8: 3
        // Block: roll 3 = PUSHED
        $dice = new FixedDiceRoller([1, 3, 3]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);

        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('dump_off', $types);
        // Ball should NOT be with defender anymore
        $this->assertNotEquals(2, $result->getNewState()->getBall()->getCarrierId());
    }

    public function testDumpOffNotTriggeredIfPassUsed(): void
    {
        // If pass already used this turn, Dump-Off doesn't trigger
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, agility: 3, skills: [SkillName::DumpOff], id: 2)
            ->addPlayer(TeamSide::AWAY, 8, 5, agility: 3, id: 3)
            ->withBallCarried(2)
            ->withAwayTeam(TeamStateDTO::create(2, 'Away', 'Orc', TeamSide::AWAY, 2)->withPassUsed())
            ->build();

        // Block: roll 3 = PUSHED (no dump-off rolls needed)
        $dice = new FixedDiceRoller([3]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);

        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertNotContains('dump_off', $types);
    }
}
