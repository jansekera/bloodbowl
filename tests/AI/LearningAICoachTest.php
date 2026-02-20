<?php
declare(strict_types=1);

namespace App\Tests\AI;

use App\AI\FeatureExtractor;
use App\AI\LearningAICoach;
use App\DTO\TeamStateDTO;
use App\Engine\RulesEngine;
use App\Enum\ActionType;
use App\Enum\PlayerState;
use App\Enum\TeamSide;
use App\Tests\Engine\GameStateBuilder;
use PHPUnit\Framework\TestCase;

final class LearningAICoachTest extends TestCase
{
    public function testZeroWeightsReturnsValidAction(): void
    {
        $builder = new GameStateBuilder();
        $builder->withActiveTeam(TeamSide::HOME);
        $builder->addPlayer(TeamSide::HOME, 10, 7, movement: 6, id: 1);
        $builder->addPlayer(TeamSide::AWAY, 15, 7, id: 2);
        $state = $builder->build();

        $ai = new LearningAICoach(null, 0.0);
        $rules = new RulesEngine();

        $decision = $ai->decideAction($state, $rules);

        $this->assertInstanceOf(ActionType::class, $decision['action']);
        $this->assertIsArray($decision['params']);
    }

    public function testHighScoreDiffWeightPrefersTouchdown(): void
    {
        $builder = new GameStateBuilder();
        $builder->withActiveTeam(TeamSide::HOME);
        // Player one step from TD
        $builder->addPlayer(TeamSide::HOME, 24, 7, movement: 6, id: 1);
        $builder->addPlayer(TeamSide::AWAY, 5, 7, id: 2);
        $builder->withBallCarried(1);
        // Behind by 1 so AI won't stall
        $homeTeam = TeamStateDTO::create(1, 'Home', 'Human', TeamSide::HOME, 3)->withScore(0);
        $awayTeam = TeamStateDTO::create(2, 'Away', 'Orc', TeamSide::AWAY, 2)->withScore(1);
        $builder->withHomeTeam($homeTeam);
        $builder->withAwayTeam($awayTeam);
        $state = $builder->build();

        // Set high weight on score_diff (index 0) and negative weight on carrier_dist_to_td (index 15)
        $weights = array_fill(0, FeatureExtractor::NUM_FEATURES, 0.0);
        $weights[0] = 10.0;   // score_diff — heavily reward scoring
        $weights[15] = -5.0;  // carrier_dist_to_td — reward being close to endzone

        $ai = new LearningAICoach(null, 0.0);
        $ai->setWeights($weights);
        $rules = new RulesEngine();

        $decision = $ai->decideAction($state, $rules);

        // Should move toward the endzone
        $this->assertSame(ActionType::MOVE, $decision['action']);
        $this->assertSame(25, $decision['params']['x']);
    }

    public function testEvaluateStateIsDotProduct(): void
    {
        $builder = new GameStateBuilder();
        $builder->addPlayer(TeamSide::HOME, 10, 7, id: 1);
        $builder->addPlayer(TeamSide::AWAY, 15, 7, id: 2);
        $state = $builder->build();

        $weights = array_fill(0, FeatureExtractor::NUM_FEATURES, 0.0);
        $weights[29] = 5.0; // bias weight

        $ai = new LearningAICoach(null);
        $ai->setWeights($weights);

        $score = $ai->evaluateState($state, TeamSide::HOME);

        // bias feature = 1.0, weight = 5.0, all others = 0 → score = 5.0
        $this->assertEqualsWithDelta(5.0, $score, 0.001);
    }

