<?php
declare(strict_types=1);

namespace App\Engine\Action;

use App\DTO\ActionResult;
use App\DTO\GameEvent;
use App\DTO\GameState;
use App\Enum\PlayerState;
use App\Enum\SkillName;
use App\Engine\DiceRollerInterface;
use App\Engine\TacklezoneCalculator;

final class HypnoticGazeHandler implements ActionHandlerInterface
{
    public function __construct(
        private readonly DiceRollerInterface $dice,
        private readonly TacklezoneCalculator $tzCalc,
    ) {
    }

    /**
     * @param array<string, mixed> $params {playerId, targetId}
     */
    public function resolve(GameState $state, array $params): ActionResult
    {
        $gazerId = (int) $params['playerId'];
        $targetId = (int) $params['targetId'];

        $gazer = $state->getPlayer($gazerId);
        $target = $state->getPlayer($targetId);

        if ($gazer === null || $target === null) {
            throw new \InvalidArgumentException('Player not found');
        }
        if (!$gazer->hasSkill(SkillName::HypnoticGaze)) {
            throw new \InvalidArgumentException('Player must have Hypnotic Gaze skill');
        }

        $gazerPos = $gazer->getPosition();
        $targetPos = $target->getPosition();

        if ($gazerPos === null || $targetPos === null) {
            throw new \InvalidArgumentException('Players must be on pitch');
        }
        if ($gazerPos->distanceTo($targetPos) !== 1) {
            throw new \InvalidArgumentException('Target must be adjacent');
        }

        // Mark gazer as acted
        $state = $state->withPlayer($gazer->withHasActed(true)->withHasMoved(true));

        // Roll: 2+ on D6, modified by opposing tackle zones on gazer
        $tz = $this->tzCalc->countTacklezones($state, $gazerPos, $gazer->getTeamSide());
        $target_roll = min(6, 2 + $tz);

        $roll = $this->dice->rollD6();
        $success = $roll >= $target_roll;

        $events = [GameEvent::hypnoticGaze($gazerId, $targetId, $roll, $success)];

        if ($success) {
            // Target loses tackle zones
            $target = $target->withLostTacklezones(true);
            $state = $state->withPlayer($target);

            return ActionResult::success($state, $events);
        }

        // Failure: turnover
        $events[] = GameEvent::turnover('Hypnotic Gaze failed');
        return ActionResult::turnover($state->withTurnoverPending(true), $events);
    }
}
