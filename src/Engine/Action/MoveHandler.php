<?php

declare(strict_types=1);

namespace App\Engine\Action;

use App\DTO\ActionResult;
use App\DTO\BallState;
use App\DTO\GameEvent;
use App\DTO\GameState;
use App\DTO\MatchPlayerDTO;
use App\Enum\PlayerState;
use App\Enum\SkillName;
use App\Enum\Weather;
use App\ValueObject\Position;
use App\Engine\BallResolver;
use App\Engine\DiceRollerInterface;
use App\Engine\Pathfinder;
use App\Engine\TacklezoneCalculator;

final class MoveHandler implements ActionHandlerInterface
{
    public function __construct(
        private readonly DiceRollerInterface $dice,
        private readonly TacklezoneCalculator $tzCalc,
        private readonly Pathfinder $pathfinder,
        private readonly BallResolver $ballResolver,
    ) {
    }

    /**
     * @param array<string, mixed> $params
     */
    public function resolve(GameState $state, array $params): ActionResult
    {
        $playerId = (int) $params['playerId'];
        $targetX = (int) $params['x'];
        $targetY = (int) $params['y'];
        $destination = new Position($targetX, $targetY);

        $player = $state->getPlayer($playerId);
        if ($player === null) {
            throw new \InvalidArgumentException("Player {$playerId} not found");
        }

        $events = [];

        // Handle PRONE player stand-up
        if ($player->getState() === PlayerState::PRONE) {
            $standUpResult = $this->resolveStandUp($state, $player);
            $events = array_merge($events, $standUpResult['events']);
            $state = $standUpResult['state'];

            if (!$standUpResult['success']) {
                // Failed stand-up: player stays prone, action done (not a turnover)
                return ActionResult::success($state, $events);
            }

            $player = $state->getPlayer($playerId);
            if ($player === null) {
                throw new \InvalidArgumentException("Player {$playerId} not found after stand-up");
            }

            // Stand in place only
            if ($player->getPosition() !== null && $player->getPosition()->equals($destination)) {
                $state = $state->withPlayer($player->withHasMoved(true));
                return ActionResult::success($state, $events);
            }
        }

        $path = $this->pathfinder->findPathTo($state, $player, $destination);
        if ($path === null) {
            throw new \InvalidArgumentException("No valid path to ({$targetX},{$targetY})");
        }

        $currentState = $state;
        $currentPlayer = $player;

        // Track team reroll availability
        $activeSide = $currentState->getActiveTeam();
        $teamRerollUsed = false;

        foreach ($path->getSteps() as $step) {
            $from = $currentPlayer->getPosition();
            $to = $step->getPosition();

            // Leap: special AG roll, skip dodge and normal movement
            if ($step->isLeap()) {
                $leapTarget = $step->getDodgeTarget();
                // Very Long Legs: -1 to leap target
                if ($currentPlayer->hasSkill(SkillName::VeryLongLegs)) {
                    $leapTarget = max(2, $leapTarget - 1);
                }
                $roll = $this->dice->rollD6();
                $success = $roll >= $leapTarget;
                $events[] = GameEvent::leap($playerId, $leapTarget, $roll, $success);

                if (!$success) {
                    // Failed leap: same as failed dodge
                    $events[] = GameEvent::playerFell($playerId);
                    $events[] = GameEvent::turnover('Failed leap');

                    $fallenPlayer = $currentPlayer
                        ->withState(PlayerState::PRONE)
                        ->withPosition($to)
                        ->withHasMoved(true)
                        ->withHasActed(true)
                        ->withMovementRemaining(0);
                    $currentState = $currentState->withPlayer($fallenPlayer);

                    [$currentState, $events] = $this->ballResolver->handleBallOnPlayerDown($currentState, $fallenPlayer, $events);

                    return ActionResult::turnover(
                        $currentState->withTurnoverPending(true),
                        $events,
                    );
                }

                // Successful leap: move player
                $fromStr = $from !== null ? (string) $from : '?';
                $events[] = GameEvent::playerMove($playerId, $fromStr, (string) $to);

                $currentPlayer = $currentPlayer
                    ->withPosition($to)
                    ->withMovementRemaining($currentPlayer->getMovementRemaining() - 2);
                $currentState = $currentState->withPlayer($currentPlayer);

                if ($currentState->getBall()->getCarrierId() === $playerId) {
                    $currentState = $currentState->withBall(BallState::carried($to, $playerId));
                }

                // Check pickup on landing
                $ball = $currentState->getBall();
                if (!$ball->isHeld() && $ball->getPosition() !== null && $ball->getPosition()->equals($to)) {
                    $currentPlayer = $currentState->getPlayer($playerId) ?? $currentPlayer;
                    $pickupRerollAvailable = !$teamRerollUsed && $currentState->getTeamState($activeSide)->canUseReroll();
                    $pickupResult = $this->ballResolver->resolvePickup($currentState, $currentPlayer, $pickupRerollAvailable);
                    $events = array_merge($events, $pickupResult['events']);
                    $currentState = $pickupResult['state'];

                    if ($pickupResult['teamRerollUsed']) {
                        $teamRerollUsed = true;
                        $currentState = $currentState->withTeamState(
                            $activeSide,
                            $currentState->getTeamState($activeSide)->withRerollUsed(),
                        );
                    }

                    if (!$pickupResult['success']) {
                        $events[] = GameEvent::turnover('Failed pickup');
                        $fallenPlayer = ($currentState->getPlayer($playerId) ?? $currentPlayer)
                            ->withHasMoved(true)
                            ->withHasActed(true);
                        $currentState = $currentState->withPlayer($fallenPlayer);

                        return ActionResult::turnover(
                            $currentState->withTurnoverPending(true),
                            $events,
                        );
                    }

                    $currentPlayer = $currentState->getPlayer($playerId) ?? $currentPlayer;
                }

                continue; // skip normal dodge/GFI for this step
            }

            // Tentacles check before dodge
            if ($step->requiresDodge() && $from !== null) {
                foreach ($this->tzCalc->getMarkingPlayers($currentState, $from, $currentPlayer->getTeamSide()) as $marker) {
                    if (!$marker->hasSkill(SkillName::Tentacles)) {
                        continue;
                    }
                    $moverRoll = $this->dice->rollD6();
                    $tentRoll = $this->dice->rollD6();
                    $escaped = ($moverRoll + $currentPlayer->getStats()->getStrength()) > ($tentRoll + $marker->getStats()->getStrength());
                    $events[] = GameEvent::tentacles($playerId, $marker->getId(), $moverRoll, $tentRoll, $escaped);
                    if (!$escaped) {
                        // Movement ends, NOT a turnover
                        $currentPlayer = $currentPlayer->withHasMoved(true);
                        $currentState = $currentState->withPlayer($currentPlayer);
                        return ActionResult::success($currentState, $events);
                    }
                }
            }

            // Dodge roll
            if ($step->requiresDodge()) {
                $roll = $this->dice->rollD6();
                $target = $step->getDodgeTarget();
                $success = $roll >= $target;
                $events[] = GameEvent::dodgeAttempt($playerId, $target, $roll, $success);

                if (!$success) {
                    // Try Dodge skill reroll (negated by adjacent enemy with Tackle)
                    $skillRerollUsed = false;
                    if ($currentPlayer->hasSkill(SkillName::Dodge)) {
                        $tackleNegatesDodge = false;
                        if ($from !== null) {
                            foreach ($this->tzCalc->getMarkingPlayers($currentState, $from, $currentPlayer->getTeamSide()) as $marker) {
                                if ($marker->hasSkill(SkillName::Tackle)) {
                                    $tackleNegatesDodge = true;
                                    break;
                                }
                            }
                        }

                        if (!$tackleNegatesDodge) {
                            $skillRerollUsed = true;
                            $roll = $this->dice->rollD6();
                            $success = $roll >= $target;
                            $events[] = GameEvent::rerollUsed($playerId, 'Dodge');
                            $events[] = GameEvent::dodgeAttempt($playerId, $target, $roll, $success);
                        }
                    }

                    // Pro reroll (after skill reroll, before team reroll)
                    if (!$success && $currentPlayer->hasSkill(SkillName::Pro) && !$currentPlayer->isProUsedThisTurn()) {
                        $proRoll = $this->dice->rollD6();
                        $currentPlayer = $currentPlayer->withProUsedThisTurn(true);
                        $currentState = $currentState->withPlayer($currentPlayer);
                        if ($proRoll >= 4) {
                            $roll = $this->dice->rollD6();
                            $success = $roll >= $target;
                            $events[] = GameEvent::proReroll($playerId, $proRoll, true, $roll);
                            $events[] = GameEvent::dodgeAttempt($playerId, $target, $roll, $success);
                        } else {
                            $events[] = GameEvent::proReroll($playerId, $proRoll, false, null);
                        }
                    }

                    // Try team reroll (only if no skill reroll was used)
                    if (!$success && !$skillRerollUsed && !$teamRerollUsed && $currentState->getTeamState($activeSide)->canUseReroll()) {
                        $teamRerollUsed = true;
                        $currentState = $currentState->withTeamState(
                            $activeSide,
                            $currentState->getTeamState($activeSide)->withRerollUsed(),
                        );
                        $lonerBlocked = false;
                        if ($currentPlayer->hasSkill(SkillName::Loner)) {
                            $lonerRoll = $this->dice->rollD6();
                            $lonerBlocked = $lonerRoll < 4;
                            $events[] = GameEvent::lonerCheck($playerId, $lonerRoll, !$lonerBlocked);
                        }
                        if (!$lonerBlocked) {
                            $roll = $this->dice->rollD6();
                            $success = $roll >= $target;
                            $events[] = GameEvent::rerollUsed($playerId, 'Team Reroll');
                            $events[] = GameEvent::dodgeAttempt($playerId, $target, $roll, $success);
                        }
                    }
                }

                if (!$success) {
                    // Failed dodge - player falls, turnover
                    $events[] = GameEvent::playerFell($playerId);
                    $events[] = GameEvent::turnover('Failed dodge');

                    $fallenPlayer = $currentPlayer
                        ->withState(PlayerState::PRONE)
                        ->withHasMoved(true)
                        ->withHasActed(true)
                        ->withMovementRemaining(0);
                    $currentState = $currentState->withPlayer($fallenPlayer);

                    // Drop ball if carrier
                    [$currentState, $events] = $this->ballResolver->handleBallOnPlayerDown($currentState, $fallenPlayer, $events);

                    return ActionResult::turnover(
                        $currentState->withTurnoverPending(true),
                        $events,
                    );
                }

                // Diving Tackle: after successful dodge, DT player goes prone
                if ($from !== null) {
                    foreach ($this->tzCalc->getMarkingPlayers($currentState, $from, $currentPlayer->getTeamSide()) as $marker) {
                        if (!$marker->hasSkill(SkillName::DivingTackle)) {
                            continue;
                        }
                        $dtPlayer = $marker->withState(PlayerState::PRONE);
                        $currentState = $currentState->withPlayer($dtPlayer);
                        $events[] = GameEvent::divingTackle($marker->getId());
                        [$currentState, $events] = $this->ballResolver->handleBallOnPlayerDown($currentState, $dtPlayer, $events);
                        break;
                    }
                }

                // Shadowing: opponent with Shadowing may follow after successful dodge
                if ($from !== null) {
                    foreach ($this->tzCalc->getMarkingPlayers($currentState, $from, $currentPlayer->getTeamSide()) as $marker) {
                        if (!$marker->hasSkill(SkillName::Shadowing)) {
                            continue;
                        }
                        $shadowRoll = $this->dice->rollD6();
                        $shadowMA = $marker->getStats()->getMovement();
                        $moverMA = $currentPlayer->getStats()->getMovement();
                        $followed = ($shadowRoll + $shadowMA - $moverMA) >= 6;
                        $events[] = GameEvent::shadowing($marker->getId(), $currentPlayer->getId(), $shadowRoll, $followed);
                        if ($followed) {
                            $marker = $marker->withPosition($from);
                            $currentState = $currentState->withPlayer($marker);
                        }
                        break; // only one Shadowing attempt per dodge step
                    }
                }
            }

            // GFI roll
            if ($step->isGfi()) {
                $gfiThreshold = $currentState->getWeather() === Weather::BLIZZARD ? 3 : 2;
                $roll = $this->dice->rollD6();
                $success = $roll >= $gfiThreshold;
                $events[] = GameEvent::gfiAttempt($playerId, $roll, $success);

                // Sure Feet: reroll failed GFI
                $skillRerollUsedGfi = false;
                if (!$success && $currentPlayer->hasSkill(SkillName::SureFeet)) {
                    $skillRerollUsedGfi = true;
                    $roll = $this->dice->rollD6();
                    $success = $roll >= $gfiThreshold;
                    $events[] = GameEvent::rerollUsed($playerId, 'Sure Feet');
                    $events[] = GameEvent::gfiAttempt($playerId, $roll, $success);
                }

                // Pro reroll (after skill reroll, before team reroll)
                if (!$success && $currentPlayer->hasSkill(SkillName::Pro) && !$currentPlayer->isProUsedThisTurn()) {
                    $proRoll = $this->dice->rollD6();
                    $currentPlayer = $currentPlayer->withProUsedThisTurn(true);
                    $currentState = $currentState->withPlayer($currentPlayer);
                    if ($proRoll >= 4) {
                        $roll = $this->dice->rollD6();
                        $success = $roll >= $gfiThreshold;
                        $events[] = GameEvent::proReroll($playerId, $proRoll, true, $roll);
                        $events[] = GameEvent::gfiAttempt($playerId, $roll, $success);
                    } else {
                        $events[] = GameEvent::proReroll($playerId, $proRoll, false, null);
                    }
                }

                // Try team reroll (only if no skill reroll was used)
                if (!$success && !$skillRerollUsedGfi && !$teamRerollUsed && $currentState->getTeamState($activeSide)->canUseReroll()) {
                    $teamRerollUsed = true;
                    $currentState = $currentState->withTeamState(
                        $activeSide,
                        $currentState->getTeamState($activeSide)->withRerollUsed(),
                    );
                    $lonerBlocked = false;
                    if ($currentPlayer->hasSkill(SkillName::Loner)) {
                        $lonerRoll = $this->dice->rollD6();
                        $lonerBlocked = $lonerRoll < 4;
                        $events[] = GameEvent::lonerCheck($playerId, $lonerRoll, !$lonerBlocked);
                    }
                    if (!$lonerBlocked) {
                        $roll = $this->dice->rollD6();
                        $success = $roll >= $gfiThreshold;
                        $events[] = GameEvent::rerollUsed($playerId, 'Team Reroll');
                        $events[] = GameEvent::gfiAttempt($playerId, $roll, $success);
                    }
                }

                if (!$success) {
                    // Failed GFI - player falls, turnover
                    $events[] = GameEvent::playerFell($playerId);
                    $events[] = GameEvent::turnover('Failed Going For It');

                    $fallenPlayer = $currentPlayer
                        ->withState(PlayerState::PRONE)
                        ->withPosition($to) // falls at destination
                        ->withHasMoved(true)
                        ->withHasActed(true)
                        ->withMovementRemaining(0);
                    $currentState = $currentState->withPlayer($fallenPlayer);

                    // Drop ball if carrier
                    [$currentState, $events] = $this->ballResolver->handleBallOnPlayerDown($currentState, $fallenPlayer, $events);

                    return ActionResult::turnover(
                        $currentState->withTurnoverPending(true),
                        $events,
                    );
                }
            }

            // Move succeeded for this step
            $fromStr = $from !== null ? (string) $from : '?';
            $events[] = GameEvent::playerMove($playerId, $fromStr, (string) $to);

            $currentPlayer = $currentPlayer
                ->withPosition($to)
                ->withMovementRemaining($currentPlayer->getMovementRemaining() - 1);
            $currentState = $currentState->withPlayer($currentPlayer);

            // Move ball with carrier
            if ($currentState->getBall()->getCarrierId() === $playerId) {
                $currentState = $currentState->withBall(BallState::carried($to, $playerId));
            }

            // Check if player moved onto a loose ball
            $ball = $currentState->getBall();
            if (!$ball->isHeld() && $ball->getPosition() !== null && $ball->getPosition()->equals($to)) {
                $currentPlayer = $currentState->getPlayer($playerId) ?? $currentPlayer;
                $pickupRerollAvailable = !$teamRerollUsed && $currentState->getTeamState($activeSide)->canUseReroll();
                $pickupResult = $this->ballResolver->resolvePickup($currentState, $currentPlayer, $pickupRerollAvailable);
                $events = array_merge($events, $pickupResult['events']);
                $currentState = $pickupResult['state'];

                if ($pickupResult['teamRerollUsed']) {
                    $teamRerollUsed = true;
                    $currentState = $currentState->withTeamState(
                        $activeSide,
                        $currentState->getTeamState($activeSide)->withRerollUsed(),
                    );
                }

                if (!$pickupResult['success']) {
                    $events[] = GameEvent::turnover('Failed pickup');
                    $fallenPlayer = ($currentState->getPlayer($playerId) ?? $currentPlayer)
                        ->withHasMoved(true)
                        ->withHasActed(true);
                    $currentState = $currentState->withPlayer($fallenPlayer);

                    return ActionResult::turnover(
                        $currentState->withTurnoverPending(true),
                        $events,
                    );
                }

                $currentPlayer = $currentState->getPlayer($playerId) ?? $currentPlayer;
            }
        }

        // Mark player as moved
        $movedPlayer = $currentPlayer->withHasMoved(true);
        $currentState = $currentState->withPlayer($movedPlayer);

        return ActionResult::success($currentState, $events);
    }

