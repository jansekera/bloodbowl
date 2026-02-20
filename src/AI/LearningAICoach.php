<?php
declare(strict_types=1);

namespace App\AI;

use App\DTO\GameState;
use App\Engine\RulesEngine;
use App\Enum\ActionType;
use App\Enum\PlayerState;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use App\ValueObject\Position;

final class LearningAICoach implements AICoachInterface
{
    private string $modelType = 'linear';
    /** @var list<float> */
    private array $weights = [];
    private float $epsilon;

    // Neural network parameters (only used when modelType === 'neural')
    /** @var list<list<float>> */
    private array $W1 = [];
    /** @var list<float> */
    private array $b1 = [];
    /** @var list<list<float>> */
    private array $W2 = [];
    /** @var list<float> */
    private array $b2 = [];

    /**
     * @param string|null $weightsFile Path to weights JSON file (null = zero weights)
     */
    public function __construct(?string $weightsFile = null, float $epsilon = 0.0)
    {
        $this->epsilon = $epsilon;

        if ($weightsFile !== null && file_exists($weightsFile)) {
            $data = json_decode(file_get_contents($weightsFile), true);
            if (is_array($data) && isset($data['type']) && $data['type'] === 'neural') {
                $this->modelType = 'neural';
                $this->W1 = $data['W1'];
                $this->b1 = $data['b1'];
                $this->W2 = $data['W2'];
                $this->b2 = $data['b2'];
            } else {
                $this->weights = array_map('floatval', $data);
            }
        } else {
            $this->weights = array_fill(0, FeatureExtractor::NUM_FEATURES, 0.0);
        }
    }

    public function decideAction(GameState $state, RulesEngine $rules): array
    {
        $side = $state->getActiveTeam();
        $actions = $rules->getAvailableActions($state);

        // Cache base state evaluation (used by many scoring functions)
        $baseScore = $this->evaluateState($state, $side);

        // Deduplicate action types per player to avoid redundant work
        $seenActions = [];

        // Build scored candidate actions
        /** @var list<array{action: ActionType, params: array<string, mixed>, score: float}> */
        $candidates = [];
        foreach ($actions as $action) {
            $type = ActionType::from($action['type']);
            $playerId = $action['playerId'] ?? null;

            if ($type === ActionType::SETUP_PLAYER || $type === ActionType::END_SETUP) {
                continue;
            }

            if ($type === ActionType::END_TURN) {
                $candidates[] = [
                    'action' => ActionType::END_TURN,
                    'params' => [],
                    'score' => $baseScore - 0.01,
                ];
                continue;
            }

            if ($playerId === null) {
                continue;
            }

            // Deduplicate: only evaluate one action per type+player
            $key = $type->value . '_' . $playerId;
            if (isset($seenActions[$key])) {
                continue;
            }
            $seenActions[$key] = true;

            $built = $this->buildScoredAction($state, $rules, $type, (int) $playerId, $side, $baseScore);
            if ($built !== null) {
                $candidates[] = $built;
            }
        }

        if ($candidates === []) {
            return ['action' => ActionType::END_TURN, 'params' => []];
        }

        // Epsilon-greedy: with probability epsilon, pick random
        if ($this->epsilon > 0 && (mt_rand() / mt_getrandmax()) < $this->epsilon) {
            $pick = $candidates[array_rand($candidates)];
            return ['action' => $pick['action'], 'params' => $pick['params']];
        }

        // Pick the highest-scored candidate
        $bestScore = -PHP_FLOAT_MAX;
        $bestAction = $candidates[0];
        foreach ($candidates as $candidate) {
            if ($candidate['score'] > $bestScore) {
                $bestScore = $candidate['score'];
                $bestAction = $candidate;
            }
        }

        return ['action' => $bestAction['action'], 'params' => $bestAction['params']];
    }

