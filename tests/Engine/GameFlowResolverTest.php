<?php

declare(strict_types=1);

namespace App\Tests\Engine;

use App\Engine\FixedDiceRoller;
use App\Engine\GameFlowResolver;
use App\Enum\GamePhase;
use App\Enum\PlayerState;
use App\Enum\TeamSide;
use PHPUnit\Framework\TestCase;

final class GameFlowResolverTest extends TestCase
{
    public function testTouchdownDetectedHomeScoresInAwayEndzone(): void
    {
        // Home player with ball at x=25 (away end zone)
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 25, 7, id: 1)
            ->withBallCarried(1)
            ->build();

        $resolver = new GameFlowResolver(new FixedDiceRoller([]));
        $this->assertEquals(TeamSide::HOME, $resolver->checkTouchdown($state));
    }

    public function testTouchdownDetectedAwayScoresInHomeEndzone(): void
    {
        $state = (new GameStateBuilder())
            ->withActiveTeam(TeamSide::AWAY)
            ->addPlayer(TeamSide::AWAY, 0, 7, id: 1)
            ->withBallCarried(1)
            ->build();

        $resolver = new GameFlowResolver(new FixedDiceRoller([]));
        $this->assertEquals(TeamSide::AWAY, $resolver->checkTouchdown($state));
    }

    public function testNoTouchdownWithoutBall(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 25, 7, id: 1)
            ->withBallOnGround(25, 7) // ball on ground, not carried
            ->build();

        $resolver = new GameFlowResolver(new FixedDiceRoller([]));
        $this->assertNull($resolver->checkTouchdown($state));
    }

    public function testNoTouchdownWhenProne(): void
    {
        $state = (new GameStateBuilder())
            ->addPronePlayer(TeamSide::HOME, 25, 7, id: 1)
            ->withBallCarried(1) // ball carried flag but player is prone
            ->build();

        $resolver = new GameFlowResolver(new FixedDiceRoller([]));
        $this->assertNull($resolver->checkTouchdown($state));
    }

    public function testNoTouchdownInWrongEndzone(): void
    {
        // Home player in home end zone (own end zone)
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 0, 7, id: 1)
            ->withBallCarried(1)
            ->build();

        $resolver = new GameFlowResolver(new FixedDiceRoller([]));
        $this->assertNull($resolver->checkTouchdown($state));
    }

    public function testResolveTouchdownIncrementsScore(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 25, 7, id: 1)
            ->withBallCarried(1)
            ->build();

        $resolver = new GameFlowResolver(new FixedDiceRoller([]));
        $result = $resolver->resolveTouchdown($state, TeamSide::HOME);

        $this->assertEquals(1, $result['state']->getHomeTeam()->getScore());
        $this->assertCount(1, $result['events']);
        $this->assertEquals('touchdown', $result['events'][0]->getType());
    }

    public function testResolvePostTouchdownResetsForSetup(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 25, 7, id: 1)
            ->addPlayer(TeamSide::AWAY, 13, 7, id: 2)
            ->withBallCarried(1)
            ->build();

        $resolver = new GameFlowResolver(new FixedDiceRoller([]));
        $result = $resolver->resolvePostTouchdown($state);

        $this->assertEquals(GamePhase::SETUP, $result['state']->getPhase());
        $this->assertFalse($result['state']->getBall()->isOnPitch());

        // Players should be off pitch
        $player1 = $result['state']->getPlayer(1);
        $this->assertNotNull($player1);
        $this->assertEquals(PlayerState::OFF_PITCH, $player1->getState());
    }

    public function testHalfTimeKoRecovery(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, id: 1)
            ->build();

        // Set a player to KO state
        $player = $state->getPlayer(1);
        $this->assertNotNull($player);
        $state = $state->withPlayer($player->withState(PlayerState::KO)->withPosition(null));

        // Roll 4 = KO recovery success
        $resolver = new GameFlowResolver(new FixedDiceRoller([4]));
        $result = $resolver->resolveHalfTime($state);

        $this->assertEquals(GamePhase::SETUP, $result['state']->getPhase());
        $this->assertEquals(2, $result['state']->getHalf());

        $recovered = $result['state']->getPlayer(1);
        $this->assertNotNull($recovered);
        $this->assertEquals(PlayerState::OFF_PITCH, $recovered->getState());
    }

    public function testHalfTimeKoRecoveryFails(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, id: 1)
            ->build();

        $player = $state->getPlayer(1);
        $this->assertNotNull($player);
        $state = $state->withPlayer($player->withState(PlayerState::KO)->withPosition(null));

        // Roll 3 = KO recovery fail
        $resolver = new GameFlowResolver(new FixedDiceRoller([3]));
        $result = $resolver->resolveHalfTime($state);

        $koPlayer = $result['state']->getPlayer(1);
        $this->assertNotNull($koPlayer);
        $this->assertEquals(PlayerState::KO, $koPlayer->getState());
    }

    public function testHalfTimeResetsTurnCounters(): void
    {
        $state = (new GameStateBuilder())->build();

        $resolver = new GameFlowResolver(new FixedDiceRoller([]));
        $result = $resolver->resolveHalfTime($state);

        $this->assertEquals(1, $result['state']->getHomeTeam()->getTurnNumber());
        $this->assertEquals(1, $result['state']->getAwayTeam()->getTurnNumber());
    }

    public function testHasRemainingTurns(): void
    {
        $state = (new GameStateBuilder())->build();

        $resolver = new GameFlowResolver(new FixedDiceRoller([]));
        $this->assertTrue($resolver->hasRemainingTurns($state));
    }

    public function testResetPlayersForSetup(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, id: 1)
            ->addPlayer(TeamSide::AWAY, 15, 5, id: 2)
            ->build();

        $resolver = new GameFlowResolver(new FixedDiceRoller([]));
        $result = $resolver->resetPlayersForSetup($state);

        $player1 = $result->getPlayer(1);
        $this->assertNotNull($player1);
        $this->assertEquals(PlayerState::OFF_PITCH, $player1->getState());
        $this->assertNull($player1->getPosition());
    }
}
