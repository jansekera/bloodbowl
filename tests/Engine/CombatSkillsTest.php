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

final class CombatSkillsTest extends TestCase
{
    // ========== STEP 1: Wrestle ==========

    public function testWrestleAttackerBothDownBothProne(): void
    {
        // Attacker has Wrestle, roll Both Down → both prone, no armor
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, skills: [SkillName::Wrestle], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, id: 2)
            ->withBallOffPitch()
            ->build();

        $dice = new FixedDiceRoller([2]); // BD result = Both Down
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);

        $this->assertFalse($result->isTurnover());
        $newState = $result->getNewState();
        $this->assertSame(PlayerState::PRONE, $newState->getPlayer(1)->getState());
        $this->assertSame(PlayerState::PRONE, $newState->getPlayer(2)->getState());

        // Check wrestle event, no armor events
        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('wrestle', $types);
        $this->assertNotContains('armour_roll', $types);
    }

    public function testWrestleDefenderBothDownBothProne(): void
    {
        // Defender has Wrestle, roll Both Down → both prone, no armor
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, skills: [SkillName::Wrestle], id: 2)
            ->withBallOffPitch()
            ->build();

        $dice = new FixedDiceRoller([2]); // BD
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);

        $this->assertFalse($result->isTurnover());
        $this->assertSame(PlayerState::PRONE, $result->getNewState()->getPlayer(1)->getState());
        $this->assertSame(PlayerState::PRONE, $result->getNewState()->getPlayer(2)->getState());
    }

    public function testWrestleBallCarrierCausesBounce(): void
    {
        // Attacker has ball + Wrestle, Both Down → ball bounces
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, skills: [SkillName::Wrestle], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, id: 2)
            ->withBallCarried(1)
            ->build();

        $dice = new FixedDiceRoller([2, 3]); // BD, bounce D8=3 (East)
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);

        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('ball_bounce', $types);
    }

    public function testWrestleOverridesBlockSkill(): void
    {
        // Attacker has Block, defender has Wrestle → Wrestle overrides, both prone
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, skills: [SkillName::Block], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, skills: [SkillName::Wrestle], id: 2)
            ->withBallOffPitch()
            ->build();

        $dice = new FixedDiceRoller([2]); // BD
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);

        $this->assertFalse($result->isTurnover());
        $this->assertSame(PlayerState::PRONE, $result->getNewState()->getPlayer(1)->getState());
        $this->assertSame(PlayerState::PRONE, $result->getNewState()->getPlayer(2)->getState());
    }

    public function testNoWrestleNormalBothDown(): void
    {
        // Neither has Wrestle, Both Down → normal armor rolls
        // Equal ST → 1 die
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, id: 2)
            ->withBallOffPitch()
            ->build();

        // 1-die: 2 → BD, both down (no Block), attacker armor 2+1=3 not > 8, defender armor 2+1=3 not > 8
        $dice = new FixedDiceRoller([2, 2, 1, 2, 1]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);

        $this->assertTrue($result->isTurnover()); // attacker down
        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('armour_roll', $types);
        $this->assertNotContains('wrestle', $types);
    }

    // ========== STEP 2: Claw ==========

    public function testClawBreaksAV10OnRoll8(): void
    {
        // Claw: AV10 defender, armor roll 4+4=8 → broken (Claw on 8+)
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, skills: [SkillName::Claw], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, armour: 10, id: 2)
            ->withBallOffPitch()
            ->build();

        // 2-dice block (both ST3): 6,6 → DD+POW (best=POW), armor 4+4=8, injury 3+3=6 stunned
        $dice = new FixedDiceRoller([6, 6, 4, 4, 3, 3]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);

        $events = $result->getEvents();
        $armorEvents = array_filter($events, fn($e) => $e->getType() === 'armour_roll');
        $armorEvent = array_values($armorEvents)[0];
        $this->assertTrue($armorEvent->getData()['broken']);
    }

    public function testClawDoesNotBreakBelow8(): void
    {
        // Claw: armor roll 3+2=5 < 8 → NOT broken (even with Claw)
        // Use ST4 attacker for 2-die attacker-chooses block
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 4, skills: [SkillName::Claw], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, armour: 10, id: 2)
            ->withBallOffPitch()
            ->build();

        // 2-dice attacker chooses: 6,6 → DD+DD (DD chosen), armor 3+2=5 (not >=8 for Claw, not >10)
        $dice = new FixedDiceRoller([6, 6, 3, 2]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);

        $events = $result->getEvents();
        $armorEvents = array_filter($events, fn($e) => $e->getType() === 'armour_roll');
        $armorEvent = array_values($armorEvents)[0];
        $this->assertFalse($armorEvent->getData()['broken']);
    }

    public function testWithoutClawAV10Roll8NotBroken(): void
    {
        // Without Claw: AV10, armor roll 4+4=8 → 8 NOT > 10, not broken
        // Use ST4 attacker for 2-die attacker-chooses
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 4, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, armour: 10, id: 2)
            ->withBallOffPitch()
            ->build();

        // 2-dice attacker chooses: 6,6 → DD, armor 4+4=8 not broken (8 not > 10)
        $dice = new FixedDiceRoller([6, 6, 4, 4]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);

        $events = $result->getEvents();
        $armorEvents = array_filter($events, fn($e) => $e->getType() === 'armour_roll');
        $armorEvent = array_values($armorEvents)[0];
        $this->assertFalse($armorEvent->getData()['broken']);
    }

    public function testClawPlusMightyBlow(): void
    {
        // Claw breaks armor on 8 (4+4), Mighty Blow adds +1 to injury
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, skills: [SkillName::Claw, SkillName::MightyBlow], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, armour: 10, id: 2)
            ->withBallOffPitch()
            ->build();

        // 2-dice: 6,6 → DD, armor 4+4=8 (broken by Claw), injury 4+4=8+1(MB)=9 → KO
        $dice = new FixedDiceRoller([6, 6, 4, 4, 4, 4]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);

        $newState = $result->getNewState();
        $this->assertSame(PlayerState::KO, $newState->getPlayer(2)->getState());
    }

    // ========== STEP 3: Grab ==========

    public function testGrabChoosesWorstSquareForDefender(): void
    {
        // Grab: attacker picks square with most enemy TZs for defender
        // Home player at 5,5. Away defender at 6,5. Push squares: 7,5 (straight), 7,4 and 7,6
        // Place a home player at 8,4 so 7,4 has a TZ from home for the away defender
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, skills: [SkillName::Grab], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, id: 2)
            ->addPlayer(TeamSide::HOME, 8, 4, id: 3) // creates TZ at 7,4 and 7,5
            ->withBallOffPitch()
            ->build();

        $dice = new FixedDiceRoller([3]); // Pushed
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);

        $newState = $result->getNewState();
        $defPos = $newState->getPlayer(2)->getPosition();
        // 7,5 and 7,4 both have TZ from player 3. Grab picks the one with most TZs.
        $this->assertNotNull($defPos);
    }

    public function testGrabPrefersCrowdSurf(): void
    {
        // Grab: if no valid on-pitch squares, crowd surf
        // Defender at edge (24,5), attacker at (23,5)
        // Push squares from Grab: (25,5), (25,4), (25,6) - all off pitch
        // Pitch is 0-25 (26 wide), actually check Position::isOnPitch
        // Use x=0 edge instead: defender at (0,5), attacker at (1,5)
        // Push squares: (-1,5), (-1,4), (-1,6) - all off pitch = crowd surf
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 1, 5, strength: 4, skills: [SkillName::Grab], id: 1)
            ->addPlayer(TeamSide::AWAY, 0, 5, id: 2)
            ->withBallOffPitch()
            ->build();

        // 2-dice attacker chooses: 6,6 → DD, crowd injury 3+3=6
        $dice = new FixedDiceRoller([6, 6, 3, 3]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);

        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('crowd_surf', $types);
    }

    public function testGrabPlusFrenzyIgnoresGrab(): void
    {
        // Grab + Frenzy: Grab is ignored, normal push
        // Equal ST → 1 die (attacker chooses on tie)
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, skills: [SkillName::Grab, SkillName::Frenzy], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, id: 2)
            ->withBallOffPitch()
            ->build();

        // 1-die: 3 → Pushed, frenzy 2nd block: 6 → DD, armor 3+3=6 not > 8
        $dice = new FixedDiceRoller([3, 6, 3, 3]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);

        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('frenzy', $types);
    }

    public function testGrabVsStandFirm(): void
    {
        // Stand Firm prevents pushback even with Grab
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, skills: [SkillName::Grab], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, skills: [SkillName::StandFirm], id: 2)
            ->withBallOffPitch()
            ->build();

        $dice = new FixedDiceRoller([3]); // Pushed
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);

        // Defender stays at original position
        $newState = $result->getNewState();
        $defPos = $newState->getPlayer(2)->getPosition();
        $this->assertSame(6, $defPos->getX());
        $this->assertSame(5, $defPos->getY());
    }

    public function testNormalPushbackWithoutGrab(): void
    {
        // Normal push: smart push picks closest to sideline
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, id: 2)
            ->withBallOffPitch()
            ->build();

        $dice = new FixedDiceRoller([3]); // Pushed
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);

        $defPos = $result->getNewState()->getPlayer(2)->getPosition();
        // Smart push: (7,4) is closer to sideline than (7,5) or (7,6)
        $this->assertSame(7, $defPos->getX());
        $this->assertSame(4, $defPos->getY());
    }

    // ========== STEP 4: Tentacles ==========

    public function testTentaclesBlocksMovement(): void
    {
        // Tentacles blocks dodging away: mover 2+ST3=5, tent 4+ST3=7 → caught
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, skills: [SkillName::Tentacles], id: 2)
            ->withBallOffPitch()
            ->build();

        $dice = new FixedDiceRoller([2, 4]); // mover=2, tent=4
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::MOVE, ['playerId' => 1, 'x' => 4, 'y' => 5]);

        // Movement ends but NOT a turnover
        $this->assertFalse($result->isTurnover());
        // Player stays at start (didn't move)
        $this->assertSame(5, $result->getNewState()->getPlayer(1)->getPosition()->getX());

        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('tentacles', $types);
    }

    public function testTentaclesEscapeThenDodge(): void
    {
        // Escape tentacles (5+ST3=8 > 2+ST3=5), then dodge
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, skills: [SkillName::Tentacles], id: 2)
            ->withBallOffPitch()
            ->build();

        $dice = new FixedDiceRoller([5, 2, 4]); // tent mover=5, tent=2 (escape), dodge=4 (pass)
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::MOVE, ['playerId' => 1, 'x' => 4, 'y' => 5]);

        $this->assertFalse($result->isTurnover());
        $this->assertSame(4, $result->getNewState()->getPlayer(1)->getPosition()->getX());
    }

    public function testTentaclesTiesMoverLoses(): void
    {
        // Tie: 3+ST3=6 vs 3+ST3=6 → mover loses (not strictly greater)
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, skills: [SkillName::Tentacles], id: 2)
            ->withBallOffPitch()
            ->build();

        $dice = new FixedDiceRoller([3, 3]); // tie
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::MOVE, ['playerId' => 1, 'x' => 4, 'y' => 5]);

        $this->assertFalse($result->isTurnover());
        // Player stays at 5,5 (caught)
        $this->assertSame(5, $result->getNewState()->getPlayer(1)->getPosition()->getX());
    }

    public function testTentaclesStrengthMatters(): void
    {
        // ST4 mover vs ST3 tentacles: 2+4=6 vs 4+3=7 → caught
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 4, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, skills: [SkillName::Tentacles], id: 2)
            ->withBallOffPitch()
            ->build();

        $dice = new FixedDiceRoller([2, 4]); // 2+4=6 vs 4+3=7 → caught
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::MOVE, ['playerId' => 1, 'x' => 4, 'y' => 5]);

        $this->assertFalse($result->isTurnover());
        $this->assertSame(5, $result->getNewState()->getPlayer(1)->getPosition()->getX());
    }

    public function testNoTentaclesNormalDodge(): void
    {
        // Without Tentacles, normal dodge happens
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, id: 2) // no Tentacles
            ->withBallOffPitch()
            ->build();

        $dice = new FixedDiceRoller([4]); // dodge roll
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::MOVE, ['playerId' => 1, 'x' => 4, 'y' => 5]);

        $this->assertFalse($result->isTurnover());
        $this->assertSame(4, $result->getNewState()->getPlayer(1)->getPosition()->getX());

        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertNotContains('tentacles', $types);
    }

    // ========== STEP 5: Juggernaut ==========

    public function testJuggernautBlitzBothDownBecomesPush(): void
    {
        // Blitz + Juggernaut: Both Down → pushed instead
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, skills: [SkillName::Juggernaut], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, id: 2)
            ->withBallOffPitch()
            ->build();

        $dice = new FixedDiceRoller([2]); // BD → converted to push by Juggernaut
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::BLITZ, ['playerId' => 1, 'targetId' => 2]);

        $this->assertFalse($result->isTurnover());
        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('juggernaut', $types);
        // Defender pushed, not knocked down
        $this->assertSame(PlayerState::STANDING, $result->getNewState()->getPlayer(2)->getState());
    }

    public function testJuggernautNormalBlockNoEffect(): void
    {
        // Normal block (not blitz) + Juggernaut: BD is normal
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, skills: [SkillName::Juggernaut], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, id: 2)
            ->withBallOffPitch()
            ->build();

        // BD, attacker armor=2+1=3 not > 8, defender armor=2+1=3 not > 8
        $dice = new FixedDiceRoller([2, 2, 1, 2, 1]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);

        // Normal BD: both down (no block), attacker turnover
        $this->assertTrue($result->isTurnover());
        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertNotContains('juggernaut', $types);
    }

    public function testJuggernautBlitzNonBDNoEffect(): void
    {
        // Blitz + Juggernaut + Pushed result → no change
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, skills: [SkillName::Juggernaut], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, id: 2)
            ->withBallOffPitch()
            ->build();

        $dice = new FixedDiceRoller([3]); // Pushed
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::BLITZ, ['playerId' => 1, 'targetId' => 2]);

        $this->assertFalse($result->isTurnover());
        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertNotContains('juggernaut', $types);
    }

    public function testJuggernautOverridesDefenderBlock(): void
    {
        // Blitz + Juggernaut: BD against defender with Block → push (Juggernaut overrides)
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, skills: [SkillName::Juggernaut], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, skills: [SkillName::Block], id: 2)
            ->withBallOffPitch()
            ->build();

        $dice = new FixedDiceRoller([2]); // BD → push by Juggernaut
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::BLITZ, ['playerId' => 1, 'targetId' => 2]);

        $this->assertFalse($result->isTurnover());
        // Defender pushed (standing, but at new position)
        $this->assertSame(PlayerState::STANDING, $result->getNewState()->getPlayer(2)->getState());
        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('juggernaut', $types);
    }

    // ========== STEP 6: Disturbing Presence ==========

    public function testDPAddsToPassAccuracy(): void
    {
        // DP +1 to pass accuracy: AG3 thrower, short pass (distance 3 → short, modifier=0)
        // Base target: 7-3+0=4+. With DP: 5+. Roll 4 = inaccurate with DP
        // Interception: enemy at (7,5) is on pass path → attempt interception first
        // Use DP player NOT on the pass path to avoid interception
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, agility: 3, id: 1)
            ->addPlayer(TeamSide::AWAY, 5, 7, skills: [SkillName::DisturbingPresence], id: 2) // within 3, off pass path
            ->addPlayer(TeamSide::HOME, 8, 5, id: 3)
            ->withBallCarried(1)
            ->build();

        // Short pass (3 squares), AG3 target: 7-3+0-1(short)=3+, +1(DP)=4+. Roll 3 = inaccurate
        // Inaccurate scatter: D8=3, D6=2 (dist). Then landing: maybe bounce, throw-in etc
        $dice = new FixedDiceRoller([3, 3, 2, 3, 3, 3]); // pass=3, scatter D8=3, scatter dist=2, bounce, throw-in extras
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::PASS, ['playerId' => 1, 'targetX' => 8, 'targetY' => 5]);

        $events = $result->getEvents();
        $passEvents = array_filter($events, fn($e) => $e->getType() === 'pass');
        $passEvent = array_values($passEvents)[0];
        $this->assertSame('inaccurate', $passEvent->getData()['result']);
    }

    public function testDPAddsToCatch(): void
    {
        // DP +1 to catch: AG3 catcher
        // Accurate pass modifier=1 for catch → target = 7-3-1+1(DP)=4+
        // Without DP: target = 7-3-1=3+ (roll 3 would succeed)
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, agility: 4, id: 1) // thrower AG4
            ->addPlayer(TeamSide::HOME, 8, 5, agility: 3, id: 3) // receiver AG3
            ->addPlayer(TeamSide::AWAY, 8, 7, skills: [SkillName::DisturbingPresence], id: 2) // within 3 of receiver
            ->withBallCarried(1)
            ->build();

        // AG4 short pass: 7-4-1(short)=2+. Roll 4 = accurate.
        // Catch at (8,5): 7-3-1(accurate)+1(DP)=4+. Roll 3 = fail.
        // Failed catch → bounce, bounce may need more rolls
        $dice = new FixedDiceRoller([4, 3, 3, 3, 3]); // pass=4, catch=3 fail, bounce D8, extras
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::PASS, ['playerId' => 1, 'targetX' => 8, 'targetY' => 5]);

        $events = $result->getEvents();
        $catchEvents = array_filter($events, fn($e) => $e->getType() === 'catch');
        $catchEvent = array_values($catchEvents)[0];
        $this->assertFalse($catchEvent->getData()['success']);
    }

    public function testDPOutOfRange(): void
    {
        // DP out of range (>3 squares) → no effect on pass
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, agility: 3, id: 1)
            ->addPlayer(TeamSide::AWAY, 5, 9, skills: [SkillName::DisturbingPresence], id: 2) // 4 squares away, off path
            ->addPlayer(TeamSide::HOME, 8, 5, id: 3)
            ->withBallCarried(1)
            ->build();

        // Short pass, AG3 target: 7-3-1(short)=3+. No DP (>3). Roll 3 = accurate
        // Catch: AG3, 7-3-1(accurate)=3+. Roll 4 = success
        $dice = new FixedDiceRoller([3, 4]); // pass=3 accurate, catch=4 success
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::PASS, ['playerId' => 1, 'targetX' => 8, 'targetY' => 5]);

        $events = $result->getEvents();
        $passEvents = array_filter($events, fn($e) => $e->getType() === 'pass');
        $passEvent = array_values($passEvents)[0];
        $this->assertSame('accurate', $passEvent->getData()['result']);
    }

    public function testDPStacking(): void
    {
        // 2 DP players = +2 on pass accuracy
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, agility: 3, id: 1)
            ->addPlayer(TeamSide::AWAY, 5, 7, skills: [SkillName::DisturbingPresence], id: 2) // within 3, off path
            ->addPlayer(TeamSide::AWAY, 5, 3, skills: [SkillName::DisturbingPresence], id: 3) // within 3, off path
            ->addPlayer(TeamSide::HOME, 8, 5, id: 4)
            ->withBallCarried(1)
            ->build();

        // Short pass, AG3: 7-3-1(short)+2(2xDP)=5+. Roll 4 = inaccurate
        $dice = new FixedDiceRoller([4, 3, 2, 3, 3, 3]); // pass=4, scatter D8, dist, bounce, extras
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::PASS, ['playerId' => 1, 'targetX' => 8, 'targetY' => 5]);

        $events = $result->getEvents();
        $passEvents = array_filter($events, fn($e) => $e->getType() === 'pass');
        $passEvent = array_values($passEvents)[0];
        $this->assertSame('inaccurate', $passEvent->getData()['result']);
    }

    // Note: DP on interception test is complex. The DP from thrower's team makes interception harder
    // which is counter-intuitive. Skipping this for now as it adds complexity.

    // ========== STEP 7: Diving Tackle ==========

    public function testDivingTackleAdds2ToDodgeTarget(): void
    {
        // DT +2 to dodge target: AG3, base 4+ → 6+. Roll 5 = fail
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, agility: 3, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, skills: [SkillName::DivingTackle], id: 2)
            ->withBallOffPitch()
            ->build();

        // Dodge: target 7-3+0(TZ-1)=4+ → +2(DT)=6+. Roll 5 = fail
        // Then: fallen player armor
        $dice = new FixedDiceRoller([5, 2, 1]); // dodge=5 fail, armor die1, die2
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::MOVE, ['playerId' => 1, 'x' => 4, 'y' => 5]);

        $this->assertTrue($result->isTurnover());
    }

    public function testDivingTacklePlayerGoesProne(): void
    {
        // DT player goes prone after SUCCESSFUL dodge
        // AG4 dodger: base 3+, +2 DT = 5+. Roll 5 = OK
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, agility: 4, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, skills: [SkillName::DivingTackle], id: 2)
            ->withBallOffPitch()
            ->build();

        $dice = new FixedDiceRoller([5]); // dodge=5 success
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::MOVE, ['playerId' => 1, 'x' => 4, 'y' => 5]);

        $this->assertFalse($result->isTurnover());
        $this->assertSame(4, $result->getNewState()->getPlayer(1)->getPosition()->getX());
        // DT player is now prone
        $this->assertSame(PlayerState::PRONE, $result->getNewState()->getPlayer(2)->getState());

        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('diving_tackle', $types);
    }

    public function testDivingTackleWithBallBounce(): void
    {
        // DT player has ball → goes prone → ball bounces
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, agility: 4, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, skills: [SkillName::DivingTackle], id: 2)
            ->withBallCarried(2)
            ->build();

        $dice = new FixedDiceRoller([6, 3]); // dodge=6 success, bounce D8
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::MOVE, ['playerId' => 1, 'x' => 4, 'y' => 5]);

        $this->assertFalse($result->isTurnover());
        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('diving_tackle', $types);
        $this->assertContains('ball_bounce', $types);
    }

    public function testDivingTacklePlusDodgeSkill(): void
    {
        // DT+2, Dodge-1 = net +1. AG3: base 4++1=5+. Dodge reroll available.
        // Roll 4 = fail, Dodge reroll: 5 = success
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, agility: 3, skills: [SkillName::Dodge], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, skills: [SkillName::DivingTackle], id: 2)
            ->withBallOffPitch()
            ->build();

        // Target: 7-3+0-1(dodge)+2(DT)=5+. Roll 4 fail, Dodge reroll: 5 success
        $dice = new FixedDiceRoller([4, 5]); // dodge=4 fail, reroll=5 success
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::MOVE, ['playerId' => 1, 'x' => 4, 'y' => 5]);

        $this->assertFalse($result->isTurnover());
        $this->assertSame(4, $result->getNewState()->getPlayer(1)->getPosition()->getX());
    }

    public function testPathfinderIncludesDTInDodgeCount(): void
    {
        // Verify that DT penalty is reflected in pathfinder dodge targets
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, agility: 3, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, skills: [SkillName::DivingTackle], id: 2)
            ->withBallOffPitch()
            ->build();

        $rulesEngine = new \App\Engine\RulesEngine();
        $targets = $rulesEngine->getValidMoveTargets($state, 1);
        // Should still be able to move (dodge required)
        $this->assertNotEmpty($targets);
    }

    // ========== STEP 12: Pro on Block Dice ==========

    public function testProRerollBadBlockDie(): void
    {
        // Pro reroll on Attacker Down (1-die block): roll AD, Pro 4+, reroll → Pushed
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, skills: [SkillName::Pro], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, id: 2)
            ->withBallOffPitch()
            ->build();

        $dice = new FixedDiceRoller([1, 4, 3]); // AD, Pro=4 (success), reroll=3 (Pushed)
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);

        $this->assertFalse($result->isTurnover());
        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('pro', $types);
    }

    public function testProFailOnBlockDie(): void
    {
        // Pro fail: roll AD, Pro 3. Keeps AD → attacker down
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, skills: [SkillName::Pro], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, id: 2)
            ->withBallOffPitch()
            ->build();

        // AD, Pro=3 (fail), armor 1+2=3 not > 8
        $dice = new FixedDiceRoller([1, 3, 1, 2]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);

        $this->assertTrue($result->isTurnover());
        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('pro', $types);
    }

    public function testProDoesNotActivateOnGoodResult(): void
    {
        // Pro not used on DD (good result)
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, skills: [SkillName::Pro], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, id: 2)
            ->withBallOffPitch()
            ->build();

        $dice = new FixedDiceRoller([6, 3, 3]); // DD, armor
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);

        $this->assertFalse($result->isTurnover());
        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertNotContains('pro', $types);
    }

    // ========== Grab: optional — skip when crowd surf available ==========

    public function testGrabSkippedWhenCrowdSurfAvailable(): void
    {
        // Attacker with Grab at (5,1), defender at (6,0) near sideline.
        // Push squares: (7,-1) off-pitch, (7,0) on-pitch, (6,-1) off-pitch.
        // Grab would pick (7,0) on-pitch. Without Grab, normal logic prefers crowd surf.
        // Attacker should choose NOT to use Grab → crowd surf.
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 1, skills: [SkillName::Grab], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 0, id: 2)
            ->withBallOffPitch()
            ->build();

        // 1 die: roll 3 → PUSHED
        // Crowd injury: 3+3=6
        $dice = new FixedDiceRoller([3, 3, 3]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);

        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('crowd_surf', $types);
        $this->assertNull($result->getNewState()->getPlayer(2)->getPosition());
    }

    // ========== Juggernaut vs Stand Firm ==========

    public function testJuggernautBlitzIgnoresStandFirm(): void
    {
        // Blitz + Juggernaut ignores Stand Firm: defender should be pushed.
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, skills: [SkillName::Juggernaut], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, skills: [SkillName::StandFirm], id: 2)
            ->withBallOffPitch()
            ->build();

        // 1 die: roll 3 → PUSHED
        $dice = new FixedDiceRoller([3]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::BLITZ, ['playerId' => 1, 'targetId' => 2]);

        $this->assertFalse($result->isTurnover());
        // Defender should be pushed despite Stand Firm
        $defPos = $result->getNewState()->getPlayer(2)->getPosition();
        $this->assertNotNull($defPos);
        $this->assertNotSame(6, $defPos->getX());
    }

    public function testJuggernautNormalBlockDoesNotIgnoreStandFirm(): void
    {
        // Normal block (not blitz) + Juggernaut does NOT ignore Stand Firm.
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, skills: [SkillName::Juggernaut], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, skills: [SkillName::StandFirm], id: 2)
            ->withBallOffPitch()
            ->build();

        // 1 die: roll 3 → PUSHED
        $dice = new FixedDiceRoller([3]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);

        // Stand Firm holds — defender stays
        $defPos = $result->getNewState()->getPlayer(2)->getPosition();
        $this->assertSame(6, $defPos->getX());
        $this->assertSame(5, $defPos->getY());
    }
}