    public function setupFormation(GameState $state, TeamSide $side): GameState
    {
        $offPitchPlayers = [];
        foreach ($state->getTeamPlayers($side) as $player) {
            if ($player->getState() === PlayerState::OFF_PITCH) {
                $offPitchPlayers[] = $player;
            }
        }

        if ($side === TeamSide::HOME) {
            $positions = [
                new Position(12, 6), new Position(12, 7), new Position(12, 8),
                new Position(8, 4), new Position(8, 6), new Position(8, 8), new Position(8, 10),
                new Position(4, 3), new Position(4, 5), new Position(4, 9), new Position(4, 11),
            ];
        } else {
            $positions = [
                new Position(13, 6), new Position(13, 7), new Position(13, 8),
                new Position(17, 4), new Position(17, 6), new Position(17, 8), new Position(17, 10),
                new Position(21, 3), new Position(21, 5), new Position(21, 9), new Position(21, 11),
            ];
        }

        $count = min(count($offPitchPlayers), count($positions));
        for ($i = 0; $i < $count; $i++) {
            $state = $state->withPlayer(
                $offPitchPlayers[$i]
                    ->withPosition($positions[$i])
                    ->withState(PlayerState::STANDING),
            );
        }

        return $state;
    }

    /**
     * Score a state using the loaded model (linear dot product or neural network).
     */
    public function evaluateState(GameState $state, TeamSide $perspective): float
    {
        $features = FeatureExtractor::extract($state, $perspective);
        if ($this->modelType === 'neural') {
            return $this->evaluateNeural($features);
        }
        return self::dotProduct($this->weights, $features);
    }

    /**
     * Neural network inference: h = ReLU(features @ W1 + b1), output = tanh(h @ W2 + b2).
     *
     * @param list<float> $features
     */
    public function evaluateNeural(array $features): float
    {
        $nFeatures = count($this->W1);
        $hiddenSize = count($this->b1);

        // h = features @ W1 + b1 (then ReLU)
        $h = [];
        for ($j = 0; $j < $hiddenSize; $j++) {
            $sum = $this->b1[$j];
            $len = min(count($features), $nFeatures);
            for ($i = 0; $i < $len; $i++) {
                $sum += $features[$i] * $this->W1[$i][$j];
            }
            // ReLU
            $h[$j] = max(0.0, $sum);
        }

        // output = tanh(h @ W2 + b2)
        $out = $this->b2[0];
        for ($j = 0; $j < $hiddenSize; $j++) {
            $out += $h[$j] * $this->W2[$j][0];
        }

        return tanh($out);
    }

    public function getModelType(): string
    {
        return $this->modelType;
    }

    /**
     * @return list<float>
     */
    public function getWeights(): array
    {
        return $this->weights;
    }

    /**
     * @param list<float> $weights
     */
    public function setWeights(array $weights): void
    {
        $this->modelType = 'linear';
        $this->weights = $weights;
    }

    /**
     * Set neural network weights directly.
     *
     * @param list<list<float>> $W1
     * @param list<float> $b1
     * @param list<list<float>> $W2
     * @param list<float> $b2
     */
    public function setNeuralWeights(array $W1, array $b1, array $W2, array $b2): void
    {
        $this->modelType = 'neural';
        $this->W1 = $W1;
        $this->b1 = $b1;
        $this->W2 = $W2;
        $this->b2 = $b2;
    }

    /**
     * Build a scored action: picks the best target and evaluates resulting hypothetical state.
     *
     * @return array{action: ActionType, params: array<string, mixed>, score: float}|null
     */
    private function buildScoredAction(
        GameState $state,
        RulesEngine $rules,
        ActionType $type,
        int $playerId,
        TeamSide $side,
        float $baseScore,
    ): ?array {
        return match ($type) {
            ActionType::MOVE => $this->buildMoveAction($state, $rules, $playerId, $side),
            ActionType::BLOCK => $this->buildBlockAction($state, $rules, $playerId, $side, $baseScore),
            ActionType::BLITZ => $this->buildBlitzAction($state, $playerId, $side, $baseScore),
            ActionType::PASS => $this->buildPassAction($state, $rules, $playerId, $side, $baseScore),
            ActionType::HAND_OFF => $this->buildHandOffAction($state, $rules, $playerId, $side, $baseScore),
            ActionType::FOUL => $this->buildFoulAction($state, $rules, $playerId, $side, $baseScore),
            ActionType::BALL_AND_CHAIN => $this->buildBallAndChainAction($baseScore),
            ActionType::HYPNOTIC_GAZE => $this->buildHypnoticGazeAction($state, $rules, $playerId, $side, $baseScore),
            ActionType::BOMB_THROW => $this->buildBombThrowAction($state, $rules, $playerId, $side, $baseScore),
            ActionType::MULTIPLE_BLOCK => $this->buildMultipleBlockAction($state, $rules, $playerId, $side, $baseScore),
            default => null,
        };
    }

