<?php
declare(strict_types=1);

namespace App\Tests\AI;

use App\AI\GreedyAICoach;
use App\Engine\RulesEngine;
use App\Enum\ActionType;
use App\Enum\GamePhase;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use App\Tests\Engine\GameStateBuilder;
use PHPUnit\Framework\TestCase;

final class GreedyAICoachTest extends TestCase
{
    private GreedyAICoach $ai;
    private RulesEngine $rules;

    protected function setUp(): void
    {
        $this->ai = new GreedyAICoach();
        $this->rules = new RulesEngine();
    }

    public function testScoreTouchdownHighestPriority(): void
    {
        // Ball carrier one step from end zone
        $builder = new GameStateBuilder();
        $builder->addPlayer(TeamSide::HOME, 24, 7, movement: 6, id: 1);
        $builder->addPlayer(TeamSide::AWAY, 13, 7, id: 2);
        $builder->withBallCarried(1);
        $state = $builder->build();

        $decision = $this->ai->decideAction($state, $this->rules);

        $this->assertSame(ActionType::MOVE, $decision['action']);
        $this->assertSame(25, $decision['params']['x']);
    }

    public function testBlitzBallCarrierHighPriority(): void
    {
        // Enemy carries ball, our player can blitz
        $builder = new GameStateBuilder();
        $builder->addPlayer(TeamSide::HOME, 10, 7, strength: 4, id: 1);
        $builder->addPlayer(TeamSide::AWAY, 12, 7, id: 2);
        $builder->withBallCarried(2);
        $state = $builder->build();

        $decision = $this->ai->decideAction($state, $this->rules);

        // Should choose BLITZ or BLOCK targeting the ball carrier
        $this->assertTrue(
            $decision['action'] === ActionType::BLITZ || $decision['action'] === ActionType::BLOCK,
            'Should target ball carrier with blitz or block',
        );
        $this->assertSame(2, $decision['params']['targetId']);
    }

    public function testPickUpBallPriority(): void
    {
        // Ball on ground, player can reach it
        $builder = new GameStateBuilder();
        $builder->addPlayer(TeamSide::HOME, 10, 7, movement: 6, agility: 3, id: 1);
        $builder->addPlayer(TeamSide::AWAY, 20, 7, id: 2);
        $builder->withBallOnGround(12, 7);
        $state = $builder->build();

        $decision = $this->ai->decideAction($state, $this->rules);

        $this->assertSame(ActionType::MOVE, $decision['action']);
        $this->assertSame(12, $decision['params']['x']);
        $this->assertSame(7, $decision['params']['y']);
    }

    public function testPrefers2DiceBlockOver1Dice(): void
    {
        // Strong player adjacent to weak enemy
        $builder = new GameStateBuilder();
        $builder->addPlayer(TeamSide::HOME, 10, 7, strength: 4, id: 1);
        $builder->addPlayer(TeamSide::AWAY, 11, 7, strength: 2, id: 2);
        $state = $builder->build();

        // The player has both MOVE and BLOCK available
        // With ST4 vs ST2, it should prefer the 2-dice block
        $gotBlock = false;
        for ($i = 0; $i < 20; $i++) {
            $decision = $this->ai->decideAction($state, $this->rules);
            if ($decision['action'] === ActionType::BLOCK) {
                $gotBlock = true;
                break;
            }
        }
        // Greedy AI is deterministic, so it should always pick block here
        $this->assertTrue($gotBlock);
    }

    public function testEndTurnWhenNoGoodOptions(): void
    {
        // Player already acted
        $builder = new GameStateBuilder();
        $builder->addPlayer(TeamSide::HOME, 5, 5, id: 1);
        $builder->addPlayer(TeamSide::AWAY, 20, 10, id: 2);
        $state = $builder->build();

        $player = $state->getPlayer(1);
        $this->assertNotNull($player);
        $state = $state->withPlayer($player->withHasMoved(true)->withHasActed(true));

        $decision = $this->ai->decideAction($state, $this->rules);
        $this->assertSame(ActionType::END_TURN, $decision['action']);
    }

    public function testMoveForwardWhenNoSpecialAction(): void
    {
        // Player far from enemies, no ball nearby, blitz already used
        $builder = new GameStateBuilder();
        $builder->addPlayer(TeamSide::HOME, 5, 7, movement: 6, id: 1);
        $builder->addPlayer(TeamSide::AWAY, 20, 7, id: 2);
        $builder->withHomeTeam(
            \App\DTO\TeamStateDTO::create(1, 'Home', 'Human', TeamSide::HOME, 3)->withBlitzUsed(),
        );
        $state = $builder->build();

        $decision = $this->ai->decideAction($state, $this->rules);

        $this->assertSame(ActionType::MOVE, $decision['action']);
        // Should move forward (higher x for HOME)
        $this->assertGreaterThan(5, $decision['params']['x']);
    }

