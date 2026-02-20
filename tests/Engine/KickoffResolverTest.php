<?php

declare(strict_types=1);

namespace App\Tests\Engine;

use App\Engine\BallResolver;
use App\Engine\FixedDiceRoller;
use App\Engine\KickoffResolver;
use App\Engine\ScatterCalculator;
use App\Engine\TacklezoneCalculator;
use App\Enum\KickoffEvent;
use App\Enum\PlayerState;
use App\Enum\TeamSide;
use App\ValueObject\Position;
use PHPUnit\Framework\TestCase;

final class KickoffResolverTest extends TestCase
{
    private function createResolver(FixedDiceRoller $dice): KickoffResolver
    {
        $scatterCalc = new ScatterCalculator();
        $tzCalc = new TacklezoneCalculator();
        $ballResolver = new BallResolver($dice, $tzCalc, $scatterCalc);
        return new KickoffResolver($dice, $scatterCalc, $ballResolver);
    }

    // --- Existing scatter/touchback/catch tests (updated with kickoff table dice) ---

    public function testKickoffLandsOnPlayerAndCaught(): void
    {
        // Home receives, kick target at (6,7)
        // Scatter: D8=1 (North), D6=2 -> (6,5)
        // Kickoff table: 4+4=8 (Changing Weather, no-op)
        // Player at (6,5) catches: roll 4, AG3 target=4+
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 6, 5, agility: 3, id: 1)
            ->addPlayer(TeamSide::AWAY, 15, 7, id: 2)
            ->build();

        $dice = new FixedDiceRoller([1, 2, 4, 4, 3, 3, 4]); // D8, D6(scatter), D6(kt1), D6(kt2), D6(weather1), D6(weather2), catch roll
        $resolver = $this->createResolver($dice);

        $result = $resolver->resolveKickoff($state, new Position(6, 7));

        $this->assertTrue($result['state']->getBall()->isHeld());
        $this->assertEquals(1, $result['state']->getBall()->getCarrierId());