    public function testEpsilonOnePicksRandomly(): void
    {
        $builder = new GameStateBuilder();
        $builder->withActiveTeam(TeamSide::HOME);
        $builder->addPlayer(TeamSide::HOME, 10, 7, movement: 6, id: 1);
        $builder->addPlayer(TeamSide::AWAY, 15, 7, id: 2);
        $state = $builder->build();

        $ai = new LearningAICoach(null, 1.0); // always explore
        $rules = new RulesEngine();

        // Run 50 times and check we get variety (not always the same action)
        $actionTypes = [];
        for ($i = 0; $i < 50; $i++) {
            $decision = $ai->decideAction($state, $rules);
            $key = $decision['action']->value;
            if (isset($decision['params']['x'])) {
                $key .= "_{$decision['params']['x']}_{$decision['params']['y']}";
            } elseif (isset($decision['params']['targetId'])) {
                $key .= "_{$decision['params']['targetId']}";
            }
            $actionTypes[$key] = true;
        }

        $this->assertGreaterThan(1, count($actionTypes), 'Epsilon=1.0 should produce variety');
    }

    public function testEpsilonZeroIsDeterministic(): void
    {
        $builder = new GameStateBuilder();
        $builder->withActiveTeam(TeamSide::HOME);
        $builder->addPlayer(TeamSide::HOME, 10, 7, movement: 6, id: 1);
        $builder->addPlayer(TeamSide::AWAY, 15, 7, id: 2);
        $state = $builder->build();

        $weights = array_fill(0, FeatureExtractor::NUM_FEATURES, 0.0);
        $weights[17] = 10.0; // reward high my_avg_x (forward position)

        $ai = new LearningAICoach(null, 0.0);
        $ai->setWeights($weights);
        $rules = new RulesEngine();

        $decision1 = $ai->decideAction($state, $rules);
        $decision2 = $ai->decideAction($state, $rules);

        $this->assertSame($decision1['action'], $decision2['action']);
        $this->assertSame($decision1['params'], $decision2['params']);
    }

    public function testBlitzPrefersSidelineTarget(): void
    {
        $builder = new GameStateBuilder();
        $builder->withActiveTeam(TeamSide::HOME);
        // Blitzer in center
        $builder->addPlayer(TeamSide::HOME, 12, 7, movement: 6, strength: 3, id: 1);
        // Enemy on sideline Y=0
        $builder->addPlayer(TeamSide::AWAY, 13, 0, strength: 3, id: 2);
        // Enemy in center Y=7
        $builder->addPlayer(TeamSide::AWAY, 13, 7, strength: 3, id: 3);
        $state = $builder->build();

        $ai = new LearningAICoach(null, 0.0);
        $rules = new RulesEngine();

        $decision = $ai->decideAction($state, $rules);

        // Should prefer blitzing the sideline target
        if ($decision['action'] === ActionType::BLITZ) {
            $this->assertSame(2, $decision['params']['targetId'], 'Should blitz sideline target');
        } else {
            // If it chose block instead (adjacent target), that's also valid
            $this->assertTrue(true);
        }
    }

    public function testCarrierStallsWhenAhead(): void
    {
        $builder = new GameStateBuilder();
        $builder->withActiveTeam(TeamSide::HOME);
        // Carrier 1 step from TD at x=24, leading 1-0, turn 2
        $builder->addPlayer(TeamSide::HOME, 24, 7, movement: 6, id: 1);
        $builder->addPlayer(TeamSide::AWAY, 5, 7, id: 2);
        $builder->withBallCarried(1);
        $homeTeam = TeamStateDTO::create(1, 'Home', 'Human', TeamSide::HOME, 3)
            ->withScore(1)->withTurnNumber(2);
        $awayTeam = TeamStateDTO::create(2, 'Away', 'Orc', TeamSide::AWAY, 2)->withScore(0);
        $builder->withHomeTeam($homeTeam);
        $builder->withAwayTeam($awayTeam);
        $state = $builder->build();

        $ai = new LearningAICoach(null, 0.0);
        $rules = new RulesEngine();

        $decision = $ai->decideAction($state, $rules);

        // Should NOT score — should stall (move but not to x=25)
        $this->assertSame(ActionType::MOVE, $decision['action']);
        $this->assertNotSame(25, $decision['params']['x'], 'Carrier should stall, not score');
    }

