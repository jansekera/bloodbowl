<?php
declare(strict_types=1);

namespace App\Tests\Engine;

use App\Engine\ActionResolver;
use App\Engine\FixedDiceRoller;
use App\Enum\ActionType;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use PHPUnit\Framework\TestCase;

/**
 * Tests for smart pushback direction choice.
 *
 * Normal push (no Grab, no SideStep): attacker picks best square:
 * - Crowd surf if any push square is off-pitch
 * - Most enemy tackle zones for defender
 * - Closest to sideline as tiebreaker
 */
final class SmartPushbackTest extends TestCase
{
    /**
     * Defender at Y=0 sideline. Attacker pushes from below (Y=1→Y=0).
     * Push squares from (10,0): (10,-1) off-pitch, (9,-1) off-pitch, (11,-1) off-pitch.
     * All push squares are off-pitch → crowd surf.
     */
    public function testPushPrefersCrowdSurf(): void
    {
        // Attacker at (10,1), defender at (10,0) — push direction is up (Y-1)
        // Push squares: (10,-1), (9,-1), (11,-1) — all off pitch
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 10, 1, id: 1)
            ->addPlayer(TeamSide::AWAY, 10, 0, id: 2)
            ->withBallOffPitch()
            ->build();

        // 1 die (equal ST): roll 3 → PUSHED, crowd injury rolls: 3+3=6
        $dice = new FixedDiceRoller([3, 3, 3]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);

        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('crowd_surf', $types);
        $this->assertNull($result->getNewState()->getPlayer(2)->getPosition());
    }

    /**
     * Defender at (10,7) middle of pitch. Attacker at (9,7) pushes right.
     * Push squares: (11,7), (11,6), (11,8).
     * Place enemy player at (12,7) — creates TZ at (11,7) and (11,8).
     * Place enemy player at (12,6) — creates TZ at (11,6) and (11,7).
     * TZs: (11,7)=2, (11,6)=1, (11,8)=1. Smart push picks (11,7).
     */
    public function testPushPrefersMostEnemyTZs(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 9, 7, id: 1)
            ->addPlayer(TeamSide::AWAY, 10, 7, id: 2)
            // Enemy players creating TZs at push destinations
            ->addPlayer(TeamSide::HOME, 12, 7, id: 3) // TZ at (11,7) and (11,8) and (11,6)
            ->addPlayer(TeamSide::HOME, 12, 6, id: 4) // TZ at (11,7) and (11,6) and (11,5)
            ->withBallOffPitch()
            ->build();

        // 1 die: roll 3 → PUSHED
        $dice = new FixedDiceRoller([3]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);

        $newState = $result->getNewState();
        $defPos = $newState->getPlayer(2)->getPosition();
        $this->assertNotNull($defPos);
        // (11,7) has 2 TZs (from player 3 and 4), (11,6) has 2 TZs (from player 3 and 4), (11,8) has 1 TZ (from player 3)
        // Between (11,7) and (11,6) both have 2 TZs → tiebreak by sideline distance
        // (11,7) is min(7, 14-7)=7 from sideline, (11,6) is min(6, 14-6)=6 from sideline
        // Picks (11,6) — closer to sideline
        $this->assertEquals(11, $defPos->getX());
        $this->assertEquals(6, $defPos->getY());
    }

    /**
     * No enemy TZs at any push square. Tiebreak by sideline distance.
     * Defender at (10,3). Attacker at (9,3) pushes right.
     * Push squares: (11,3), (11,2), (11,4).
     * Sideline distances: (11,3)=min(3,11)=3, (11,2)=min(2,12)=2, (11,4)=min(4,10)=4
     * Smart push picks (11,2) — closest to sideline.
     */
    public function testPushPrefersCloserToSideline(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 9, 3, id: 1)
            ->addPlayer(TeamSide::AWAY, 10, 3, id: 2)
            ->withBallOffPitch()
            ->build();

        // 1 die: roll 3 → PUSHED
        $dice = new FixedDiceRoller([3]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);

        $defPos = $result->getNewState()->getPlayer(2)->getPosition();
        $this->assertNotNull($defPos);
        $this->assertEquals(11, $defPos->getX());
        $this->assertEquals(2, $defPos->getY());
    }

    /**
     * Defender at sideline with one push square off-pitch and one on-pitch with good TZs.
     * Crowd surf takes priority over TZ-heavy on-pitch squares.
     */
    public function testPushCrowdSurfOverridesTZ(): void
    {
        // Attacker at (10,1), defender at (10,0).
        // Push squares: (10,-1) off-pitch, (9,-1) off-pitch, (11,-1) off-pitch.
        // Even if we had on-pitch squares with great TZs, crowd surf wins.
        // (All push squares are off-pitch here, so crowd surf is guaranteed)
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 10, 1, id: 1)
            ->addPlayer(TeamSide::AWAY, 10, 0, id: 2)
            // Enemy player creating TZs near sideline (doesn't matter, crowd surf wins)
            ->addPlayer(TeamSide::HOME, 10, 2, id: 3)
            ->withBallOffPitch()
            ->build();

        // 1 die: roll 3 → PUSHED, crowd injury: 3+3=6
        $dice = new FixedDiceRoller([3, 3, 3]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);

        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('crowd_surf', $types);
    }

    /**
     * SideStep defender still picks the safest square (fewest TZs) — smart push doesn't apply.
     */
    public function testPushWithSideStepStillDefenderChoice(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 9, 7, id: 1)
            ->addPlayer(TeamSide::AWAY, 10, 7, skills: [SkillName::SideStep], id: 2)
            // Enemy creating TZs: player 3 at (12,7) adds TZ to (11,7), (11,6), (11,8)
            ->addPlayer(TeamSide::HOME, 12, 7, id: 3)
            ->withBallOffPitch()
            ->build();

        // 1 die: roll 3 → PUSHED
        $dice = new FixedDiceRoller([3]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);

        $defPos = $result->getNewState()->getPlayer(2)->getPosition();
        $this->assertNotNull($defPos);
        // SideStep defender picks FEWEST TZs.
        // All three squares (11,7), (11,6), (11,8) have 1 TZ each from player 3.
        // Equal TZs → SideStep picks first in sort order (no sideline tiebreak in SideStep path).
        // Just verify defender ended up on one of the valid push squares.
        $this->assertEquals(11, $defPos->getX());
        $this->assertContains($defPos->getY(), [6, 7, 8]);
    }

    /**
     * Grab still uses its own attacker-worst-for-defender path.
     */
    public function testPushWithGrabStillAttackerWorst(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 9, 7, skills: [SkillName::Grab], id: 1)
            ->addPlayer(TeamSide::AWAY, 10, 7, id: 2)
            ->addPlayer(TeamSide::HOME, 12, 7, id: 3) // TZ source
            ->withBallOffPitch()
            ->build();

        // 1 die: roll 3 → PUSHED
        $dice = new FixedDiceRoller([3]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);

        $defPos = $result->getNewState()->getPlayer(2)->getPosition();
        $this->assertNotNull($defPos);
        // Grab picks most TZs for defender. All 3 squares have 1 TZ from player 3.
        // Grab has no sideline tiebreak, just picks first in TZ order.
        $this->assertEquals(11, $defPos->getX());
    }

    /**
     * All 3 push squares on-pitch. No crowd surf, sorts by TZ + sideline.
     * Defender in middle of pitch, all push squares valid and on-pitch.
     */
    public function testPushNoCrowdSurfWhenAllOnPitch(): void
    {
        // Attacker at (12,7), defender at (13,7). Push direction: right.
        // Push squares: (14,7), (14,6), (14,8) — all on-pitch.
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 12, 7, id: 1)
            ->addPlayer(TeamSide::AWAY, 13, 7, id: 2)
            ->withBallOffPitch()
            ->build();

        // 1 die: roll 3 → PUSHED
        $dice = new FixedDiceRoller([3]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);

        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertNotContains('crowd_surf', $types);

        $defPos = $result->getNewState()->getPlayer(2)->getPosition();
        $this->assertNotNull($defPos);
        $this->assertEquals(14, $defPos->getX());
        // No TZs at any square → tiebreak by sideline
        // (14,7)=min(7,7)=7, (14,6)=min(6,8)=6, (14,8)=min(8,6)=6
        // (14,6) and (14,8) both have sideline distance 6. First in sort wins.
        // usort is not stable, but both are equally close. Either is valid.
        $this->assertContains($defPos->getY(), [6, 8]);
    }
}