    /**
     * @return array{action: ActionType, params: array<string, mixed>, score: float}|null
     */
    private function buildMoveAction(GameState $state, RulesEngine $rules, int $playerId, TeamSide $side): ?array
    {
        $targets = $rules->getValidMoveTargets($state, $playerId);
        if ($targets === []) {
            return null;
        }

        $player = $state->getPlayer($playerId);
        if ($player === null) {
            return null;
        }

        // Quick heuristic scoring without full feature extraction per target
        // Use the endzone direction and ball-related logic
        $ball = $state->getBall();
        $isCarrier = $ball->isHeld() && $ball->getCarrierId() === $playerId;
        $endZoneX = $side === TeamSide::HOME ? 25 : 0;
        $currentPos = $player->getPosition();

        $bestScore = -PHP_FLOAT_MAX;
        $bestTarget = $targets[0];

        foreach ($targets as $target) {
            $score = 0.0;
            $riskPenalty = $target['dodges'] * 0.15 + $target['gfis'] * 0.08;

            // Touchdown: carrier reaching endzone
            if ($isCarrier) {
                $pos = new Position($target['x'], $target['y']);
                if ($pos->isInEndZone($side !== TeamSide::HOME)) {
                    $tdScore = $this->shouldStall($state, $side) ? 1.0 : 10.0;
                    $score = $tdScore - $riskPenalty;
                    if ($score > $bestScore) {
                        $bestScore = $score;
                        $bestTarget = $target;
                    }
                    continue;
                }
            }

            // Pick up ball
            if (!$ball->isHeld() && $ball->isOnPitch()) {
                $ballPos = $ball->getPosition();
                if ($ballPos !== null && $target['x'] === $ballPos->getX() && $target['y'] === $ballPos->getY()) {
                    $score = 5.0 - $riskPenalty;
                    if ($score > $bestScore) {
                        $bestScore = $score;
                        $bestTarget = $target;
                    }
                    continue;
                }
            }

            // Forward advancement
            $distToEndZone = abs($target['x'] - $endZoneX);
            if ($currentPos !== null) {
                $currentDist = abs($currentPos->getX() - $endZoneX);
                $advancement = $currentDist - $distToEndZone;
                $score = $advancement * 0.1 - $riskPenalty;
            }

            // Defensive positioning: penalize moving to sideline
            $targetY = $target['y'];
            if ($targetY === 0 || $targetY === 14) {
                $score -= 0.2;
            } elseif ($targetY === 1 || $targetY === 13) {
                $score -= 0.05;
            }

            // Carrier movement strategy
            if ($isCarrier) {
                if ($this->shouldStall($state, $side)) {
                    // When stalling: prefer 2-5 squares from endzone, central Y positions
                    if ($distToEndZone >= 2 && $distToEndZone <= 5) {
                        $score += 1.5;
                    }
                    // Centrality bonus (Y=7 is center of 0-14 pitch)
                    $centralityScore = 1.0 - abs($target['y'] - 7) / 7.0;
                    $score += $centralityScore * 0.3;
                } else {
                    // Normal: move closer to endzone
                    $score += (26 - $distToEndZone) * 0.05;
                }
            }

            // Cage formation: move adjacent to own ball carrier
            if (!$isCarrier && $ball->isHeld() && $ball->getCarrierId() !== null) {
                $carrier = $state->getPlayer($ball->getCarrierId());
                if ($carrier !== null && $carrier->getTeamSide() === $side && $carrier->getPosition() !== null) {
                    $carrierPos = $carrier->getPosition();
                    $pos = new Position($target['x'], $target['y']);
                    if ($carrierPos->distanceTo($pos) === 1) {
                        $score += 1.0;
                    }
                }
            }

            if ($score > $bestScore) {
                $bestScore = $score;
                $bestTarget = $target;
            }
        }

        // Add base state evaluation to the score
        $baseScore = $this->evaluateState($state, $side);

        return [
            'action' => ActionType::MOVE,
            'params' => ['playerId' => $playerId, 'x' => $bestTarget['x'], 'y' => $bestTarget['y']],
            'score' => $baseScore + $bestScore,
        ];
    }

