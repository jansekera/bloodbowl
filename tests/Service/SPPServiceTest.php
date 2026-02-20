<?php

declare(strict_types=1);

namespace App\Tests\Service;

use App\DTO\GameEvent;
use App\DTO\MatchStatsDTO;
use App\Service\SPPService;
use PHPUnit\Framework\TestCase;

final class SPPServiceTest extends TestCase
{
    private SPPService $spp;

    protected function setUp(): void
    {
        $this->spp = new SPPService();
    }

    // --- MatchStatsDTO tests ---

    public function testMatchStatsDTODefaultsToZero(): void
    {
        $stats = new MatchStatsDTO(1);
        $this->assertEquals(1, $stats->getPlayerId());
        $this->assertEquals(0, $stats->getTouchdowns());
        $this->assertEquals(0, $stats->getCompletions());
        $this->assertEquals(0, $stats->getInterceptions());
        $this->assertEquals(0, $stats->getCasualties());
        $this->assertFalse($stats->isMvp());
        $this->assertEquals(0, $stats->getSpp());
    }

    public function testTouchdownWorth3Spp(): void
    {
        $stats = (new MatchStatsDTO(1))->withTouchdown();
        $this->assertEquals(1, $stats->getTouchdowns());
        $this->assertEquals(3, $stats->getSpp());
    }

    public function testCompletionWorth1Spp(): void
    {
        $stats = (new MatchStatsDTO(1))->withCompletion();
        $this->assertEquals(1, $stats->getCompletions());
        $this->assertEquals(1, $stats->getSpp());
    }

    public function testInterceptionWorth2Spp(): void
    {
        $stats = (new MatchStatsDTO(1))->withInterception();
        $this->assertEquals(1, $stats->getInterceptions());
        $this->assertEquals(2, $stats->getSpp());
    }

    public function testCasualtyWorth2Spp(): void
    {
        $stats = (new MatchStatsDTO(1))->withCasualty();
        $this->assertEquals(1, $stats->getCasualties());
        $this->assertEquals(2, $stats->getSpp());
    }

    public function testMvpWorth5Spp(): void
    {
        $stats = (new MatchStatsDTO(1))->withMvp();
        $this->assertTrue($stats->isMvp());
        $this->assertEquals(5, $stats->getSpp());
    }

    public function testCombinedSpp(): void
    {
        $stats = (new MatchStatsDTO(1))
            ->withTouchdown()      // 3
            ->withTouchdown()      // 3
            ->withCompletion()     // 1
            ->withInterception()   // 2
            ->withCasualty()       // 2
            ->withMvp();           // 5
        // 3+3+1+2+2+5 = 16
        $this->assertEquals(16, $stats->getSpp());
        $this->assertEquals(2, $stats->getTouchdowns());
    }

    public function testMatchStatsDTOToArray(): void
    {
        $stats = (new MatchStatsDTO(42))
            ->withTouchdown()
            ->withMvp();

        $arr = $stats->toArray();
        $this->assertEquals(42, $arr['playerId']);
        $this->assertEquals(1, $arr['touchdowns']);
        $this->assertEquals(0, $arr['completions']);
        $this->assertTrue($arr['mvp']);
        $this->assertEquals(8, $arr['spp']); // 3 + 5
    }

    public function testMatchStatsDTOImmutable(): void
    {
        $original = new MatchStatsDTO(1);
        $withTd = $original->withTouchdown();

        $this->assertEquals(0, $original->getTouchdowns());
        $this->assertEquals(1, $withTd->getTouchdowns());
        $this->assertNotSame($original, $withTd);
    }

    // --- Level calculation tests ---

    public function testLevel1AtZeroSpp(): void
    {
        $this->assertEquals(1, $this->spp->getLevel(0));
        $this->assertEquals(1, $this->spp->getLevel(5));
    }

    public function testLevel2At6Spp(): void
    {
        $this->assertEquals(2, $this->spp->getLevel(6));
        $this->assertEquals(2, $this->spp->getLevel(15));
    }

    public function testLevel3At16Spp(): void
    {
        $this->assertEquals(3, $this->spp->getLevel(16));
        $this->assertEquals(3, $this->spp->getLevel(30));
    }

    public function testLevel4At31Spp(): void
    {
        $this->assertEquals(4, $this->spp->getLevel(31));
    }

    public function testLevel5At51Spp(): void
    {
        $this->assertEquals(5, $this->spp->getLevel(51));
    }

    public function testLevel6At76Spp(): void
    {
        $this->assertEquals(6, $this->spp->getLevel(76));
    }

    public function testLevel7At176Spp(): void
    {
        $this->assertEquals(7, $this->spp->getLevel(176));
    }