        // Should have kickoff_table event
        $types = array_map(fn($e) => $e->getType(), $result['events']);
        $this->assertContains('kickoff_table', $types);
    }

    public function testKickoffLandsEmptySquareBounces(): void
    {
        // Scatter: D8=3 (East), D6=1 -> (7,7) - no player
        // Kickoff table: 4+4=8 (Changing Weather)
        // Bounce: D8=5 (South) -> (7,8)
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, id: 1)
            ->addPlayer(TeamSide::AWAY, 15, 7, id: 2)
            ->build();

        $dice = new FixedDiceRoller([3, 1, 4, 4, 3, 3, 5]); // D8, D6(scatter), D6(kt1), D6(kt2), D6(weather1), D6(weather2), bounce D8
        $resolver = $this->createResolver($dice);

        $result = $resolver->resolveKickoff($state, new Position(6, 7));

        $this->assertFalse($result['state']->getBall()->isHeld());
        $ballPos = $result['state']->getBall()->getPosition();
        $this->assertNotNull($ballPos);
        $this->assertEquals(7, $ballPos->getX());
        $this->assertEquals(8, $ballPos->getY());
    }

    public function testKickoffScattersOutOfReceivingHalfTouchback(): void
    {
        // Scatter: D8=3 (East), D6=5 -> (15,7) not in home half -> touchback
        // Kickoff table: 4+4=8 (Changing Weather)
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 6, 5, agility: 3, id: 1)
            ->addPlayer(TeamSide::AWAY, 15, 7, id: 2)
            ->build();

        $dice = new FixedDiceRoller([3, 5, 4, 4, 3, 3]); // D8, D6(scatter), D6(kt1), D6(kt2), D6(weather1), D6(weather2)
        $resolver = $this->createResolver($dice);

        $result = $resolver->resolveKickoff($state, new Position(10, 7));

        $this->assertTrue($result['state']->getBall()->isHeld());
        $this->assertEquals(1, $result['state']->getBall()->getCarrierId());
    }

    public function testTouchbackGivesToClosestPlayer(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 3, 7, id: 1) // closer to center
            ->addPlayer(TeamSide::HOME, 10, 2, id: 2) // farther
            ->addPlayer(TeamSide::AWAY, 15, 7, id: 3)
            ->build();

        $dice = new FixedDiceRoller([]);
        $resolver = $this->createResolver($dice);

        $result = $resolver->resolveTouchback($state, TeamSide::HOME);

        $this->assertTrue($result['state']->getBall()->isHeld());
        $this->assertEquals(1, $result['state']->getBall()->getCarrierId());
    }

    public function testGetDefaultKickTarget(): void
    {
        $dice = new FixedDiceRoller([]);
        $resolver = $this->createResolver($dice);

        $homeReceives = $resolver->getDefaultKickTarget(TeamSide::HOME);
        $this->assertEquals(6, $homeReceives->getX());
        $this->assertEquals(7, $homeReceives->getY());

        $awayReceives = $resolver->getDefaultKickTarget(TeamSide::AWAY);
        $this->assertEquals(19, $awayReceives->getX());
        $this->assertEquals(7, $awayReceives->getY());
    }

    public function testKickoffScattersOffPitchTouchback(): void
    {
        // Scatter: D8=7 (West), D6=5 -> off pitch
        // Kickoff table: 4+4=8 (Changing Weather)
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 6, 5, agility: 3, id: 1)
            ->addPlayer(TeamSide::AWAY, 15, 7, id: 2)
            ->build();

        $dice = new FixedDiceRoller([7, 5, 4, 4, 3, 3]); // D8, D6(scatter), D6(kt1), D6(kt2), D6(weather1), D6(weather2)
        $resolver = $this->createResolver($dice);

        $result = $resolver->resolveKickoff($state, new Position(2, 7));

        $this->assertTrue($result['state']->getBall()->isHeld());
        $this->assertEquals(1, $result['state']->getBall()->getCarrierId());
    }

    // --- Kickoff Table Event Tests ---

    public function testKickoffTableGetTheRef(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 6, 5, id: 1)
            ->addPlayer(TeamSide::AWAY, 15, 7, id: 2)
            ->build();

        // 1+1=2 = Get the Ref!
        $dice = new FixedDiceRoller([1, 1]);
        $resolver = $this->createResolver($dice);

        $result = $resolver->resolveKickoffTable($state, TeamSide::HOME);

        $this->assertCount(1, $result['events']);
        $this->assertEquals('kickoff_table', $result['events'][0]->getType());
        $this->assertStringContains('Get the Ref', $result['events'][0]->getDescription());
    }

    public function testKickoffTableRiotFirstTurnLosesTurn(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 6, 5, id: 1)
            ->addPlayer(TeamSide::AWAY, 15, 7, id: 2)
            ->build();

        // 1+2=3 = Riot
        $dice = new FixedDiceRoller([1, 2]);
        $resolver = $this->createResolver($dice);

        $result = $resolver->resolveKickoffTable($state, TeamSide::HOME);

        // Turn 1 → Turn 2 (lost a turn)
        $this->assertEquals(2, $result['state']->getHomeTeam()->getTurnNumber());
        $this->assertStringContains('loses a turn', $result['events'][0]->getDescription());
    }

    public function testKickoffTableRiotLaterTurnGainsTurn(): void
    {
        $homeTeam = \App\DTO\TeamStateDTO::create(1, 'Home', 'Human', TeamSide::HOME, 3)
            ->withTurnNumber(3);
        $state = (new GameStateBuilder())
            ->withHomeTeam($homeTeam)
            ->addPlayer(TeamSide::HOME, 6, 5, id: 1)
            ->addPlayer(TeamSide::AWAY, 15, 7, id: 2)
            ->build();

        // 1+2=3 = Riot
        $dice = new FixedDiceRoller([1, 2]);
        $resolver = $this->createResolver($dice);

        $result = $resolver->resolveKickoffTable($state, TeamSide::HOME);

        // Turn 3 → Turn 2 (gained a turn)
        $this->assertEquals(2, $result['state']->getHomeTeam()->getTurnNumber());
        $this->assertStringContains('gains a turn', $result['events'][0]->getDescription());
    }

    public function testKickoffTablePerfectDefence(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 6, 5, id: 1)
            ->addPlayer(TeamSide::AWAY, 15, 7, id: 2)
            ->build();

        // 1+3=4 = Perfect Defence
        $dice = new FixedDiceRoller([1, 3]);
        $resolver = $this->createResolver($dice);

        $result = $resolver->resolveKickoffTable($state, TeamSide::HOME);

        $this->assertStringContains('Perfect Defence', $result['events'][0]->getDescription());
    }

    public function testKickoffTableHighKickMovesPlayerUnderBall(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 6, 5, id: 1) // closest to ball
            ->addPlayer(TeamSide::HOME, 3, 7, id: 2) // farther
            ->addPlayer(TeamSide::AWAY, 15, 7, id: 3)
            ->withBallOnGround(8, 7)
            ->build();

        // 1+4=5 = High Kick
        $dice = new FixedDiceRoller([1, 4]);
        $resolver = $this->createResolver($dice);

        $result = $resolver->resolveKickoffTable($state, TeamSide::HOME);

        // Player 1 should be moved to ball position (8,7)
        $player1 = $result['state']->getPlayer(1);
        $this->assertNotNull($player1);
        $pos = $player1->getPosition();
        $this->assertNotNull($pos);
        $this->assertEquals(8, $pos->getX());
        $this->assertEquals(7, $pos->getY());
    }

    public function testKickoffTableCheeringHomeWins(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 6, 5, id: 1)
            ->addPlayer(TeamSide::AWAY, 15, 7, id: 2)
            ->build();

        $initialRerolls = $state->getHomeTeam()->getRerolls();

        // 2+4=6 = Cheering, then home=5, away=2
        $dice = new FixedDiceRoller([2, 4, 5, 2]);
        $resolver = $this->createResolver($dice);

        $result = $resolver->resolveKickoffTable($state, TeamSide::HOME);

        $this->assertEquals($initialRerolls + 1, $result['state']->getHomeTeam()->getRerolls());
    }

    public function testKickoffTableCheeringTieNoEffect(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 6, 5, id: 1)
            ->addPlayer(TeamSide::AWAY, 15, 7, id: 2)
            ->build();

        $initialHomeRerolls = $state->getHomeTeam()->getRerolls();
        $initialAwayRerolls = $state->getAwayTeam()->getRerolls();

        // 2+4=6 = Cheering, then home=3, away=3
        $dice = new FixedDiceRoller([2, 4, 3, 3]);
        $resolver = $this->createResolver($dice);

        $result = $resolver->resolveKickoffTable($state, TeamSide::HOME);

        $this->assertEquals($initialHomeRerolls, $result['state']->getHomeTeam()->getRerolls());
        $this->assertEquals($initialAwayRerolls, $result['state']->getAwayTeam()->getRerolls());
    }

    public function testKickoffTableBrilliantCoachingAwayWins(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 6, 5, id: 1)
            ->addPlayer(TeamSide::AWAY, 15, 7, id: 2)
            ->build();

        $initialRerolls = $state->getAwayTeam()->getRerolls();

        // 3+4=7 = Brilliant Coaching, then home=2, away=5
        $dice = new FixedDiceRoller([3, 4, 2, 5]);
        $resolver = $this->createResolver($dice);

        $result = $resolver->resolveKickoffTable($state, TeamSide::HOME);

        $this->assertEquals($initialRerolls + 1, $result['state']->getAwayTeam()->getRerolls());
    }

    public function testKickoffTableChangingWeather(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 6, 5, id: 1)
            ->addPlayer(TeamSide::AWAY, 15, 7, id: 2)
            ->build();

        // 4+4=8 = Changing Weather, then weather roll 6+6=12 = Blizzard
        $dice = new FixedDiceRoller([4, 4, 6, 6]);
        $resolver = $this->createResolver($dice);

        $result = $resolver->resolveKickoffTable($state, TeamSide::HOME);

        $this->assertStringContains('Changing Weather', $result['events'][0]->getDescription());
        $this->assertEquals(\App\Enum\Weather::BLIZZARD, $result['state']->getWeather());
    }

    public function testKickoffTableQuickSnapMovesReceivingTeam(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 6, 5, id: 1) // should move to (7,5)
            ->addPlayer(TeamSide::HOME, 10, 7, id: 2) // should move to (11,7)
            ->addPlayer(TeamSide::AWAY, 15, 7, id: 3) // away team, not moved
            ->build();

        // 4+5=9 = Quick Snap (receiving=HOME moves +x)
        $dice = new FixedDiceRoller([4, 5]);
        $resolver = $this->createResolver($dice);

        $result = $resolver->resolveKickoffTable($state, TeamSide::HOME);

        $p1 = $result['state']->getPlayer(1);
        $this->assertNotNull($p1);
        $this->assertEquals(7, $p1->getPosition()->getX());

        $p2 = $result['state']->getPlayer(2);
        $this->assertNotNull($p2);
        $this->assertEquals(11, $p2->getPosition()->getX());

        // Away player should not move
        $p3 = $result['state']->getPlayer(3);
        $this->assertNotNull($p3);
        $this->assertEquals(15, $p3->getPosition()->getX());
    }

    public function testKickoffTableQuickSnapDoesNotMoveIntoOccupied(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 6, 5, id: 1) // should move to (7,5)
            ->addPlayer(TeamSide::HOME, 7, 5, id: 2) // blocks player 1 from moving, but moves to (8,5)
            ->addPlayer(TeamSide::AWAY, 15, 7, id: 3)
            ->build();

        // 4+5=9 = Quick Snap
        $dice = new FixedDiceRoller([4, 5]);
        $resolver = $this->createResolver($dice);

        $result = $resolver->resolveKickoffTable($state, TeamSide::HOME);

        // Player 2 moves first (getPlayersOnPitch order), player 1 can then move
        // But since we iterate the snapshot, player 1 sees (7,5) as occupied
        $p1 = $result['state']->getPlayer(1);
        $this->assertNotNull($p1);
        // Player 1 couldn't move because (7,5) was occupied in the snapshot
        $this->assertEquals(6, $p1->getPosition()->getX());
    }

    public function testKickoffTableBlitzMovesKickingTeam(): void
    {
        // Home kicks, away receives. Home is kicking → Home moves forward (+x)
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 12, 7, id: 1) // kicking team
            ->addPlayer(TeamSide::AWAY, 15, 7, id: 2) // receiving team
            ->build();

        // 5+5=10 = Blitz (kicking=AWAY)
        $dice = new FixedDiceRoller([5, 5]);
        $resolver = $this->createResolver($dice);

        // Home receives, so away is kicking
        $result = $resolver->resolveKickoffTable($state, TeamSide::HOME);

        // Away (kicking) moves -x (toward home half)
        $p2 = $result['state']->getPlayer(2);
        $this->assertNotNull($p2);
        $this->assertEquals(14, $p2->getPosition()->getX());

        // Home (receiving) doesn't move
        $p1 = $result['state']->getPlayer(1);
        $this->assertNotNull($p1);
        $this->assertEquals(12, $p1->getPosition()->getX());
    }

    public function testKickoffTableThrowARockStunsPlayers(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 6, 5, id: 1)
            ->addPlayer(TeamSide::HOME, 6, 7, id: 2)
            ->addPlayer(TeamSide::AWAY, 15, 5, id: 3)
            ->addPlayer(TeamSide::AWAY, 15, 7, id: 4)
            ->build();

        // 5+6=11 = Throw a Rock
        // Home targets: D6=1 -> index 0 -> player 1
        // Away targets: D6=2 -> index 1 -> player 4
        $dice = new FixedDiceRoller([5, 6, 1, 2]);
        $resolver = $this->createResolver($dice);

        $result = $resolver->resolveKickoffTable($state, TeamSide::HOME);

        $p1 = $result['state']->getPlayer(1);
        $this->assertEquals(PlayerState::STUNNED, $p1->getState());

        $p4 = $result['state']->getPlayer(4);
        $this->assertEquals(PlayerState::STUNNED, $p4->getState());

        // Other players unaffected
        $p2 = $result['state']->getPlayer(2);
        $this->assertEquals(PlayerState::STANDING, $p2->getState());
    }

    public function testKickoffTablePitchInvasionStunsOnSix(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 6, 5, id: 1)
            ->addPlayer(TeamSide::HOME, 6, 7, id: 2)
            ->addPlayer(TeamSide::AWAY, 15, 5, id: 3)
            ->addPlayer(TeamSide::AWAY, 15, 7, id: 4)
            ->build();

        // 6+6=12 = Pitch Invasion
        // Home players: D6=6 (stunned), D6=3 (safe)
        // Away players: D6=1 (safe), D6=6 (stunned)
        $dice = new FixedDiceRoller([6, 6, 6, 3, 1, 6]);
        $resolver = $this->createResolver($dice);

        $result = $resolver->resolveKickoffTable($state, TeamSide::HOME);

        $p1 = $result['state']->getPlayer(1);
        $this->assertEquals(PlayerState::STUNNED, $p1->getState());

        $p2 = $result['state']->getPlayer(2);
        $this->assertEquals(PlayerState::STANDING, $p2->getState());

        $p3 = $result['state']->getPlayer(3);
        $this->assertEquals(PlayerState::STANDING, $p3->getState());

        $p4 = $result['state']->getPlayer(4);
        $this->assertEquals(PlayerState::STUNNED, $p4->getState());
    }

    public function testKickoffTablePitchInvasionNobodyStunned(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 6, 5, id: 1)
            ->addPlayer(TeamSide::AWAY, 15, 5, id: 2)
            ->build();

        // 6+6=12 = Pitch Invasion
        // All rolls < 6
        $dice = new FixedDiceRoller([6, 6, 3, 4]);
        $resolver = $this->createResolver($dice);

        $result = $resolver->resolveKickoffTable($state, TeamSide::HOME);

        $this->assertEquals(PlayerState::STANDING, $result['state']->getPlayer(1)->getState());
        $this->assertEquals(PlayerState::STANDING, $result['state']->getPlayer(2)->getState());
        $this->assertStringContains('no one is hurt', $result['events'][0]->getDescription());
    }

    // --- KickoffEvent Enum Tests ---

    public function testKickoffEventEnumValues(): void
    {
        $this->assertEquals(2, KickoffEvent::GetTheRef->value);
        $this->assertEquals(3, KickoffEvent::Riot->value);
        $this->assertEquals(12, KickoffEvent::PitchInvasion->value);
    }

    public function testKickoffEventEnumLabels(): void
    {
        $this->assertEquals('Get the Ref!', KickoffEvent::GetTheRef->label());
        $this->assertEquals('Riot!', KickoffEvent::Riot->label());
        $this->assertEquals('Pitch Invasion!', KickoffEvent::PitchInvasion->label());
    }

    public function testKickoffEventFromValue(): void
    {
        $this->assertEquals(KickoffEvent::ChangingWeather, KickoffEvent::from(8));
        $this->assertEquals(KickoffEvent::Blitz, KickoffEvent::from(10));
    }

    private function assertStringContains(string $needle, string $haystack): void
    {
        $this->assertTrue(
            str_contains($haystack, $needle),
            "Failed asserting that '{$haystack}' contains '{$needle}'",
        );
    }
}
