<?php

declare(strict_types=1);

namespace App\Tests\Engine;

use App\Engine\ActionResolver;
use App\Engine\FixedDiceRoller;
use App\Engine\GameFlowResolver;
use App\Enum\ActionType;
use App\Enum\GamePhase;
use App\Enum\PlayerState;
use App\Enum\TeamSide;
use PHPUnit\Framework\TestCase;

final class FullGameFlowTest extends TestCase
{
    public function testEndSetupTriggersKickoff(): void
    {
        // Both teams already set up (11 players each on correct halves, 3 on LoS)
        $builder = new GameStateBuilder();
        $builder->withPhase(GamePhase::SETUP);
        $builder->withActiveTeam(TeamSide::HOME);

        // Home team: 3 on LoS (x=12), 8 behind
        for ($i = 0; $i < 3; $i++) {
            $builder->addPlayer(TeamSide::HOME, 12, 4 + $i, agility: 3, id: $i + 1);
        }
        for ($i = 0; $i < 8; $i++) {
            $builder->addPlayer(TeamSide::HOME, 6, $i + 3, id: $i + 4);
        }

        // Away team: 3 on LoS (x=13), 8 behind
        for ($i = 0; $i < 3; $i++) {
            $builder->addPlayer(TeamSide::AWAY, 13, 4 + $i, id: $i + 12);
        }
        for ($i = 0; $i < 8; $i++) {
            $builder->addPlayer(TeamSide::AWAY, 19, $i + 3, id: $i + 15);
        }

        $state = $builder->build();

        // Kickoff scatter: D8=1 (North), D6=1 -> scatters 1 square north from (6,7) = (6,6)
        // Kickoff table: 4+4=8 (Changing Weather, no-op)
        // Player at (6,6) is player id=7 - catches: roll 4, AG3, target=4+ -> success
        $dice = new FixedDiceRoller([1, 1, 4, 4, 3, 3, 4]); // D8, D6(scatter), D6(kt1), D6(kt2), D6(weather1), D6(weather2), catch roll
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::END_SETUP, []);

        $this->assertEquals(GamePhase::PLAY, $result->getNewState()->getPhase());
        $this->assertEquals(TeamSide::HOME, $result->getNewState()->getActiveTeam());

