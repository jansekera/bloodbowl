<?php
declare(strict_types=1);

namespace App\AI;

use App\DTO\GameState;
use App\DTO\MatchPlayerDTO;
use App\Engine\RulesEngine;
use App\Engine\StrengthCalculator;
use App\Enum\ActionType;
use App\Enum\PlayerState;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use App\ValueObject\Position;

final class GreedyAICoach implements AICoachInterface
{
    private readonly StrengthCalculator $strCalc;

    public function __construct()
    {
        $this->strCalc = new StrengthCalculator();
    }

    public function decideAction(GameState $state, RulesEngine $rules): array
    {
        $side = $state->getActiveTeam();
        $actions = $rules->getAvailableActions($state);

        $bestScore = -1;
        $bestAction = ['action' => ActionType::END_TURN, 'params' => []];

        foreach ($actions as $action) {
            $type = ActionType::from($action['type']);
            $playerId = $action['playerId'] ?? null;

            if ($type === ActionType::END_TURN) {
                continue;
            }
            if ($type === ActionType::SETUP_PLAYER || $type === ActionType::END_SETUP) {
                continue;
            }
            if ($playerId === null) {
                continue;
            }

            $scored = $this->scoreAction($state, $rules, $type, $playerId, $side);
            if ($scored !== null && $scored['score'] > $bestScore) {
                $bestScore = $scored['score'];
                $bestAction = ['action' => $scored['action'], 'params' => $scored['params']];
            }
        }

        return $bestAction;
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
     * @return array{action: ActionType, params: array<string, mixed>, score: int}|null
     */
    private function scoreAction(
        GameState $state,
        RulesEngine $rules,
        ActionType $type,
        int $playerId,
        TeamSide $side,
    ): ?array {
        return match ($type) {
            ActionType::MOVE => $this->scoreMove($state, $rules, $playerId, $side),
            ActionType::BLOCK => $this->scoreBlock($state, $playerId, $side),
            ActionType::BLITZ => $this->scoreBlitz($state, $playerId, $side),
            ActionType::PASS => $this->scorePass($state, $rules, $playerId, $side),
            ActionType::HAND_OFF => $this->scoreHandOff($state, $rules, $playerId, $side),
            ActionType::FOUL => $this->scoreFoul($state, $rules, $playerId),
            ActionType::BALL_AND_CHAIN => $this->scoreBallAndChain(),
            ActionType::HYPNOTIC_GAZE => $this->scoreHypnoticGaze($state, $rules, $playerId, $side),
            ActionType::BOMB_THROW => $this->scoreBombThrow($state, $rules, $playerId, $side),
            ActionType::MULTIPLE_BLOCK => $this->scoreMultipleBlock($state, $playerId, $side),
            default => null,
        };
    }

    /**
     * @return array{action: ActionType, params: array<string, mixed>, score: int}|null
     */
    private function scoreMove(
        GameState $state,
        RulesEngine $rules,
        int $playerId,
        TeamSide $side,
    ): ?array {
        $targets = $rules->getValidMoveTargets($state, $playerId);
        if ($targets === []) {
            return null;
        }

        $player = $state->getPlayer($playerId);
        if ($player === null) {
            return null;
        }

        $ball = $state->getBall();
        $isCarrier = $ball->isHeld() && $ball->getCarrierId() === $playerId;
        $endZoneX = $side === TeamSide::HOME ? 25 : 0;

        $bestTarget = null;
        $bestScore = -1;

        foreach ($targets as $target) {
            $score = 0;
            $pos = new Position($target['x'], $target['y']);

            // Score touchdown: ball carrier reaching end zone
            if ($isCarrier && $pos->isInEndZone($side !== TeamSide::HOME)) {
                $riskPenalty = $target['dodges'] * 30 + $target['gfis'] * 15;
                $score = 1000 - $riskPenalty;
                if ($bestTarget === null || $score > $bestScore) {
                    $bestTarget = $target;
                    $bestScore = $score;
                }
                continue;
            }

            // Pick up ball: move to loose ball position
            if (!$ball->isHeld() && $ball->isOnPitch()) {
                $ballPos = $ball->getPosition();
                if ($ballPos !== null && $pos->equals($ballPos)) {
                    $riskPenalty = $target['dodges'] * 30 + $target['gfis'] * 15;
                    $score = 400 - $riskPenalty;
                    if ($bestTarget === null || $score > $bestScore) {
                        $bestTarget = $target;
                        $bestScore = $score;
                    }
                    continue;
                }
            }

            // Cage formation: move adjacent to own ball carrier
            if (!$isCarrier && $ball->isHeld() && $ball->getCarrierId() !== null) {
                $carrier = $state->getPlayer($ball->getCarrierId());
                if ($carrier !== null && $carrier->getTeamSide() === $side) {
                    $carrierPos = $carrier->getPosition();
                    if ($carrierPos !== null && $pos->distanceTo($carrierPos) === 1) {
                        $riskPenalty = $target['dodges'] * 30 + $target['gfis'] * 15;
                        $score = 200 - $riskPenalty;
                    }
                }
            }

            // Move forward: closer to opponent end zone
            $distToEndZone = abs($target['x'] - $endZoneX);
            $currentPos = $player->getPosition();
            if ($currentPos !== null) {
                $currentDist = abs($currentPos->getX() - $endZoneX);
                $advancement = $currentDist - $distToEndZone;
                if ($advancement > 0) {
                    $riskPenalty = $target['dodges'] * 30 + $target['gfis'] * 15;
                    $score = max($score, 50 + $advancement * 10 - $riskPenalty);
                }
            }

            if ($score > $bestScore) {
                $bestScore = $score;
                $bestTarget = $target;
            }
        }

        if ($bestTarget === null || $bestScore <= 0) {
            return null;
        }

        return [
            'action' => ActionType::MOVE,
            'params' => ['playerId' => $playerId, 'x' => $bestTarget['x'], 'y' => $bestTarget['y']],
            'score' => $bestScore,
        ];
    }

    /**
     * @return array{action: ActionType, params: array<string, mixed>, score: int}|null
     */
    private function scoreBlock(
        GameState $state,
        int $playerId,
        TeamSide $side,
    ): ?array {
        $player = $state->getPlayer($playerId);
        if ($player === null || $player->getPosition() === null) {
            return null;
        }

        $enemies = $state->getPlayersOnPitch($side->opponent());
        $bestScore = -1;
        $bestTargetId = null;

        foreach ($enemies as $enemy) {
            $enemyPos = $enemy->getPosition();
            if ($enemyPos === null || $player->getPosition()->distanceTo($enemyPos) !== 1) {
                continue;
            }
            if (!$enemy->getState()->canAct()) {
                continue;
            }

            $attStr = $this->strCalc->calculateEffectiveStrength($state, $player, $enemyPos);
            $defStr = $this->strCalc->calculateEffectiveStrength($state, $enemy, $player->getPosition());
            $diceInfo = $this->strCalc->getBlockDiceInfo($attStr, $defStr);
            $dice = $diceInfo['attackerChooses'] ? $diceInfo['count'] : -$diceInfo['count'];
            $score = match (true) {
                $dice >= 2 => 300,
                $dice === 1 => 150,
                default => 50, // both down risk
            };

            // Bonus for blocking ball carrier
            $ball = $state->getBall();
            if ($ball->isHeld() && $ball->getCarrierId() === $enemy->getId()) {
                $score += 200;
            }

            if ($score > $bestScore) {
                $bestScore = $score;
                $bestTargetId = $enemy->getId();
            }
        }

        if ($bestTargetId === null) {
            return null;
        }

        return [
            'action' => ActionType::BLOCK,
            'params' => ['playerId' => $playerId, 'targetId' => $bestTargetId],
            'score' => $bestScore,
        ];
    }

    /**
     * @return array{action: ActionType, params: array<string, mixed>, score: int}|null
     */
    private function scoreBlitz(
        GameState $state,
        int $playerId,
        TeamSide $side,
    ): ?array {
        $player = $state->getPlayer($playerId);
        if ($player === null || $player->getPosition() === null) {
            return null;
        }

        $ball = $state->getBall();
        $enemies = $state->getPlayersOnPitch($side->opponent());

        $bestScore = -1;
        $bestTargetId = null;

        foreach ($enemies as $enemy) {
            $enemyPos = $enemy->getPosition();
            if ($enemyPos === null) {
                continue;
            }

            $score = 100;

            // High priority: blitz the ball carrier
            if ($ball->isHeld() && $ball->getCarrierId() === $enemy->getId()) {
                $score = 500;
            }

            // Bonus for favorable strength
            $attStr = $this->strCalc->calculateEffectiveStrength($state, $player, $enemyPos);
            $defStr = $this->strCalc->calculateEffectiveStrength($state, $enemy, $player->getPosition());
            $diceInfo = $this->strCalc->getBlockDiceInfo($attStr, $defStr);
            $dice = $diceInfo['attackerChooses'] ? $diceInfo['count'] : -$diceInfo['count'];
            $score += max(0, $dice) * 30;

            if ($score > $bestScore) {
                $bestScore = $score;
                $bestTargetId = $enemy->getId();
            }
        }

        if ($bestTargetId === null) {
            return null;
        }

        return [
            'action' => ActionType::BLITZ,
            'params' => ['playerId' => $playerId, 'targetId' => $bestTargetId],
            'score' => $bestScore,
        ];
    }

    /**
     * @return array{action: ActionType, params: array<string, mixed>, score: int}|null
     */
    private function scorePass(
        GameState $state,
        RulesEngine $rules,
        int $playerId,
        TeamSide $side,
    ): ?array {
        $player = $state->getPlayer($playerId);
        if ($player === null) {
            return null;
        }

        $targets = $rules->getPassTargets($state, $player);
        if ($targets === []) {
            return null;
        }

        $endZoneX = $side === TeamSide::HOME ? 25 : 0;
        $bestScore = -1;
        $bestTarget = null;

        foreach ($targets as $target) {
            $targetPos = new Position($target['x'], $target['y']);
            // Prefer passing to a teammate closer to the end zone
            $receiver = $state->getPlayerAtPosition($targetPos);
            if ($receiver === null || $receiver->getTeamSide() !== $side) {
                continue;
            }

            $distToEndZone = abs($target['x'] - $endZoneX);
            $score = 150 - $distToEndZone * 5;

            // Prefer shorter passes (more accurate)
            $rangePenalty = match ($target['range']) {
                'quick' => 0,
                'short' => 10,
                'long' => 30,
                'bomb' => 60,
                default => 20,
            };
            $score -= $rangePenalty;

            if ($score > $bestScore) {
                $bestScore = $score;
                $bestTarget = $target;
            }
        }

        if ($bestTarget === null || $bestScore <= 0) {
            return null;
        }

        return [
            'action' => ActionType::PASS,
            'params' => ['playerId' => $playerId, 'targetX' => $bestTarget['x'], 'targetY' => $bestTarget['y']],
            'score' => $bestScore,
        ];
    }

    /**
     * @return array{action: ActionType, params: array<string, mixed>, score: int}|null
     */
    private function scoreHandOff(
        GameState $state,
        RulesEngine $rules,
        int $playerId,
        TeamSide $side,
    ): ?array {
        $player = $state->getPlayer($playerId);
        if ($player === null) {
            return null;
        }

        $targets = $rules->getHandOffTargets($state, $player);
        if ($targets === []) {
            return null;
        }

        $endZoneX = $side === TeamSide::HOME ? 25 : 0;
        $bestScore = -1;
        $bestTargetId = null;

        foreach ($targets as $receiver) {
            $receiverPos = $receiver->getPosition();
            if ($receiverPos === null) {
                continue;
            }

            $distToEndZone = abs($receiverPos->getX() - $endZoneX);
            $score = 150 - $distToEndZone * 5;

            if ($score > $bestScore) {
                $bestScore = $score;
                $bestTargetId = $receiver->getId();
            }
        }

        if ($bestTargetId === null || $bestScore <= 0) {
            return null;
        }

        return [
            'action' => ActionType::HAND_OFF,
            'params' => ['playerId' => $playerId, 'targetId' => $bestTargetId],
            'score' => $bestScore,
        ];
    }

    /**
     * @return array{action: ActionType, params: array<string, mixed>, score: int}|null
     */
    private function scoreFoul(
        GameState $state,
        RulesEngine $rules,
        int $playerId,
    ): ?array {
        $player = $state->getPlayer($playerId);
        if ($player === null) {
            return null;
        }

        $targets = $rules->getFoulTargets($state, $player);
        if ($targets === []) {
            return null;
        }

        // Pick the weakest armour target
        $bestTarget = $targets[0];
        $lowestArmour = $bestTarget->getStats()->getArmour();
        foreach ($targets as $target) {
            $av = $target->getStats()->getArmour();
            if ($av < $lowestArmour) {
                $lowestArmour = $av;
                $bestTarget = $target;
            }
        }

        return [
            'action' => ActionType::FOUL,
            'params' => ['playerId' => $playerId, 'targetId' => $bestTarget->getId()],
            'score' => 80,
        ];
    }

    /**
     * Ball & Chain is the only action B&C players can take — always use it.
     *
     * @return array{action: ActionType, params: array<string, mixed>, score: int}
     */
    private function scoreBallAndChain(): array
    {
        return [
            'action' => ActionType::BALL_AND_CHAIN,
            'params' => [],
            'score' => 200,
        ];
    }

    /**
     * @return array{action: ActionType, params: array<string, mixed>, score: int}|null
     */
    private function scoreHypnoticGaze(
        GameState $state,
        RulesEngine $rules,
        int $playerId,
        TeamSide $side,
    ): ?array {
        $player = $state->getPlayer($playerId);
        if ($player === null) {
            return null;
        }

        $targets = $rules->getHypnoticGazeTargets($state, $player);
        if ($targets === []) {
            return null;
        }

        $ball = $state->getBall();
        $bestScore = -1;
        $bestTargetId = null;

        foreach ($targets as $target) {
            $score = 120;

            // High priority: gaze the ball carrier
            if ($ball->isHeld() && $ball->getCarrierId() === $target->getId()) {
                $score += 150;
            }

            // Bonus for gazing strong players
            if ($target->getStats()->getStrength() >= 4) {
                $score += 30;
            }

            if ($score > $bestScore) {
                $bestScore = $score;
                $bestTargetId = $target->getId();
            }
        }

        if ($bestTargetId === null) {
            return null;
        }

        // Penalty for gazer being in many tackle zones (reduces success chance)
        $gazerPos = $player->getPosition();
        if ($gazerPos !== null) {
            $enemies = $state->getPlayersOnPitch($side->opponent());
            $tzCount = 0;
            foreach ($enemies as $enemy) {
                $enemyPos = $enemy->getPosition();
                if ($enemyPos !== null && $gazerPos->distanceTo($enemyPos) === 1 && $enemy->getState()->canAct()) {
                    $tzCount++;
                }
            }
            // Each extra TZ beyond the target itself reduces score
            $bestScore -= max(0, $tzCount - 1) * 20;
        }

        return [
            'action' => ActionType::HYPNOTIC_GAZE,
            'params' => ['playerId' => $playerId, 'targetId' => $bestTargetId],
            'score' => max(1, $bestScore),
        ];
    }

    /**
     * @return array{action: ActionType, params: array<string, mixed>, score: int}|null
     */
    private function scoreBombThrow(
        GameState $state,
        RulesEngine $rules,
        int $playerId,
        TeamSide $side,
    ): ?array {
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

        $bestScore = -1;
        $bestTarget = null;

        foreach ($targets as $target) {
            $tx = $target['x'];
            $ty = $target['y'];

            // Count enemies and allies in 3x3 area around target
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

            // Only consider targets that hit enemies without too many allies
            if ($enemyCount === 0) {
                continue;
            }

            // Prefer shorter range for accuracy
            $rangePenalty = match ($target['range']) {
                'quick' => 0,
                'short' => 10,
                'long' => 25,
                'bomb' => 50,
                default => 15,
            };

            $score = 80 + $enemyCount * 40 - $allyCount * 30 - $rangePenalty;

            if ($score > $bestScore) {
                $bestScore = $score;
                $bestTarget = $target;
            }
        }

        if ($bestTarget === null || $bestScore <= 0) {
            return null;
        }

        return [
            'action' => ActionType::BOMB_THROW,
            'params' => ['playerId' => $playerId, 'targetX' => $bestTarget['x'], 'targetY' => $bestTarget['y']],
            'score' => $bestScore,
        ];
    }

    /**
     * @return array{action: ActionType, params: array<string, mixed>, score: int}|null
     */
    private function scoreMultipleBlock(
        GameState $state,
        int $playerId,
        TeamSide $side,
    ): ?array {
        $player = $state->getPlayer($playerId);
        if ($player === null || $player->getPosition() === null) {
            return null;
        }

        $enemies = $state->getPlayersOnPitch($side->opponent());
        $ball = $state->getBall();

        // Collect adjacent standing enemies with their individual scores
        $adjacentTargets = [];
        foreach ($enemies as $enemy) {
            $enemyPos = $enemy->getPosition();
            if ($enemyPos === null || $player->getPosition()->distanceTo($enemyPos) !== 1) {
                continue;
            }
            if (!$enemy->getState()->canAct()) {
                continue;
            }

            // Each defender gets +2 ST in Multiple Block
            $attStr = $this->strCalc->calculateEffectiveStrength($state, $player, $enemyPos);
            $defStr = $this->strCalc->calculateEffectiveStrength($state, $enemy, $player->getPosition()) + 2;
            $diceInfo = $this->strCalc->getBlockDiceInfo($attStr, $defStr);
            $dice = $diceInfo['attackerChooses'] ? $diceInfo['count'] : -$diceInfo['count'];
            $score = match (true) {
                $dice >= 2 => 120,
                $dice === 1 => 60,
                default => 20,
            };

            if ($ball->isHeld() && $ball->getCarrierId() === $enemy->getId()) {
                $score += 100;
            }

            $adjacentTargets[] = ['enemy' => $enemy, 'score' => $score];
        }

        if (count($adjacentTargets) < 2) {
            return null;
        }

        // Sort by score descending, pick best 2
        usort($adjacentTargets, fn($a, $b) => $b['score'] <=> $a['score']);
        $target1 = $adjacentTargets[0];
        $target2 = $adjacentTargets[1];
        $combinedScore = $target1['score'] + $target2['score'];

        return [
            'action' => ActionType::MULTIPLE_BLOCK,
            'params' => [
                'playerId' => $playerId,
                'targetId' => $target1['enemy']->getId(),
                'targetId2' => $target2['enemy']->getId(),
            ],
            'score' => $combinedScore,
        ];
    }
}