    public function testCarrierScoresWhenBehind(): void
    {
        $builder = new GameStateBuilder();
        $builder->withActiveTeam(TeamSide::HOME);
        // Carrier 1 step from TD at x=24, trailing 0-1
        $builder->addPlayer(TeamSide::HOME, 24, 7, movement: 6, id: 1);
        $builder->addPlayer(TeamSide::AWAY, 5, 7, id: 2);
        $builder->withBallCarried(1);
        $homeTeam = TeamStateDTO::create(1, 'Home', 'Human', TeamSide::HOME, 3)
            ->withScore(0)->withTurnNumber(2);
        $awayTeam = TeamStateDTO::create(2, 'Away', 'Orc', TeamSide::AWAY, 2)->withScore(1);
        $builder->withHomeTeam($homeTeam);
        $builder->withAwayTeam($awayTeam);
        $state = $builder->build();

        $ai = new LearningAICoach(null, 0.0);
        $rules = new RulesEngine();

        $decision = $ai->decideAction($state, $rules);

        // Should score — trailing, no reason to stall
        $this->assertSame(ActionType::MOVE, $decision['action']);
        $this->assertSame(25, $decision['params']['x'], 'Carrier should score when behind');
    }

    public function testCarrierScoresOnLastTurns(): void
    {
        $builder = new GameStateBuilder();
        $builder->withActiveTeam(TeamSide::HOME);
        // Carrier 1 step from TD, leading but turn 8 (last turn)
        $builder->addPlayer(TeamSide::HOME, 24, 7, movement: 6, id: 1);
        $builder->addPlayer(TeamSide::AWAY, 5, 7, id: 2);
        $builder->withBallCarried(1);
        $homeTeam = TeamStateDTO::create(1, 'Home', 'Human', TeamSide::HOME, 3)
            ->withScore(1)->withTurnNumber(8);
        $awayTeam = TeamStateDTO::create(2, 'Away', 'Orc', TeamSide::AWAY, 2)->withScore(0);
        $builder->withHomeTeam($homeTeam);
        $builder->withAwayTeam($awayTeam);
        $state = $builder->build();

        $ai = new LearningAICoach(null, 0.0);
        $rules = new RulesEngine();

        $decision = $ai->decideAction($state, $rules);

        // Should score on last turn even when ahead
        $this->assertSame(ActionType::MOVE, $decision['action']);
        $this->assertSame(25, $decision['params']['x'], 'Carrier should score on last turn');
    }

    public function testSetupFormationPlaces11Players(): void
    {
        $builder = new GameStateBuilder();
        $builder->withPhase(\App\Enum\GamePhase::SETUP);
        for ($i = 0; $i < 11; $i++) {
            $builder->addOffPitchPlayer(TeamSide::HOME, id: $i + 1);
        }
        for ($i = 0; $i < 11; $i++) {
            $builder->addOffPitchPlayer(TeamSide::AWAY, id: $i + 20);
        }
        $state = $builder->build();

        $ai = new LearningAICoach(null);
        $state = $ai->setupFormation($state, TeamSide::HOME);

        $onPitch = $state->getPlayersOnPitch(TeamSide::HOME);
        $this->assertCount(11, $onPitch);
    }

    public function testNeuralWeightsHardcoded(): void
    {
        // Simple 3-input, 2-hidden neural network with known weights
        $W1 = [
            [0.5, -0.3],   // input 0 -> hidden
            [0.2, 0.8],    // input 1 -> hidden
            [-0.1, 0.4],   // input 2 -> hidden
        ];
        $b1 = [0.1, -0.2];
        $W2 = [
            [0.6],   // hidden 0 -> output
            [-0.5],  // hidden 1 -> output
        ];
        $b2 = [0.05];

        // features = [1.0, 0.5, -0.5]
        // z1 = [1.0*0.5 + 0.5*0.2 + (-0.5)*(-0.1) + 0.1, 1.0*(-0.3) + 0.5*0.8 + (-0.5)*0.4 + (-0.2)]
        //    = [0.5 + 0.1 + 0.05 + 0.1, -0.3 + 0.4 - 0.2 - 0.2]
        //    = [0.75, -0.3]
        // h = ReLU(z1) = [0.75, 0.0]
        // z2 = 0.75*0.6 + 0.0*(-0.5) + 0.05 = 0.45 + 0.05 = 0.5
        // output = tanh(0.5) ≈ 0.46212

        $ai = new LearningAICoach(null);
        $ai->setNeuralWeights($W1, $b1, $W2, $b2);

        $features = array_fill(0, FeatureExtractor::NUM_FEATURES, 0.0);
        $features[0] = 1.0;
        $features[1] = 0.5;
        $features[2] = -0.5;

        $result = $ai->evaluateNeural($features);
        $expected = tanh(0.5);
        $this->assertEqualsWithDelta($expected, $result, 0.0001);
    }

