<?php
declare(strict_types=1);

namespace App\Engine\Action;

use App\DTO\ActionResult;
use App\DTO\BallState;
use App\DTO\GameEvent;
use App\DTO\GameState;
use App\DTO\PendingRerollDTO;
use App\Engine\BallResolver;
use App\Engine\DiceRollerInterface;
use App\Enum\PlayerState;
use App\Enum\SkillName;
use App\ValueObject\Position;

final class RerollHandler
{
    public function __construct(
        private readonly DiceRollerInterface $dice,
        private readonly BallResolver $ballResolver,
    ) {
    }

    /**
     * @param array<string, mixed> $params
     */
    public function resolve(GameState $state, array $params): ActionResult
    {
        $pending = $state->getPendingReroll();
        if ($pending === null) {
            throw new \InvalidArgumentException('No pending reroll to resolve');
        }

        $choice = (string) ($params['choice'] ?? 'decline');
        $state = $state->withPendingReroll(null);

        return match ($choice) {
            'pro' => $this->resolveProReroll($state, $pending),
            'team_reroll' => $this->resolveTeamReroll($state, $pending),
            'decline' => $this->applyFailure($state, $pending),
            default => throw new \InvalidArgumentException("Invalid reroll choice: {$choice}"),
        };
    }

    /**
     * Auto-resolve: always accept the best available reroll.
     */
    public function autoResolve(GameState $state): ActionResult
    {
        $pending = $state->getPendingReroll();
        if ($pending === null) {
            throw new \InvalidArgumentException('No pending reroll');
        }

        $state = $state->withPendingReroll(null);

        // AI: try Pro first, then Team Reroll
        if ($pending->isProAvailable()) {
            return $this->resolveProReroll($state, $pending);
        }
        if ($pending->isTeamRerollAvailable()) {
            return $this->resolveTeamReroll($state, $pending);
        }

        return $this->applyFailure($state, $pending);
    }

    private function resolveProReroll(GameState $state, PendingRerollDTO $pending): ActionResult
    {
        $player = $state->getPlayer($pending->getPlayerId());
        if ($player === null) {
            return $this->applyFailure($state, $pending);
        }

        $player = $player->withProUsedThisTurn(true);
        $state = $state->withPlayer($player);

        $proRoll = $this->dice->rollD6();
        $events = [];

        if ($proRoll < 4) {
            $events[] = GameEvent::proReroll($pending->getPlayerId(), $proRoll, false, null);

            // r. 8385-8387: puvodni vysledek plati; tymovy prehoz uz jen na hod Pro
            if ($pending->isTeamRerollAvailable()) {
                return ActionResult::success($state->withPendingReroll($pending->withProFailed()), $events);
            }

            return $this->applyFailure($state, $pending, $events);
        }

        $events[] = GameEvent::proReroll($pending->getPlayerId(), $proRoll, true, null);

        // ⛔ OPRAVA 18.09.2026: po prehozu kostky Pro se uz tymovy prehoz
        //   NENABIZI -- r. 926: "you may never re-roll a single dice roll more
        //   than once." Driv se nabidl a kostka sla prehodit podruhe.
        return $this->rerollDie($state, $pending, $events);
    }

    /**
     * Prehodi puvodni kostku (jedinkrat) a vyhodnoti vysledek.
     *
     * @param list<GameEvent> $events
     */
    private function rerollDie(GameState $state, PendingRerollDTO $pending, array $events): ActionResult
    {
        $newRoll = $this->dice->rollD6();
        $success = $newRoll >= $pending->getTarget();
        $events[] = $this->makeRollEvent($pending, $newRoll, $success);

        if ($success) {
            return $this->applySuccess($state, $pending, $events);
        }

        return $this->applyFailure($state, $pending, $events);
    }

    private function resolveTeamReroll(GameState $state, PendingRerollDTO $pending): ActionResult
    {
        $side = $state->getActiveTeam();
        $state = $state->withTeamState($side, $state->getTeamState($side)->withRerollUsed());

        $player = $state->getPlayer($pending->getPlayerId());
        if ($player === null) {
            return $this->applyFailure($state, $pending);
        }

        $events = [];

        // Loner check
        if ($player->hasSkill(SkillName::Loner)) {
            $lonerRoll = $this->dice->rollD6();
            if ($lonerRoll < 4) {
                $events[] = GameEvent::lonerCheck($pending->getPlayerId(), $lonerRoll, false);
                return $this->applyFailure($state, $pending, $events);
            }
            $events[] = GameEvent::lonerCheck($pending->getPlayerId(), $lonerRoll, true);
        }

        $events[] = GameEvent::rerollUsed($pending->getPlayerId(), 'Team Reroll');

        // ⛔ OPRAVA 18.09.2026: po neuspesnem Pro prehazuje tymovy prehoz HOD
        //   PRO, ne puvodni kostku (`rules_bb2016.txt` r. 8385-8387).
        if ($pending->isProFailed()) {
            $proRoll = $this->dice->rollD6();
            $events[] = GameEvent::proReroll($pending->getPlayerId(), $proRoll, $proRoll >= 4, null);
            if ($proRoll < 4) {
                return $this->applyFailure($state, $pending, $events);
            }
        }

        return $this->rerollDie($state, $pending, $events);
    }

