<?php
declare(strict_types=1);

namespace App\Tests\Engine;

use App\Engine\ActionResolver;
use App\Engine\FixedDiceRoller;
use App\Enum\ActionType;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use PHPUnit\Framework\TestCase;

final class ChainPushTest extends TestCase
{
    /**
     * Push into occupied square → chain push occurs, both players move.
     *
     * Setup: Attacker at (5,7), Defender at (6,7), Blocker at (7,7).
     * Push squares for defender: (7,7) occupied, (7,6), (7,8).
     * (7,6) and (7,8) are empty, so no chain push needed.
     * For chain push: we need ALL push squares occupied.
     *
     * Better setup: Attacker at (5,7), Defender at (6,7).
     * Push squares: (7,7), (7,6), (7,8) — all occupied by teammates/enemies.
     * Chain push on (7,7): that player pushes to (8,7) which is empty.
     */
    public function testChainPushIntoOccupiedSquare(): void
    {
        // Attacker ST6 to guarantee 3-dice attacker chooses vs ST3
        // AWAY players in all 3 push squares — they assist defender (+3) but ST6 vs ST6 = 1 die
        // Actually, let's just use neutral positions. Place blockers far from the attacker.
        // Attacker at (10,7), defender at (11,7). Push squares: (12,7), (12,6), (12,8).
        // Occupy all three with HOME players (they don't assist defender since same team as attacker).
        // HOME assists to attacker from (12,*): they're adjacent to defender at (11,7) → +3 assist.
        // Attacker ST3 + 3 assists = 6 vs defender ST3 → 3 dice attacker chooses.
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 10, 7, id: 1) // attacker
            ->addPlayer(TeamSide::AWAY, 11, 7, id: 2) // defender
            ->addPlayer(TeamSide::HOME, 12, 7, id: 3) // in push square (chain target)
            ->addPlayer(TeamSide::HOME, 12, 6, id: 4) // in push square
            ->addPlayer(TeamSide::HOME, 12, 8, id: 5) // in push square
            ->withBallOffPitch()
            ->build();

        // 3 dice attacker chooses: rolls 3,3,3 → all PUSHED, pick PUSHED
        $dice = new FixedDiceRoller([3, 3, 3]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);

        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('chain_push', $types);

        $newState = $result->getNewState();
        // Defender pushed to (12,7), chain-pushed player 3 moved to (13,7)
        $defPos = $newState->requirePlayer(2)->requirePosition();
        $this->assertEquals(12, $defPos->getX());
        $this->assertEquals(7, $defPos->getY());