    public function testGetSppForNextLevel(): void
    {
        $this->assertEquals(6, $this->spp->getSppForNextLevel(0));
        $this->assertEquals(6, $this->spp->getSppForNextLevel(5));
        $this->assertEquals(16, $this->spp->getSppForNextLevel(6));
        $this->assertEquals(16, $this->spp->getSppForNextLevel(10));
        $this->assertEquals(31, $this->spp->getSppForNextLevel(16));
        $this->assertNull($this->spp->getSppForNextLevel(176)); // max level
    }

    public function testHasLeveledUp(): void
    {
        $this->assertTrue($this->spp->hasLeveledUp(5, 6));   // 1→2
        $this->assertTrue($this->spp->hasLeveledUp(0, 16));   // 1→3
        $this->assertFalse($this->spp->hasLeveledUp(6, 10));  // 2→2
        $this->assertFalse($this->spp->hasLeveledUp(0, 0));   // 1→1
    }

    // --- Event collection tests ---

    public function testCollectStatsTouchdown(): void
    {
        $events = [
            GameEvent::touchdown(10, 'Home'),
        ];

        $stats = $this->spp->collectStats($events);
        $this->assertArrayHasKey(10, $stats);
        $this->assertEquals(1, $stats[10]->getTouchdowns());
        $this->assertEquals(3, $stats[10]->getSpp());
    }

    public function testCollectStatsCompletion(): void
    {
        $events = [
            GameEvent::passAttempt(10, '5,7', '5,4', 'short', 3, 4, 'accurate'),
        ];

        $stats = $this->spp->collectStats($events);
        $this->assertArrayHasKey(10, $stats);
        $this->assertEquals(1, $stats[10]->getCompletions());
    }

    public function testCollectStatsFumbleNotCounted(): void
    {
        $events = [
            GameEvent::passAttempt(10, '5,7', '5,4', 'short', 3, 1, 'fumble'),
        ];

        $stats = $this->spp->collectStats($events);
        $this->assertEmpty($stats);
    }

    public function testCollectStatsInterception(): void
    {
        $events = [
            GameEvent::interception(20, 4, 5, true),
        ];

        $stats = $this->spp->collectStats($events);
        $this->assertArrayHasKey(20, $stats);
        $this->assertEquals(1, $stats[20]->getInterceptions());
        $this->assertEquals(2, $stats[20]->getSpp());
    }

    public function testCollectStatsFailedInterceptionNotCounted(): void
    {
        $events = [
            GameEvent::interception(20, 4, 2, false),
        ];

        $stats = $this->spp->collectStats($events);
        $this->assertEmpty($stats);
    }

    public function testCollectStatsMultipleEvents(): void
    {
        $events = [
            GameEvent::passAttempt(10, '5,7', '5,4', 'short', 3, 4, 'accurate'),
            GameEvent::touchdown(10, 'Home'),
            GameEvent::interception(20, 4, 5, true),
            GameEvent::touchdown(10, 'Home'),
        ];

        $stats = $this->spp->collectStats($events);
        $this->assertEquals(2, $stats[10]->getTouchdowns());
        $this->assertEquals(1, $stats[10]->getCompletions());
        $this->assertEquals(7, $stats[10]->getSpp()); // 3+3+1
        $this->assertEquals(2, $stats[20]->getSpp()); // 2
    }

    // --- MVP tests ---

    public function testAwardMvp(): void
    {
        $stats = [];
        $playerIds = [1, 2, 3, 4, 5];

        $stats = $this->spp->awardMvp($playerIds, $stats, 0);
        $this->assertTrue($stats[1]->isMvp());
        $this->assertEquals(5, $stats[1]->getSpp());
    }

    public function testAwardMvpToExistingStats(): void
    {
        $stats = [
            10 => (new MatchStatsDTO(10))->withTouchdown(),
        ];
        $playerIds = [10, 20, 30];

        $stats = $this->spp->awardMvp($playerIds, $stats, 0);
        $this->assertTrue($stats[10]->isMvp());
        $this->assertEquals(8, $stats[10]->getSpp()); // 3 + 5
    }

    public function testAwardMvpEmptyPlayerList(): void
    {
        $stats = $this->spp->awardMvp([], [], 0);
        $this->assertEmpty($stats);
    }

    public function testAwardMvpRandomDistribution(): void
    {
        $playerIds = [10, 20, 30];

        // Index 0 → player 10
        $stats0 = $this->spp->awardMvp($playerIds, [], 0);
        $this->assertTrue($stats0[10]->isMvp());

        // Index 1 → player 20
        $stats1 = $this->spp->awardMvp($playerIds, [], 1);
        $this->assertTrue($stats1[20]->isMvp());

        // Index 2 → player 30
        $stats2 = $this->spp->awardMvp($playerIds, [], 2);
        $this->assertTrue($stats2[30]->isMvp());

        // Index 5 → wraps to player 30 (5 % 3 = 2)
        $stats5 = $this->spp->awardMvp($playerIds, [], 5);
        $this->assertTrue($stats5[30]->isMvp());
    }
}