    /**
     * @return array{action: ActionType, params: array<string, mixed>, score: float}|null
     */
    private function buildBlockAction(GameState $state, RulesEngine $rules, int $playerId, TeamSide $side, float $baseScore): ?array
    {
        $player = $state->getPlayer($playerId);
        if ($player === null) {
            return null;
        }

        $targets = $rules->getBlockTargets($state, $player);
        if ($targets === []) {
            return null;
        }

        $bestScore = -PHP_FLOAT_MAX;
        $bestTarget = $targets[0];
        $ball = $state->getBall();

        foreach ($targets as $target) {
            $score = $baseScore + 0.05;
            if ($ball->isHeld() && $ball->getCarrierId() === $target->getId()) {
                $score += 0.5;
            }

            // Sideline surfing bonus
            $targetPos = $target->getPosition();
            if ($targetPos !== null) {
                $ty = $targetPos->getY();
                if ($ty === 0 || $ty === 14) {
                    $score += 0.3;
                } elseif ($ty === 1 || $ty === 13) {
                    $score += 0.1;
                }
            }

            if ($score > $bestScore) {
                $bestScore = $score;
                $bestTarget = $target;
            }
        }

        return [
            'action' => ActionType::BLOCK,
            'params' => ['playerId' => $playerId, 'targetId' => $bestTarget->getId()],
            'score' => $bestScore,
        ];
    }

    /**
     * @return array{action: ActionType, params: array<string, mixed>, score: float}|null
     */
    private function buildBlitzAction(GameState $state, int $playerId, TeamSide $side, float $baseScore): ?array
    {
        $player = $state->getPlayer($playerId);
        if ($player === null) {
            return null;
        }

        $enemies = $state->getPlayersOnPitch($side->opponent());
        if ($enemies === []) {
            return null;
        }

        $ball = $state->getBall();
        $bestScore = -PHP_FLOAT_MAX;
        $bestTarget = $enemies[0];

        foreach ($enemies as $enemy) {
            $score = $baseScore + 0.03;
            if ($ball->isHeld() && $ball->getCarrierId() === $enemy->getId()) {
                $score += 0.8;
            }

            // Sideline surfing bonus (higher for blitz — it's a scarce resource)
            $enemyPos = $enemy->getPosition();
            if ($enemyPos !== null) {
                $ey = $enemyPos->getY();
                if ($ey === 0 || $ey === 14) {
                    $score += 0.4;
                } elseif ($ey === 1 || $ey === 13) {
                    $score += 0.15;
                }
                // End zone edges
                $ex = $enemyPos->getX();
                if ($ex === 0 || $ex === 25) {
                    $score += 0.2;
                }
            }

            if ($score > $bestScore) {
                $bestScore = $score;
                $bestTarget = $enemy;
            }
        }

        return [
            'action' => ActionType::BLITZ,
            'params' => ['playerId' => $playerId, 'targetId' => $bestTarget->getId()],
            'score' => $bestScore,
        ];
    }

    /**
     * @return array{action: ActionType, params: array<string, mixed>, score: float}|null
     */
    private function buildPassAction(GameState $state, RulesEngine $rules, int $playerId, TeamSide $side, float $baseScore): ?array
    {
        $player = $state->getPlayer($playerId);
        if ($player === null) {
            return null;
        }

        $targets = $rules->getPassTargets($state, $player);
        if ($targets === []) {
            return null;
        }

        $bestScore = -PHP_FLOAT_MAX;
        $bestTarget = $targets[0];

        foreach ($targets as $target) {
            $rangePenalty = match ($target['range'] ?? 'short') {
                'quick' => 0.0,
                'short' => 0.02,
                'long' => 0.05,
                'bomb' => 0.1,
                default => 0.03,
            };
            $score = $baseScore - $rangePenalty;

            if ($score > $bestScore) {
                $bestScore = $score;
                $bestTarget = $target;
            }
        }

        return [
            'action' => ActionType::PASS,
            'params' => ['playerId' => $playerId, 'targetX' => $bestTarget['x'], 'targetY' => $bestTarget['y']],
            'score' => $bestScore,
        ];
    }

