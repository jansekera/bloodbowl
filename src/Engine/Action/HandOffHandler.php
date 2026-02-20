<?php

declare(strict_types=1);

namespace App\Engine\Action;

use App\DTO\ActionResult;
use App\DTO\BallState;
use App\DTO\GameEvent;
use App\DTO\GameState;
use App\DTO\MatchPlayerDTO;
use App\Engine\BallResolver;
use App\Engine\DiceRollerInterface;
use App\Enum\SkillName;

final class HandOffHandler implements ActionHandlerInterface
{
    public function __construct(
        private readonly BallResolver $ballResolver,
        private readonly DiceRollerInterface $dice,
    ) {
    }

    /**
     * @param array<string, mixed> $params
     */
    public function resolve(GameState $state, array $params): ActionResult
    {
        $playerId = (int) $params['playerId'];
        $targetId = (int) $params['targetId'];

        $giver = $state->getPlayer($playerId);
        $receiver = $state->getPlayer($targetId);

        if ($giver === null || $receiver === null) {
            throw new \InvalidArgumentException('Player not found');
        }

        $giverPos = $giver->getPosition();
        $receiverPos = $receiver->getPosition();

        if ($giverPos === null || $receiverPos === null) {
            throw new \InvalidArgumentException('Players must be on pitch');
        }

        if ($giverPos->distanceTo($receiverPos) !== 1) {
            throw new \InvalidArgumentException('Players must be adjacent for hand-off');
        }

        $events = [GameEvent::handOff($playerId, $targetId)];

        // Animosity check: if giver has Animosity and receiver has different raceName
        if ($giver->hasSkill(SkillName::Animosity) && $this->isDifferentRace($giver, $receiver)) {
            $animRoll = $this->dice->rollD6();
            $animSuccess = $animRoll >= 2;
            $events[] = GameEvent::animosity($playerId, $targetId, $animRoll, $animSuccess);
            if (!$animSuccess) {
                // Ball stays with giver, mark acted, not a turnover
                $state = $state->withPlayer($giver->withHasActed(true)->withHasMoved(true));
                return ActionResult::success($state, $events);
            }
        }

        // Mark giver as acted
        $state = $state->withPlayer($giver->withHasActed(true)->withHasMoved(true));

        // Move ball to receiver's position for the catch attempt
        $state = $state->withBall(BallState::onGround($receiverPos));

        // Receiver attempts catch with +1 modifier
        $activeSide = $state->getActiveTeam();
        $teamRerollAvailable = $state->getTeamState($activeSide)->canUseReroll();
        $catchResult = $this->ballResolver->resolveCatch($state, $receiver, modifier: 1, teamRerollAvailable: $teamRerollAvailable);
        $events = array_merge($events, $catchResult['events']);
        $state = $catchResult['state'];

        if ($catchResult['teamRerollUsed']) {
            $state = $state->withTeamState($activeSide, $state->getTeamState($activeSide)->withRerollUsed());
        }

        if (!$catchResult['success']) {
            $events[] = GameEvent::turnover('Failed hand-off');
            return ActionResult::turnover($state->withTurnoverPending(true), $events);
        }

        return ActionResult::success($state, $events);
    }

    private function isDifferentRace(MatchPlayerDTO $a, MatchPlayerDTO $b): bool
    {
        $raceA = $a->getRaceName();
        $raceB = $b->getRaceName();
        if ($raceA === null && $raceB === null) {
            return false;
        }
        return $raceA !== $raceB;
    }
}