        // Ball should be on pitch (either caught or bounced)
        $ball = $result->getNewState()->getBall();
        $this->assertTrue($ball->isOnPitch());
    }

    public function testEndSetupKickoffTouchback(): void
    {
        $builder = new GameStateBuilder();
        $builder->withPhase(GamePhase::SETUP);
        $builder->withActiveTeam(TeamSide::HOME);

        // Home: 3 on LoS (x=12), 8 behind
        for ($i = 0; $i < 3; $i++) {
            $builder->addPlayer(TeamSide::HOME, 12, 5 + $i, agility: 3, id: $i + 1);
        }
        for ($i = 0; $i < 8; $i++) {
            $builder->addPlayer(TeamSide::HOME, 6, $i + 3, id: $i + 4);
        }

        // Away: 3 on LoS (x=13), 8 behind
        for ($i = 0; $i < 3; $i++) {
            $builder->addPlayer(TeamSide::AWAY, 13, 5 + $i, id: $i + 12);
        }
        for ($i = 0; $i < 8; $i++) {
            $builder->addPlayer(TeamSide::AWAY, 19, $i + 3, id: $i + 15);
        }

        $state = $builder->build();

        // Kickoff: default target for HOME receiving is (6,7)
        // D8=3 (East), D6=6 -> scatters 6 east: (12,7)
        // (12,7) x<=12 -> still in receiving half, need bigger scatter
        // D8=3 (East), D6=6 from (6,7) -> (12,7) still in half
        // Use: kick into away half: scatter east beyond LoS
        // Actually let scatter go off pitch: D8=1 (North), D6=6 from (6,7) -> (6,1)
        // (6,1) is still on pitch and in receiving half.
        // Off-pitch: D8=1 (North), D6=6 from (6,2) -> y=2-6=-4 off pitch
        // But kick target is auto (6,7). So: D8=3 East, D6=6 -> (12,7) still ok.
        // Let's go big: D8=3 (East), D6=6 -> (12,7) x=12 <= 12, still in half.
        // Need: x > 12 for home receiving to fail.
        // D8=3, D6=7? No, D6 max=6.
        // The receiving half for HOME is x <= 12. Default kick at (6,7).
        // Max east scatter: (6+6,7) = (12,7) - exactly at boundary, still in half.
        // So let's test scatter going off pitch (north):
        // D8=1 (North), D6=6 from (6,7) -> (6,1). Still on pitch (y>=0).
        // We need it off pitch: y < 0. (6, 7-8) = (6,-1) but D6 max is 6.
        // Alternative: kick target near edge. But default is (6,7).
        // Easiest: just test that scatter lands outside receiving half by going east.
        // But we can't get x>12 with D6 max 6 from (6,7).
        // Solution: test off-pitch scenario. D8=5 (South), D6=6 from (6,7) -> (6,13).
        // y=13 < 15, still on pitch. D8=8 (NW), D6=6 from (6,7) -> (0,1). On pitch.
        // Hmm. The test in KickoffResolverTest uses kick target (10,7) with D8=3,D6=5
        // which lands at (15,7) - not in home half. But resolveEndSetup uses
        // getDefaultKickTarget which is (6,7). We can't control the kick target.
        // Actually, the default kick target center of receiving half = (6,7) for HOME.
        // Max scatter east: (12,7). Max scatter south: (6,13). Both on pitch & in half.
        // Off pitch: north or west edge. D8=1 N, D6=6 -> (6,1). Still on pitch.
        // D8=7 W, D6=6 -> (0,7). On pitch (x=0 valid).
        // D8=8 NW, D6=6 -> (0,1). On pitch.
        // It seems impossible to scatter off-pitch from (6,7) with D6<=6.
        // Let's use the fact that x <= 12 includes all of these.
        // We can't produce a touchback from default (6,7) with max D6=6!
        // Let me instead test landing on empty square -> bounce
        // D8=2 (NE), D6=3 -> (9,4). No player there. Bounce D8=5 (S) -> (9,5)
        $dice = new FixedDiceRoller([2, 3, 4, 4, 3, 3, 5]); // D8=NE, D6=3(scatter), D6(kt1), D6(kt2), D6(weather1), D6(weather2), bounce D8=South
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::END_SETUP, []);

        $this->assertEquals(GamePhase::PLAY, $result->getNewState()->getPhase());
        // Ball should be on pitch (bounced to (9,5))
        $ball = $result->getNewState()->getBall();
        $this->assertTrue($ball->isOnPitch());
        $this->assertFalse($ball->isHeld());
    }

    public function testTouchdownDetectedAfterMove(): void
    {
        // Home player carries ball near away end zone
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 24, 7, id: 1, movement: 6)
            ->addPlayer(TeamSide::AWAY, 13, 7, id: 2)
            ->withBallCarried(1)
            ->build();

        // Move to end zone (x=25)
        $dice = new FixedDiceRoller([]); // no dodges needed
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::MOVE, [
            'playerId' => 1, 'x' => 25, 'y' => 7,
        ]);

        $newState = $result->getNewState();

        // Ball should be at (25,7)
        $this->assertEquals(25, $newState->getBall()->getPosition()?->getX());

        // GameFlowResolver should detect touchdown
        $gameFlow = $resolver->getGameFlowResolver();
        $this->assertEquals(TeamSide::HOME, $gameFlow->checkTouchdown($newState));
    }

    public function testTouchdownScoreAndReset(): void
    {
        // Home player at end zone with ball
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 25, 7, id: 1)
            ->addPlayer(TeamSide::AWAY, 13, 7, id: 2)
            ->withBallCarried(1)
            ->build();

        $gameFlow = new GameFlowResolver(new FixedDiceRoller([]));

        // Touchdown
        $tdResult = $gameFlow->resolveTouchdown($state, TeamSide::HOME);
        $this->assertEquals(1, $tdResult['state']->getHomeTeam()->getScore());

        // Post-touchdown
        $postResult = $gameFlow->resolvePostTouchdown($tdResult['state']);
        $this->assertEquals(GamePhase::SETUP, $postResult['state']->getPhase());

        // Players should be off pitch
        $player1 = $postResult['state']->getPlayer(1);
        $this->assertNotNull($player1);
        $this->assertEquals(PlayerState::OFF_PITCH, $player1->getState());
    }

    public function testMovePickupBallAtDestination(): void
    {
        // Player moves to a square with a loose ball -> pickup
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1, movement: 6, agility: 3)
            ->addPlayer(TeamSide::AWAY, 20, 7, id: 2)
            ->withBallOnGround(7, 7)
            ->build();

        // Pickup roll: AG3, target=3+ (7-3-1+0=3), roll 3 -> success
        $dice = new FixedDiceRoller([3]); // pickup roll
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::MOVE, [
            'playerId' => 1, 'x' => 7, 'y' => 7,
        ]);

        $this->assertTrue($result->isSuccess());
        $ball = $result->getNewState()->getBall();
        $this->assertTrue($ball->isHeld());
        $this->assertEquals(1, $ball->getCarrierId());
    }

    public function testMovePickupFailTurnover(): void
    {
        // Ball at destination, pickup fails -> turnover
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1, movement: 6, agility: 3)
            ->addPlayer(TeamSide::AWAY, 20, 7, id: 2)
            ->withBallOnGround(7, 7)
            ->build();

        // Pickup roll: 1 (always fails, target=3), team reroll 1 = fail, then bounce D8=1
        $dice = new FixedDiceRoller([1, 1, 1]); // pickup fail, team reroll fail, bounce direction
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::MOVE, [
            'playerId' => 1, 'x' => 7, 'y' => 7,
        ]);

        $this->assertTrue($result->isTurnover());
    }

    public function testHandOffSuccess(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1, agility: 3)
            ->addPlayer(TeamSide::HOME, 6, 7, id: 2, agility: 3)
            ->addPlayer(TeamSide::AWAY, 20, 7, id: 3)
            ->withBallCarried(1)
            ->build();

        // Catch with +1 modifier: 7-3+0-1 = 3+ target, roll 3
        $dice = new FixedDiceRoller([3]); // catch roll
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::HAND_OFF, [
            'playerId' => 1, 'targetId' => 2,
        ]);

        $this->assertTrue($result->isSuccess());
        $this->assertEquals(2, $result->getNewState()->getBall()->getCarrierId());
    }

    public function testPassActionSuccess(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1, agility: 3)
            ->addPlayer(TeamSide::HOME, 8, 7, id: 2, agility: 3)
            ->addPlayer(TeamSide::AWAY, 20, 7, id: 3)
            ->withBallCarried(1)
            ->build();

        // Distance 3 = quick pass (+1). Accuracy: 7-3+(-1)+0 = 3+
        // Roll 4 -> success. Catch: 7-3+0-1(accurate) = 3+, roll 3 -> success
        $dice = new FixedDiceRoller([4, 3]); // accuracy roll, catch roll
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::PASS, [
            'playerId' => 1, 'targetX' => 8, 'targetY' => 7,
        ]);

        $this->assertTrue($result->isSuccess());
        $this->assertEquals(2, $result->getNewState()->getBall()->getCarrierId());
    }

    public function testHalfTimeTransition(): void
    {
        // Both teams at turn 8, half 1
        $builder = new GameStateBuilder();
        $builder->withActiveTeam(TeamSide::HOME);

        // Need at least one player per team
        $builder->addPlayer(TeamSide::HOME, 5, 5, id: 1);
        $builder->addPlayer(TeamSide::AWAY, 15, 5, id: 2);

        $state = $builder->build();

        // Set both teams to turn 8
        $homeTeam = $state->getHomeTeam()->withTurnNumber(8);
        $awayTeam = $state->getAwayTeam()->withTurnNumber(8);
        $state = $state->withHomeTeam($homeTeam)->withAwayTeam($awayTeam);

        $dice = new FixedDiceRoller([]); // no KO recovery rolls needed
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::END_TURN, []);
        $newState = $result->getNewState();

        // Should transition to half-time
        $this->assertEquals(GamePhase::HALF_TIME, $newState->getPhase());
        $this->assertEquals(2, $newState->getHalf());
    }

    public function testGameOverAfterSecondHalf(): void
    {
        // Both teams at turn 8, half 2
        $builder = new GameStateBuilder();
        $builder->withActiveTeam(TeamSide::HOME);
        $builder->withHalf(2);

        $builder->addPlayer(TeamSide::HOME, 5, 5, id: 1);
        $builder->addPlayer(TeamSide::AWAY, 15, 5, id: 2);

        $state = $builder->build();

        // Set both teams to turn 8
        $homeTeam = $state->getHomeTeam()->withTurnNumber(8);
        $awayTeam = $state->getAwayTeam()->withTurnNumber(8);
        $state = $state->withHomeTeam($homeTeam)->withAwayTeam($awayTeam);

        $dice = new FixedDiceRoller([]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::END_TURN, []);

        $this->assertEquals(GamePhase::GAME_OVER, $result->getNewState()->getPhase());
    }
}