    /**
     * @return array{action: ActionType, params: array<string, mixed>, score: float}|null
     */
    private function buildHandOffAction(GameState $state, RulesEngine $rules, int $playerId, TeamSide $side, float $baseScore): ?array
    {
        $player = $state->getPlayer($playerId);
        if ($player === null) {
            return null;
        }

        $targets = $rules->getHandOffTargets($state, $player);
        if ($targets === []) {
            return null;
        }

        $target = $targets[0];

        return [
            'action' => ActionType::HAND_OFF,
            'params' => ['playerId' => $playerId, 'targetId' => $target->getId()],
            'score' => $baseScore + 0.01,
        ];
    }

    /**
     * @return array{action: ActionType, params: array<string, mixed>, score: float}|null
     */
    private function buildFoulAction(GameState $state, RulesEngine $rules, int $playerId, TeamSide $side, float $baseScore): ?array
    {
        $player = $state->getPlayer($playerId);
        if ($player === null) {
            return null;
        }

        $targets = $rules->getFoulTargets($state, $player);
        if ($targets === []) {
            return null;
        }

        $bestTarget = $targets[0];
        $lowestArmour = $bestTarget->getStats()->getArmour();
        foreach ($targets as $target) {
            if ($target->getStats()->getArmour() < $lowestArmour) {
                $lowestArmour = $target->getStats()->getArmour();
                $bestTarget = $target;
            }
        }

        return [
            'action' => ActionType::FOUL,
            'params' => ['playerId' => $playerId, 'targetId' => $bestTarget->getId()],
            'score' => $baseScore - 0.02,
        ];
    }

    /**
     * Ball & Chain is the only action B&C players can take — must always be used.
     *
     * @return array{action: ActionType, params: array<string, mixed>, score: float}
     */
    private function buildBallAndChainAction(float $baseScore): array
    {
        return [
            'action' => ActionType::BALL_AND_CHAIN,
            'params' => [],
            'score' => $baseScore + 0.1,
        ];
    }

    /**
     * @return array{action: ActionType, params: array<string, mixed>, score: float}|null
     */
    private function buildHypnoticGazeAction(GameState $state, RulesEngine $rules, int $playerId, TeamSide $side, float $baseScore): ?array
    {
        $player = $state->getPlayer($playerId);
        if ($player === null) {
            return null;
        }

        $targets = $rules->getHypnoticGazeTargets($state, $player);
        if ($targets === []) {
            return null;
        }

        $ball = $state->getBall();
        $bestScore = -PHP_FLOAT_MAX;
        $bestTarget = $targets[0];

        foreach ($targets as $target) {
            $score = $baseScore + 0.05;

            // High priority: gaze the ball carrier
            if ($ball->isHeld() && $ball->getCarrierId() === $target->getId()) {
                $score += 0.4;
            }

            // Bonus for gazing strong players
            if ($target->getStats()->getStrength() >= 4) {
                $score += 0.1;
            }

            if ($score > $bestScore) {
                $bestScore = $score;
                $bestTarget = $target;
            }
        }

        return [
            'action' => ActionType::HYPNOTIC_GAZE,
            'params' => ['playerId' => $playerId, 'targetId' => $bestTarget->getId()],
            'score' => $bestScore,
        ];
    }