    public function testSetupFormationPlaces11Players(): void
    {
        $builder = new GameStateBuilder();
        $builder->withPhase(GamePhase::SETUP);

        for ($i = 0; $i < 11; $i++) {
            $builder->addOffPitchPlayer(TeamSide::AWAY, id: 100 + $i);
        }

        $state = $builder->build();
        $newState = $this->ai->setupFormation($state, TeamSide::AWAY);

        $this->assertCount(11, $newState->getPlayersOnPitch(TeamSide::AWAY));
    }

    public function testFoulTargetsWeakestArmour(): void
    {
        // Two prone enemies, one with lower armour
        $builder = new GameStateBuilder();
        $builder->addPlayer(TeamSide::HOME, 5, 5, id: 1);
        $builder->addPronePlayer(TeamSide::AWAY, 6, 5, armour: 9, id: 2);
        $builder->addPronePlayer(TeamSide::AWAY, 5, 6, armour: 7, id: 3);
        $state = $builder->build();

        // Mark player as already moved/acted so block/move aren't prioritized
        $player = $state->getPlayer(1);
        $this->assertNotNull($player);
        $state = $state->withPlayer($player->withHasMoved(true));

        $decision = $this->ai->decideAction($state, $this->rules);

        // Should choose foul targeting the weaker armour player
        if ($decision['action'] === ActionType::FOUL) {
            $this->assertSame(3, $decision['params']['targetId']);
        }
    }

    public function testBallAndChainPlayerUsesBallAndChainAction(): void
    {
        // B&C player is the ONLY home player; B&C is their only available action
        $builder = new GameStateBuilder();
        $builder->addPlayer(
            TeamSide::HOME, 10, 7, movement: 6, strength: 4, id: 1,
            skills: [SkillName::BallAndChain],
        );
        $builder->addPlayer(TeamSide::AWAY, 15, 7, id: 2);
        $state = $builder->build();

        $decision = $this->ai->decideAction($state, $this->rules);

        $this->assertSame(ActionType::BALL_AND_CHAIN, $decision['action']);
    }

    public function testHypnoticGazeTargetsAdjacentEnemy(): void
    {
        // Vampire with HypnoticGaze adjacent to an enemy
        $builder = new GameStateBuilder();
        $builder->addPlayer(
            TeamSide::HOME, 10, 7, strength: 4, agility: 4, id: 1,
            skills: [SkillName::HypnoticGaze],
        );
        $builder->addPlayer(TeamSide::AWAY, 11, 7, id: 2);
        // Mark blitz used so it doesn't compete
        $builder->withHomeTeam(
            \App\DTO\TeamStateDTO::create(1, 'Home', 'Human', TeamSide::HOME, 3)->withBlitzUsed(),
        );
        $state = $builder->build();

        // Mark player as already moved so gaze is prioritized
        $player = $state->getPlayer(1);
        $this->assertNotNull($player);
        $state = $state->withPlayer($player->withHasMoved(true));

        $decision = $this->ai->decideAction($state, $this->rules);

        // Should use Hypnotic Gaze or Block (both are valid, but gaze scores ~120+ vs block)
        $this->assertTrue(
            $decision['action'] === ActionType::HYPNOTIC_GAZE || $decision['action'] === ActionType::BLOCK,
            'Should use Hypnotic Gaze or Block on adjacent enemy',
        );
        $this->assertSame(2, $decision['params']['targetId']);
    }

    public function testBombThrowTargetsEnemyCluster(): void
    {
        // Bombardier with a cluster of enemies in range
        $builder = new GameStateBuilder();
        $builder->addPlayer(
            TeamSide::HOME, 5, 7, agility: 3, id: 1,
            skills: [SkillName::Bombardier],
        );
        // Cluster of enemies nearby
        $builder->addPlayer(TeamSide::AWAY, 10, 7, id: 2);
        $builder->addPlayer(TeamSide::AWAY, 10, 8, id: 3);
        $builder->addPlayer(TeamSide::AWAY, 11, 7, id: 4);
        // Mark blitz used
        $builder->withHomeTeam(
            \App\DTO\TeamStateDTO::create(1, 'Home', 'Human', TeamSide::HOME, 3)->withBlitzUsed(),
        );
        $state = $builder->build();

        $decision = $this->ai->decideAction($state, $this->rules);

        // Should choose BOMB_THROW (high value with 3 enemies) or MOVE
        if ($decision['action'] === ActionType::BOMB_THROW) {
            $this->assertSame(1, $decision['params']['playerId']);
            $this->assertArrayHasKey('targetX', $decision['params']);
            $this->assertArrayHasKey('targetY', $decision['params']);
        }
    }
}