    /**
     * Resolve stand-up for a prone player.
     * Jump Up: free stand-up (0 MA cost).
     * MA >= 3: automatic, costs 3 MA. MA < 3: roll 4+ required.
     *
     * @return array{state: GameState, events: list<GameEvent>, success: bool}
     */
    private function resolveStandUp(GameState $state, MatchPlayerDTO $player): array
    {
        $playerId = $player->getId();

        // Jump Up: stand up for free (0 MA cost)
        if ($player->hasSkill(SkillName::JumpUp)) {
            $events = [GameEvent::standUp($playerId)];
            $player = $player->withState(PlayerState::STANDING);
            $state = $state->withPlayer($player);
            return ['state' => $state, 'events' => $events, 'success' => true];
        }

        if ($player->getStats()->getMovement() < 3) {
            // Low MA: roll 4+ to stand
            $roll = $this->dice->rollD6();
            $success = $roll >= 4;
            $events = [GameEvent::standUp($playerId, $roll, $success)];

            if (!$success) {
                // Failed: player stays prone, action done
                $player = $player->withHasMoved(true)->withHasActed(true);
                $state = $state->withPlayer($player);
                return ['state' => $state, 'events' => $events, 'success' => false];
            }

            // Success: stand up with 0 MA remaining
            $player = $player
                ->withState(PlayerState::STANDING)
                ->withMovementRemaining(0);
            $state = $state->withPlayer($player);
            return ['state' => $state, 'events' => $events, 'success' => true];
        }

        // Normal stand-up: costs 3 MA
        $events = [GameEvent::standUp($playerId)];
        $player = $player
            ->withState(PlayerState::STANDING)
            ->withMovementRemaining($player->getMovementRemaining() - 3);
        $state = $state->withPlayer($player);

        return ['state' => $state, 'events' => $events, 'success' => true];
    }
}
