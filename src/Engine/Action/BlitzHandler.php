<?php

declare(strict_types=1);

namespace App\Engine\Action;

use App\DTO\ActionResult;
use App\DTO\GameState;
use App\DTO\MatchPlayerDTO;
use App\Enum\PlayerState;
use App\Enum\SkillName;
use App\ValueObject\Position;
use App\Engine\Pathfinder;

final class BlitzHandler implements ActionHandlerInterface
{
    public function __construct(
        private readonly MoveHandler $moveHandler,
        private readonly BlockHandler $blockHandler,
        private readonly Pathfinder $pathfinder,
    ) {
    }

    /**
     * @param array<string, mixed> $params
     */
    public function resolve(GameState $state, array $params): ActionResult
    {
        $attackerId = (int) $params['playerId'];
        $targetId = (int) $params['targetId'];

        $attacker = $state->getPlayer($attackerId);
        $defender = $state->getPlayer($targetId);

        if ($attacker === null || $defender === null) {
            throw new \InvalidArgumentException('Player not found');
        }

        $attackerPos = $attacker->getPosition();
        $defenderPos = $defender->getPosition();

        if ($attackerPos === null || $defenderPos === null) {
            throw new \InvalidArgumentException('Players must be on pitch');
        }

        // Mark blitz used for this turn
        $teamState = $state->getTeamState($attacker->getTeamSide());
        $state = $state->withTeamState($attacker->getTeamSide(), $teamState->withBlitzUsed());

        $events = [];

        // If not adjacent, move to adjacent square first
        if ($attackerPos->distanceTo($defenderPos) > 1) {
            // Find best adjacent square to move to
            $moveTarget = $this->findBlitzMoveTarget($state, $attacker, $defenderPos);
            if ($moveTarget === null) {
                throw new \InvalidArgumentException('Cannot reach target for blitz');
            }

            // Resolve movement to adjacent square
            $moveResult = $this->moveHandler->resolve($state, [
                'playerId' => $attackerId,
                'x' => $moveTarget->getX(),
                'y' => $moveTarget->getY(),
            ]);

            if ($moveResult->isTurnover()) {
                return $moveResult;
            }

            $state = $moveResult->getNewState();
            $events = array_merge($events, $moveResult->getEvents());

            // Reset hasMoved so block can still mark it
            $movedAttacker = $state->getPlayer($attackerId);
            if ($movedAttacker !== null) {
                $state = $state->withPlayer($movedAttacker->withHasMoved(false)->withHasActed(false));
            }
        }

        // Now resolve the block (with Horns bonus if applicable)
        $blockParams = [
            'playerId' => $attackerId,
            'targetId' => $targetId,
            'isBlitz' => true,
        ];
        $freshAttacker = $state->getPlayer($attackerId);
        if ($freshAttacker !== null && $freshAttacker->hasSkill(SkillName::Horns)) {
            $blockParams['hornsBonus'] = true;
        }
        $blockResult = $this->blockHandler->resolve($state, $blockParams);

        $events = array_merge($events, $blockResult->getEvents());

        if ($blockResult->isTurnover()) {
            return ActionResult::turnover($blockResult->getNewState(), $events);
        }

        return ActionResult::success($blockResult->getNewState(), $events);
    }

    /**
     * Find best adjacent square to defender for a blitz move.
     */
    private function findBlitzMoveTarget(GameState $state, MatchPlayerDTO $attacker, Position $defenderPos): ?Position
    {
        $validMoves = $this->pathfinder->findValidMoves($state, $attacker);

        $bestTarget = null;
        $bestScore = PHP_INT_MAX;

        foreach ($validMoves as $path) {
            $dest = $path->getDestination();
            if ($dest->distanceTo($defenderPos) !== 1) {
                continue;
            }

            // Prefer fewest dodges, then fewest GFIs
            $score = $path->getDodgeCount() * 100 + $path->getGfiCount() * 10 + $path->getTotalCost();

            // Surfing tiebreaker: prefer approach angle that pushes defender toward sideline
            $dy = $defenderPos->getY() - $dest->getY();
            $ndy = $dy === 0 ? 0 : ($dy > 0 ? 1 : -1);
            $pushY = $defenderPos->getY() + $ndy;

            $surfBonus = 0;
            if ($pushY < 0 || $pushY > 14) {
                $surfBonus = 5; // crowd surf!
            } elseif ($pushY === 0 || $pushY === 14) {
                $surfBonus = 3; // pushed to edge
            } elseif ($pushY === 1 || $pushY === 13) {
                $surfBonus = 1; // pushed toward edge
            }

            // Same for X edges
            $dx = $defenderPos->getX() - $dest->getX();
            $ndx = $dx === 0 ? 0 : ($dx > 0 ? 1 : -1);
            $pushX = $defenderPos->getX() + $ndx;
            if ($pushX < 0 || $pushX > 25) {
                $surfBonus = max($surfBonus, 5);
            }

            $score -= $surfBonus;

            if ($score < $bestScore) {
                $bestScore = $score;
                $bestTarget = $dest;
            }
        }

        return $bestTarget;
    }
}
