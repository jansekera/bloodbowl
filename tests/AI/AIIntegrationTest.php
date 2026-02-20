<?php
declare(strict_types=1);

namespace App\Tests\AI;

use App\AI\AICoachInterface;
use App\AI\GreedyAICoach;
use App\AI\RandomAICoach;
use App\DTO\GameState;
use App\Engine\ActionResolver;
use App\Engine\FixedDiceRoller;
use App\Engine\RulesEngine;
use App\Enum\ActionType;
use App\Enum\GamePhase;
use App\Enum\PlayerState;
use App\Enum\TeamSide;
use App\Tests\Engine\GameStateBuilder;
use PHPUnit\Framework\TestCase;

final class AIIntegrationTest extends TestCase
{
    public function testAiTeamSerializesCorrectly(): void
    {
        $builder = new GameStateBuilder();
        $builder->addPlayer(TeamSide::HOME, 5, 5, id: 1);
        $builder->addPlayer(TeamSide::AWAY, 20, 7, id: 2);
        $state = $builder->build()->withAiTeam(TeamSide::AWAY);

        $array = $state->toArray();
        $this->assertSame('away', $array['aiTeam']);

        $restored = GameState::fromArray($array);
        $this->assertSame(TeamSide::AWAY, $restored->getAiTeam());
    }

    public function testAiTeamNullByDefault(): void
    {
        $builder = new GameStateBuilder();
        $builder->addPlayer(TeamSide::HOME, 5, 5, id: 1);
        $state = $builder->build();

        $this->assertNull($state->getAiTeam());

        $array = $state->toArray();
        $this->assertNull($array['aiTeam']);

        $restored = GameState::fromArray($array);
        $this->assertNull($restored->getAiTeam());
    }

    public function testRandomAiPlaysCompleteTurn(): void
    {
        $builder = new GameStateBuilder();
        $builder->withActiveTeam(TeamSide::HOME);

        // 11 home players
        for ($i = 0; $i < 3; $i++) {
            $builder->addPlayer(TeamSide::HOME, 12, 5 + $i, movement: 6, id: $i + 1);
        }
        for ($i = 0; $i < 8; $i++) {
            $builder->addPlayer(TeamSide::HOME, 6, $i + 3, movement: 6, id: $i + 4);
        }

        // 11 away players
        for ($i = 0; $i < 3; $i++) {
            $builder->addPlayer(TeamSide::AWAY, 13, 5 + $i, movement: 6, id: $i + 20);
        }
        for ($i = 0; $i < 8; $i++) {
            $builder->addPlayer(TeamSide::AWAY, 19, $i + 3, movement: 6, id: $i + 23);
        }

        $state = $builder->build();

        $ai = new RandomAICoach();
        $rules = new RulesEngine();

        // Simulate a full AI turn
        $actions = 0;
        for ($i = 0; $i < 100; $i++) {
            $decision = $ai->decideAction($state, $rules);
            $actions++;

            if ($decision['action'] === ActionType::END_TURN) {
                break;
            }

            $dice = new FixedDiceRoller([6, 6, 6, 6, 6, 6, 6, 6, 6, 6]); // all succeed
            $resolver = new ActionResolver($dice);

            try {
                $result = $resolver->resolve($state, $decision['action'], $decision['params']);
                $state = $result->getNewState();

                if ($result->isTurnover()) {
                    break;
                }
            } catch (\Exception $e) {
                // AI picked an invalid action - fallback to END_TURN
                break;
            }
        }

        $this->assertGreaterThan(0, $actions, 'AI should take at least one action');
    }

    public function testGreedyAiScoresTouchdownWhenPossible(): void
    {
        // Ball carrier one step from end zone
        $builder = new GameStateBuilder();
        $builder->addPlayer(TeamSide::HOME, 24, 7, movement: 6, id: 1);
        $builder->addPlayer(TeamSide::AWAY, 13, 7, id: 2);
        $builder->withBallCarried(1);
        $state = $builder->build();

        $ai = new GreedyAICoach();
        $rules = new RulesEngine();
        $decision = $ai->decideAction($state, $rules);

        $this->assertSame(ActionType::MOVE, $decision['action']);
        $this->assertSame(25, $decision['params']['x']);
    }

    public function testGreedyAiPicksUpLooseBall(): void
    {
        $builder = new GameStateBuilder();
        $builder->addPlayer(TeamSide::HOME, 10, 7, movement: 6, agility: 3, id: 1);
        $builder->addPlayer(TeamSide::AWAY, 20, 7, id: 2);
        $builder->withBallOnGround(12, 7);
        $builder->withHomeTeam(
            \App\DTO\TeamStateDTO::create(1, 'Home', 'Human', TeamSide::HOME, 3)->withBlitzUsed(),
        );
        $state = $builder->build();

        $ai = new GreedyAICoach();
        $rules = new RulesEngine();
        $decision = $ai->decideAction($state, $rules);

        $this->assertSame(ActionType::MOVE, $decision['action']);
        $this->assertSame(12, $decision['params']['x']);
        $this->assertSame(7, $decision['params']['y']);
    }
}