    /**
     * @param list<GameEvent> $events
     */
    private function applySuccess(GameState $state, PendingRerollDTO $pending, array $events): ActionResult
    {
        $player = $state->getPlayer($pending->getPlayerId());
        if ($player === null) {
            return ActionResult::success($state, $events);
        }

        $target = new Position($pending->getTargetX(), $pending->getTargetY());

        if ($pending->getRollType() === 'pickup') {
            $pos = $player->getPosition();
            if ($pos !== null) {
                $state = $state->withBall(BallState::carried($pos, $player->getId()));
            }
            return ActionResult::success($state, $events);
        }

        // Dodge or GFI: move player to target square
        $player = $player
            ->withPosition($target)
            ->withMovementRemaining($player->getMovementRemaining() - 1);
        $state = $state->withPlayer($player);

        // Move ball with carrier
        if ($state->getBall()->getCarrierId() === $player->getId()) {
            $state = $state->withBall(BallState::carried($target, $player->getId()));
        }

        // Check for ball pickup at target
        $ball = $state->getBall();
        if (!$ball->isHeld() && $ball->getPosition() !== null && $ball->getPosition()->equals($target)) {
            $freshPlayer = $state->getPlayer($player->getId()) ?? $player;
            $pickupResult = $this->ballResolver->resolvePickup($state, $freshPlayer, false);
            $events = array_merge($events, $pickupResult['events']);
            $state = $pickupResult['state'];
            if (!$pickupResult['success']) {
                $events[] = GameEvent::turnover('Failed pickup');
                $failPlayer = ($state->getPlayer($pending->getPlayerId()) ?? $freshPlayer)
                    ->withHasMoved(true)
                    ->withHasActed(true);
                $state = $state->withPlayer($failPlayer);
                return ActionResult::turnover($state->withTurnoverPending(true), $events);
            }
        }

        return ActionResult::success($state, $events);
    }

    /**
     * @param list<GameEvent> $events
     */
    private function applyFailure(GameState $state, PendingRerollDTO $pending, array $events = []): ActionResult
    {
        $player = $state->getPlayer($pending->getPlayerId());
        if ($player === null) {
            return ActionResult::turnover($state->withTurnoverPending(true), $events);
        }

        if ($pending->getRollType() === 'pickup') {
            $pos = $player->getPosition();
            if ($pos !== null) {
                $bounceResult = $this->ballResolver->resolveBounce($state, $pos);
                $events = array_merge($events, $bounceResult['events']);
                $state = $bounceResult['state'];
            }
            $events[] = GameEvent::turnover('Failed pickup');
            $player = ($state->getPlayer($pending->getPlayerId()) ?? $player)
                ->withHasMoved(true)
                ->withHasActed(true);
            $state = $state->withPlayer($player);
            return ActionResult::turnover($state->withTurnoverPending(true), $events);
        }

        // Dodge or GFI: player falls
        $fallPos = $pending->getRollType() === 'gfi'
            ? new Position($pending->getTargetX(), $pending->getTargetY())
            : $player->getPosition();

        $events[] = GameEvent::playerFell($pending->getPlayerId());
        $events[] = GameEvent::turnover(
            $pending->getRollType() === 'gfi' ? 'Failed Going For It' : 'Failed dodge',
        );

        $fallenPlayer = $player
            ->withState(PlayerState::PRONE)
            ->withPosition($fallPos)
            ->withHasMoved(true)
            ->withHasActed(true)
            ->withMovementRemaining(0);
        $state = $state->withPlayer($fallenPlayer);

        [$state, $events] = $this->ballResolver->handleBallOnPlayerDown($state, $fallenPlayer, $events);

        return ActionResult::turnover($state->withTurnoverPending(true), $events);
    }

    private function makeRollEvent(PendingRerollDTO $pending, int $roll, bool $success): GameEvent
    {
        return match ($pending->getRollType()) {
            'dodge' => GameEvent::dodgeAttempt($pending->getPlayerId(), $pending->getTarget(), $roll, $success),
            'gfi' => GameEvent::gfiAttempt($pending->getPlayerId(), $roll, $success),
            default => GameEvent::ballPickup($pending->getPlayerId(), $pending->getTarget(), $roll, $success),
        };
    }
}