        $chainedPos = $newState->requirePlayer(3)->requirePosition();
        // smer sekundarniho odtlaceni voli tym na tahu -- kterekoli ze tri poli (r. 648-649)
        $this->assertEquals(13, $chainedPos->getX());
        $this->assertLessThanOrEqual(1, abs($chainedPos->getY() - 7));
    }

    /**
     * Chain push off-pitch → crowd surf for chain target.
     */
    public function testChainPushCrowdSurf(): void
    {
        // Attacker at (23,7), defender at (24,7), blocker at (25,7)=(off-pitch candidate)
        // Wait — pitch is 0-25 wide. Position 25 might be out of range.
        // Let me use: Attacker at (0,7), defender at (1,7), occupants at (2,6), (2,7), (2,8).
        // Chain target at (2,7), push squares for (2,7): (3,7), (3,6), (3,8) — all empty.
        // Not a crowd surf case.

        // For crowd surf: attacker at (23,7), defender at (24,7).
        // Push squares: (25,7)=off-pitch if pitch is 0..25.
        // Actually Position::PITCH_WIDTH = 26 (0-25), PITCH_HEIGHT = 15 (0-14).
        // So (25,7) is valid but (26,7) is off-pitch.

        // Better: attacker at (24,7), defender at (25,7) near sideline.
        // Push squares for defender: all off-pitch → regular crowd surf (no chain push).

        // For chain push crowd surf: occupant near edge.
        // Attacker at (22,7), defender at (23,7). Blocker at (24,7).
        // All three push squares: (24,7) occupied, (24,6), (24,8).
        // (24,6) and (24,8) also occupied → chain push into (24,7).
        // Chain target at (24,7), push squares: (25,7), (25,6), (25,8).
        // (25,7) is on pitch (last column). We need blocker at (25,7) too.
        // Chain of 24→25, then 25→off pitch.

        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 22, 7, id: 1) // attacker
            ->addPlayer(TeamSide::AWAY, 23, 7, id: 2) // defender
            ->addPlayer(TeamSide::AWAY, 24, 7, id: 3) // blocker in push square
            ->addPlayer(TeamSide::AWAY, 24, 6, id: 4) // blocker in push square
            ->addPlayer(TeamSide::AWAY, 24, 8, id: 5) // blocker in push square
            ->addPlayer(TeamSide::AWAY, 25, 7, id: 6) // will be chain-pushed to (26,7)=off-pitch
            ->addPlayer(TeamSide::AWAY, 25, 6, id: 7) // blocks chain push alt
            ->addPlayer(TeamSide::AWAY, 25, 8, id: 8) // blocks chain push alt
            ->withBallOffPitch()
            ->build();

        // 1 die: roll 3 → PUSHED
        // Chain: (24,7) player 3 → chain push to (25,7) occupied by 6
        // Chain: (25,7) player 6 → push to (26,7) off-pitch → crowd surf
        // Crowd injury rolls: 3+3=6
        $dice = new FixedDiceRoller([3, 3, 3]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);

        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('chain_push', $types);
        $this->assertContains('crowd_surf', $types);

        $newState = $result->getNewState();
        // Player 6 pushed off pitch
        $this->assertNull($newState->requirePlayer(6)->getPosition());
    }

    /**
     * ALL push squares have Stand Firm → cannot chain push → crowd surf.
     */
    public function testAllStandFirmCrowdSurf(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1) // attacker
            ->addPlayer(TeamSide::AWAY, 6, 7, id: 2) // defender
            ->addPlayer(TeamSide::AWAY, 7, 7, id: 3, skills: [SkillName::StandFirm])
            ->addPlayer(TeamSide::AWAY, 7, 6, id: 4, skills: [SkillName::StandFirm])
            ->addPlayer(TeamSide::AWAY, 7, 8, id: 5, skills: [SkillName::StandFirm])
            ->withBallOffPitch()
            ->build();

        // 1 die: roll 3 → PUSHED
        // All push squares occupied by Stand Firm → no chain push possible → crowd surf
        // Crowd injury rolls: 3+3=6
        $dice = new FixedDiceRoller([3, 3, 3]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);

        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('crowd_surf', $types);
        $this->assertNotContains('chain_push', $types);

        $newState = $result->getNewState();
        $this->assertNull($newState->requirePlayer(2)->getPosition());
        // All Stand Firm players unmoved
        $this->assertEquals(7, $newState->requirePlayer(3)->requirePosition()->getX());
    }

    /**
     * One Stand Firm occupant, others chain-pushable → chain push skips Stand Firm.
     */
    public function testStandFirmSkippedChainPushOther(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1) // attacker
            ->addPlayer(TeamSide::AWAY, 6, 7, id: 2) // defender
            ->addPlayer(TeamSide::AWAY, 7, 7, id: 3, skills: [SkillName::StandFirm]) // Stand Firm — skip
            ->addPlayer(TeamSide::AWAY, 7, 6, id: 4) // chain-pushable
            ->addPlayer(TeamSide::AWAY, 7, 8, id: 5) // chain-pushable
            ->withBallOffPitch()
            ->build();

        // 1 die: roll 3 → PUSHED
        // (7,7) Stand Firm skipped. (7,6) chain-pushable → chain push into (7,6)
        // Chain: player 4 from (7,6) → push to (8,5) or similar empty square
        $dice = new FixedDiceRoller([3]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);

        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('chain_push', $types);
        $this->assertNotContains('crowd_surf', $types);

        $newState = $result->getNewState();
        // Defender pushed to (7,6), player 4 chain-pushed away
        $defPos = $newState->requirePlayer(2)->requirePosition();
        // Stand Firm player unmoved
        $this->assertEquals(7, $newState->requirePlayer(3)->requirePosition()->getX());
        $this->assertEquals(7, $newState->requirePlayer(3)->requirePosition()->getY());
    }

    /**
     * Ball bounces when chain-pushed carrier moves.
     */
    public function testChainPushedCarrierKeepsBall(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1) // attacker
            ->addPlayer(TeamSide::AWAY, 6, 7, id: 2) // defender
            ->addPlayer(TeamSide::AWAY, 7, 7, id: 3) // ball carrier in push square
            ->addPlayer(TeamSide::AWAY, 7, 6, id: 4)
            ->addPlayer(TeamSide::AWAY, 7, 8, id: 5)
            ->withBallCarried(3) // ball carrier is chain push target
            ->build();

        // 1 die: roll 3 → PUSHED → chain push
        // Odtlaceny nosic mic DRZI -- neni sraženy (r. 635-653); jen Strip Ball blokujiciho ho vyrazi
        $dice = new FixedDiceRoller([3]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);

        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('chain_push', $types);
        $this->assertNotContains('ball_bounce', $types);

        // Mic zustava u hrace 3 i po odtlaceni
        $ball = $result->getNewState()->getBall();
        $this->assertEquals(3, $ball->getCarrierId());
    }

    /**
     * Side Step + chain push: all empty squares taken → Side Step defender chain pushes.
     */
    public function testSideStepWithChainPush(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 10, 7, id: 1) // attacker
            ->addPlayer(TeamSide::AWAY, 11, 7, id: 2, skills: [SkillName::SideStep]) // defender with Side Step
            ->addPlayer(TeamSide::HOME, 12, 7, id: 3) // in push square
            ->addPlayer(TeamSide::HOME, 12, 6, id: 4) // in push square
            ->addPlayer(TeamSide::HOME, 12, 8, id: 5) // in push square
            ->withBallOffPitch()
            ->build();

        // 3 dice attacker chooses (HOME assists): rolls 3,3,3 → PUSHED
        // Side Step: all push squares occupied → chain push
        // Defender with Side Step chooses safest (fewest enemy TZ)
        $dice = new FixedDiceRoller([3, 3, 3]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);

        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('chain_push', $types);
        $this->assertNotContains('crowd_surf', $types);

        // Defender should be at one of (12,7), (12,6), (12,8)
        $defPos = $result->getNewState()->requirePlayer(2)->requirePosition();
        $this->assertEquals(12, $defPos->getX());
    }

    /**
     * Side Step with empty square available → no chain push, normal Side Step.
     */
    public function testSideStepPrefersEmptyOverChainPush(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 10, 7, id: 1) // attacker
            ->addPlayer(TeamSide::AWAY, 11, 7, id: 2, skills: [SkillName::SideStep]) // defender
            ->addPlayer(TeamSide::HOME, 12, 7, id: 3) // in push square (occupied)
            ->addPlayer(TeamSide::HOME, 12, 6, id: 4) // in push square (occupied)
            // (12,8) is empty — Side Step should pick this
            ->withBallOffPitch()
            ->build();

        // 3 dice attacker chooses: rolls 3,3,3 → PUSHED
        $dice = new FixedDiceRoller([3, 3, 3]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);

        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertNotContains('chain_push', $types);

        $defPos = $result->getNewState()->requirePlayer(2)->requirePosition();
        $this->assertEquals(12, $defPos->getX());
        $this->assertEquals(8, $defPos->getY());
    }

    /**
     * Grab + chain push: all empty squares taken → Grab attacker chain pushes.
     */
    public function testGrabWithChainPush(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 10, 7, id: 1, skills: [SkillName::Grab]) // attacker with Grab
            ->addPlayer(TeamSide::AWAY, 11, 7, id: 2) // defender
            ->addPlayer(TeamSide::HOME, 12, 7, id: 3) // in push square
            ->addPlayer(TeamSide::HOME, 12, 6, id: 4) // in push square
            ->addPlayer(TeamSide::HOME, 12, 8, id: 5) // in push square
            ->withBallOffPitch()
            ->build();

        // 3 dice attacker chooses: rolls 3,3,3 → PUSHED
        $dice = new FixedDiceRoller([3, 3, 3]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);

        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('chain_push', $types);
        $this->assertNotContains('crowd_surf', $types);
    }

    /**
     * Triple chain: A pushes B into C into D into empty.
     */
    public function testTripleChainPush(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1) // attacker
            ->addPlayer(TeamSide::AWAY, 6, 7, id: 2) // defender
            ->addPlayer(TeamSide::AWAY, 7, 7, id: 3) // first chain
            ->addPlayer(TeamSide::AWAY, 7, 6, id: 10)
            ->addPlayer(TeamSide::AWAY, 7, 8, id: 11)
            ->addPlayer(TeamSide::AWAY, 8, 7, id: 4) // second chain
            ->addPlayer(TeamSide::AWAY, 8, 6, id: 12)
            ->addPlayer(TeamSide::AWAY, 8, 8, id: 13)
            // (9,7) is empty — end of chain
            ->withBallOffPitch()
            ->build();

        // 1 die: roll 3 → PUSHED
        $dice = new FixedDiceRoller([3]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);

        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        // Should have 2 chain_push events (for player 3 and player 4)
        $chainPushCount = count(array_filter($types, fn($t) => $t === 'chain_push'));
        $this->assertEquals(2, $chainPushCount);

        $newState = $result->getNewState();
        // Defender at (7,7), player 3 at (8,7), player 4 at (9,7)
        $this->assertEquals(7, $newState->requirePlayer(2)->requirePosition()->getX());
        $this->assertEquals(8, $newState->requirePlayer(3)->requirePosition()->getX());
        $this->assertEquals(9, $newState->requirePlayer(4)->requirePosition()->getX());
    }

    /**
     * Side Step plati i v RETEZU (FAQ: "The coach of the moving team decides all pushback
     * directions unless the pushed player has the Side Step skill"). Hrac 3 v retezu ma
     * jedine pole bez nasich zon -- diagonalu (13,6); tym na tahu by jinak vybral pole se zonou.
     */
    public function testSideStepAppliesToSecondaryPush(): void
    {
        $s = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 10, 7, id: 1)
            ->addPlayer(TeamSide::AWAY, 11, 7, id: 2)
            ->addPlayer(TeamSide::AWAY, 12, 7, skills: [SkillName::SideStep], id: 3)
            ->addPlayer(TeamSide::AWAY, 12, 6, id: 4)
            ->addPlayer(TeamSide::AWAY, 12, 8, id: 5)
            ->addPlayer(TeamSide::HOME, 14, 8, id: 6) // zona na (13,7) i (13,8); (13,6) bez zony
            ->withBallOffPitch()
            ->build();
        // krajni dva drzi pole (zakoreneni) -- retez pujde jen pres hrace 3
        $s = $s->withPlayer($s->requirePlayer(4)->withRooted(true));
        $s = $s->withPlayer($s->requirePlayer(5)->withRooted(true));

        // 1 kostka: 3 = Pushed
        $r = (new ActionResolver(new FixedDiceRoller([3])))->resolve($s, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);

        $pos = $r->getNewState()->requirePlayer(3)->requirePosition();
        $this->assertSame([13, 6], [$pos->getX(), $pos->getY()]);
    }
}