    /**
     * @return array{action: ActionType, params: array<string, mixed>, score: float}|null
     */
    private function buildBombThrowAction(GameState $state, RulesEngine $rules, int $playerId, TeamSide $side, float $baseScore): ?array
    {
        $player = $state->getPlayer($playerId);
        if ($player === null || !$player->hasSkill(SkillName::Bombardier)) {
            return null;
        }

        $targets = $rules->getPassTargets($state, $player);
        if ($targets === []) {
            return null;
        }

        $enemies = $state->getPlayersOnPitch($side->opponent());
        $myPlayers = $state->getPlayersOnPitch($side);

        $bestScore = -PHP_FLOAT_MAX;
        $bestTarget = null;

        foreach ($targets as $target) {
            $tx = $target['x'];
            $ty = $target['y'];

            $enemyCount = 0;
            $allyCount = 0;

            foreach ($enemies as $enemy) {
                $ePos = $enemy->getPosition();
                if ($ePos !== null && abs($ePos->getX() - $tx) <= 1 && abs($ePos->getY() - $ty) <= 1) {
                    $enemyCount++;
                }
            }

            foreach ($myPlayers as $ally) {
                $aPos = $ally->getPosition();
                if ($aPos !== null && abs($aPos->getX() - $tx) <= 1 && abs($aPos->getY() - $ty) <= 1) {
                    $allyCount++;
                }
            }

            if ($enemyCount === 0) {
                continue;
            }

            $score = $baseScore + $enemyCount * 0.08 - $allyCount * 0.06;

            if ($score > $bestScore) {
                $bestScore = $score;
                $bestTarget = $target;
            }
        }

        if ($bestTarget === null) {
            return null;
        }

        return [
            'action' => ActionType::BOMB_THROW,
            'params' => ['playerId' => $playerId, 'targetX' => $bestTarget['x'], 'targetY' => $bestTarget['y']],
            'score' => $bestScore,
        ];
    }

    /**
     * @return array{action: ActionType, params: array<string, mixed>, score: float}|null
     */
    private function buildMultipleBlockAction(GameState $state, RulesEngine $rules, int $playerId, TeamSide $side, float $baseScore): ?array
    {
        $player = $state->getPlayer($playerId);
        if ($player === null) {
            return null;
        }

        $targets = $rules->getBlockTargets($state, $player);
        if (count($targets) < 2) {
            return null;
        }

        $ball = $state->getBall();
        $bestPairScore = -PHP_FLOAT_MAX;
        $bestT1 = $targets[0];
        $bestT2 = $targets[1];

        // Evaluate all pairs (small count so O(n^2) is fine)
        for ($i = 0; $i < count($targets); $i++) {
            for ($j = $i + 1; $j < count($targets); $j++) {
                $score = $baseScore + 0.03;

                // Ball carrier bonus
                if ($ball->isHeld()) {
                    if ($ball->getCarrierId() === $targets[$i]->getId()) {
                        $score += 0.4;
                    }
                    if ($ball->getCarrierId() === $targets[$j]->getId()) {
                        $score += 0.4;
                    }
                }

                if ($score > $bestPairScore) {
                    $bestPairScore = $score;
                    $bestT1 = $targets[$i];
                    $bestT2 = $targets[$j];
                }
            }
        }

        return [
            'action' => ActionType::MULTIPLE_BLOCK,
            'params' => [
                'playerId' => $playerId,
                'targetId' => $bestT1->getId(),
                'targetId2' => $bestT2->getId(),
            ],
            'score' => $bestPairScore,
        ];
    }

    /**
     * Should the AI stall (hold the ball) rather than score immediately?
     */
    private function shouldStall(GameState $state, TeamSide $side): bool
    {
        $myTeam = $state->getTeamState($side);
        $oppTeam = $state->getTeamState($side->opponent());
        $scoreDiff = $myTeam->getScore() - $oppTeam->getScore();
        $turnsLeft = max(0, 9 - $myTeam->getTurnNumber());

        // Behind: never stall
        if ($scoreDiff < 0) {
            return false;
        }

        // Last 2 turns: always score
        if ($turnsLeft <= 2) {
            return false;
        }

        // Tied with 3 or fewer turns: score
        if ($scoreDiff === 0 && $turnsLeft <= 3) {
            return false;
        }

        // Ahead or tied with plenty of time: stall
        return true;
    }

    /**
     * @param list<float> $a
     * @param list<float> $b
     */
    private static function dotProduct(array $a, array $b): float
    {
        $sum = 0.0;
        $len = min(count($a), count($b));
        for ($i = 0; $i < $len; $i++) {
            $sum += $a[$i] * $b[$i];
        }
        return $sum;
    }
}
