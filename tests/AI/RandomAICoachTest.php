<?php
declare(strict_types=1);

namespace App\Tests\AI;

use App\AI\RandomAICoach;
use App\Engine\RulesEngine;
use App\Enum\ActionType;
use App\Enum\GamePhase;
use App\Enum\TeamSide;
use App\Tests\Engine\GameStateBuilder;
use PHPUnit\Framework\TestCase;

final class RandomAICoachTest extends TestCase
{
    private RandomAICoach $ai;
    private RulesEngine $rules;

    protected function setUp(): void
    {
        $this->ai = new RandomAICoach();
        $this->rules = new RulesEngine();
    }

    public function testDecideActionReturnsValidAction(): void
    {
        $builder = new GameStateBuilder();
        $builder->addPlayer(TeamSide::HOME, 5, 5, id: 1);
        $builder->addPlayer(TeamSide::AWAY, 20, 10, id: 2);
        $state = $builder->build();

        $decision = $this->ai->decideAction($state, $this->rules);

        $this->assertInstanceOf(ActionType::class, $decision['action']);
    }

    public function testDecideActionReturnsEndTurnWhenNoActions(): void
    {
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

    public function testDecideActionCanChooseMove(): void
    {
        $builder = new GameStateBuilder();
        $builder->addPlayer(TeamSide::HOME, 5, 5, movement: 6, id: 1);
        $builder->addPlayer(TeamSide::AWAY, 20, 10, id: 2);
        $state = $builder->build();

        $gotMove = false;
        for ($i = 0; $i < 50; $i++) {
            $decision = $this->ai->decideAction($state, $this->rules);
            if ($decision['action'] === ActionType::MOVE) {
                $gotMove = true;
                $this->assertArrayHasKey('playerId', $decision['params']);
                $this->assertArrayHasKey('x', $decision['params']);
                $this->assertArrayHasKey('y', $decision['params']);
                break;
            }
        }
        $this->assertTrue($gotMove, 'RandomAI should sometimes choose MOVE');
    }

    public function testDecideActionCanChooseBlock(): void
    {
        $builder = new GameStateBuilder();
        $builder->addPlayer(TeamSide::HOME, 5, 5, id: 1);
        $builder->addPlayer(TeamSide::AWAY, 6, 5, id: 2);
        $state = $builder->build();

        $gotBlock = false;
        for ($i = 0; $i < 50; $i++) {
            $decision = $this->ai->decideAction($state, $this->rules);
            if ($decision['action'] === ActionType::BLOCK) {
                $gotBlock = true;
                $this->assertSame(1, $decision['params']['playerId']);
                $this->assertSame(2, $decision['params']['targetId']);
                break;
            }
        }
        $this->assertTrue($gotBlock, 'RandomAI should sometimes choose BLOCK');
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

        $onPitch = $newState->getPlayersOnPitch(TeamSide::AWAY);
        $this->assertCount(11, $onPitch);
    }

    public function testSetupFormationPlaces3OnLoS(): void
    {
        $builder = new GameStateBuilder();
        $builder->withPhase(GamePhase::SETUP);

        for ($i = 0; $i < 11; $i++) {
            $builder->addOffPitchPlayer(TeamSide::HOME, id: 100 + $i);
        }

        $state = $builder->build();
        $newState = $this->ai->setupFormation($state, TeamSide::HOME);

        $losCount = 0;
        foreach ($newState->getPlayersOnPitch(TeamSide::HOME) as $player) {
            $pos = $player->getPosition();
            $this->assertNotNull($pos);
            if ($pos->getX() === 12) {
                $losCount++;
            }
        }
        $this->assertSame(3, $losCount);
    }

    public function testSetupFormationRespectsWideZones(): void
    {
        $builder = new GameStateBuilder();
        $builder->withPhase(GamePhase::SETUP);

        for ($i = 0; $i < 11; $i++) {
            $builder->addOffPitchPlayer(TeamSide::AWAY, id: 100 + $i);
        }

        $state = $builder->build();
        $newState = $this->ai->setupFormation($state, TeamSide::AWAY);

        $topWide = 0;
        $bottomWide = 0;
        foreach ($newState->getPlayersOnPitch(TeamSide::AWAY) as $player) {
            $pos = $player->getPosition();
            $this->assertNotNull($pos);
            $y = $pos->getY();
            if ($y < 4) {
                $topWide++;
            }
            if ($y >= 11) {
                $bottomWide++;
            }
        }
        $this->assertLessThanOrEqual(2, $topWide);
        $this->assertLessThanOrEqual(2, $bottomWide);
    }

    public function testSetupFormationPlayersOnCorrectSide(): void
    {
        $builder = new GameStateBuilder();
        $builder->withPhase(GamePhase::SETUP);

        for ($i = 0; $i < 11; $i++) {
            $builder->addOffPitchPlayer(TeamSide::HOME, id: 100 + $i);
        }

        $state = $builder->build();
        $newState = $this->ai->setupFormation($state, TeamSide::HOME);

        foreach ($newState->getPlayersOnPitch(TeamSide::HOME) as $player) {
            $pos = $player->getPosition();
            $this->assertNotNull($pos);
            $this->assertLessThanOrEqual(12, $pos->getX(), 'Home players must be on left half');
        }
    }
}