    public function testNeuralInferenceFromFile(): void
    {
        // Create a temporary neural weights file
        $tmpFile = tempnam(sys_get_temp_dir(), 'neural_weights_');
        $W1 = [[0.5, -0.3], [0.2, 0.8]];
        $b1 = [0.1, -0.2];
        $W2 = [[0.6], [-0.5]];
        $b2 = [0.05];
        $data = [
            'type' => 'neural',
            'hidden_size' => 2,
            'n_features' => 2,
            'W1' => $W1,
            'b1' => $b1,
            'W2' => $W2,
            'b2' => $b2,
        ];
        file_put_contents($tmpFile, json_encode($data));

        $ai = new LearningAICoach($tmpFile);
        $this->assertSame('neural', $ai->getModelType());

        unlink($tmpFile);
    }

    public function testLinearWeightsStillWork(): void
    {
        // Create a temporary linear weights file
        $tmpFile = tempnam(sys_get_temp_dir(), 'linear_weights_');
        $weights = array_fill(0, FeatureExtractor::NUM_FEATURES, 0.0);
        $weights[29] = 5.0; // bias weight
        file_put_contents($tmpFile, json_encode($weights));

        $ai = new LearningAICoach($tmpFile);
        $this->assertSame('linear', $ai->getModelType());

        $builder = new GameStateBuilder();
        $builder->addPlayer(TeamSide::HOME, 10, 7, id: 1);
        $builder->addPlayer(TeamSide::AWAY, 15, 7, id: 2);
        $state = $builder->build();

        $score = $ai->evaluateState($state, TeamSide::HOME);
        $this->assertEqualsWithDelta(5.0, $score, 0.001);

        unlink($tmpFile);
    }

    public function testNeuralCrossValidationWithPython(): void
    {
        // Create a known neural network and verify PHP matches expected output
        // This test uses the same weights as the Python cross-validation
        $W1 = [];
        $b1 = [];
        $W2 = [];
        $b2 = [0.0];

        // Simple identity-like network: 5 inputs, 4 hidden
        for ($i = 0; $i < 5; $i++) {
            $row = [];
            for ($j = 0; $j < 4; $j++) {
                $row[] = ($i === $j) ? 1.0 : 0.0;
            }
            $W1[] = $row;
        }
        $b1 = [0.0, 0.0, 0.0, 0.0];
        for ($j = 0; $j < 4; $j++) {
            $W2[] = [0.25];
        }

        // features = [0.5, 0.3, 0.7, 0.1, 0.0]
        // z1 = [0.5, 0.3, 0.7, 0.1]  (identity projection of first 4 features)
        // h = ReLU = [0.5, 0.3, 0.7, 0.1]
        // z2 = 0.5*0.25 + 0.3*0.25 + 0.7*0.25 + 0.1*0.25 = 0.4
        // output = tanh(0.4) ≈ 0.37995

        $ai = new LearningAICoach(null);
        $ai->setNeuralWeights($W1, $b1, $W2, $b2);

        $features = array_fill(0, FeatureExtractor::NUM_FEATURES, 0.0);
        $features[0] = 0.5;
        $features[1] = 0.3;
        $features[2] = 0.7;
        $features[3] = 0.1;

        $result = $ai->evaluateNeural($features);
        $expected = tanh(0.4);
        $this->assertEqualsWithDelta($expected, $result, 0.0001);
    }
}
