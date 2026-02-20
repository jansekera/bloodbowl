<?php
declare(strict_types=1);

namespace App\Engine\Action;

use App\DTO\ActionResult;
use App\DTO\BallState;
use App\DTO\GameEvent;
use App\DTO\GameState;
use App\DTO\MatchPlayerDTO;
use App\Enum\BlockDiceFace;
use App\Enum\PlayerState;
use App\Enum\SkillName;
use App\ValueObject\Position;
use App\Engine\BallResolver;
use App\Engine\DiceRollerInterface;
use App\Engine\InjuryResolver;
use App\Engine\ScatterCalculator;
use App\Engine\StrengthCalculator;
use App\Engine\TacklezoneCalculator;

final class BallAndChainHandler implements ActionHandlerInterface
{
    public function __construct(
        private readonly DiceRollerInterface $dice,
        private readonly StrengthCalculator $strCalc,
        private readonly TacklezoneCalculator $tzCalc,
        private readonly InjuryResolver $injuryResolver,
        private readonly BallResolver $ballResolver,
        private readonly ScatterCalculator $scatterCalc,
    ) {
    }

    /**
     * @param array<string, mixed> $params {playerId}
     */
    public function resolve(GameState $state, array $params): ActionResult
    {
        $playerId = (int) $params['playerId'];
        $player = $state->getPlayer($playerId);

        if ($player === null) {
            throw new \InvalidArgumentException('Player not found');
        }
        if (!$player->hasSkill(SkillName::BallAndChain)) {
            throw new \InvalidArgumentException('Player must have Ball & Chain skill');
        }

        $events = [];
        $ma = $player->getStats()->getMovement();

        for ($step = 0; $step < $ma; $step++) {
            $currentPos = $player->getPosition();
            if ($currentPos === null) {
                break; // Player was KO'd off pitch
            }

            // Roll D8 for scatter direction
            $direction = $this->dice->rollD8();
            $newPos = $this->scatterCalc->scatterOnce($currentPos, $direction);

            // Off pitch: player is KO'd
            if (!$newPos->isOnPitch()) {
                $events[] = GameEvent::ballAndChainMove($playerId, (string) $currentPos, 'off-pitch', $direction);

                // Drop ball if carried
                if ($state->getBall()->getCarrierId() === $playerId) {
                    $state = $state->withBall(BallState::onGround($currentPos));
                    $bounceResult = $this->ballResolver->resolveBounce($state, $currentPos);
                    $events = array_merge($events, $bounceResult['events']);
                    $state = $bounceResult['state'];
                }

                $player = $player->withPosition(null)->withState(PlayerState::KO);
                $state = $state->withPlayer($player);
                $events[] = GameEvent::crowdSurf($playerId);
                break;
            }

            // Check for occupant
            $occupant = $state->getPlayerAtPosition($newPos);
            if ($occupant !== null) {
                // Auto-block the occupant
                $events[] = GameEvent::ballAndChainMove($playerId, (string) $currentPos, (string) $newPos, $direction);
                $events[] = GameEvent::ballAndChainBlock($playerId, $occupant->getId());

                // Move to the square first
                $player = $player->withPosition($newPos);
                $state = $state->withPlayer($player);

                // Resolve 1-die block (using B&C player's ST vs occupant's ST)
                [$state, $events, $player] = $this->resolveAutoBlock($state, $player, $occupant, $events);

                // If player was knocked down during auto-block, stop movement
                $player = $state->getPlayer($playerId);
                if ($player === null || $player->getState() !== PlayerState::STANDING) {
                    break;
                }
            } else {
                // Move to empty square
                $events[] = GameEvent::ballAndChainMove($playerId, (string) $currentPos, (string) $newPos, $direction);
                $player = $player->withPosition($newPos);
                $state = $state->withPlayer($player);

                // Ball & Chain player cannot pick up the ball — ball bounces
                $ball = $state->getBall();
                if (!$ball->isHeld() && $ball->getPosition() !== null && $ball->getPosition()->equals($newPos)) {
                    $bounceResult = $this->ballResolver->resolveBounce($state, $newPos);
                    $events = array_merge($events, $bounceResult['events']);
                    $state = $bounceResult['state'];
                }
            }
        }

        // Mark as acted
        $player = $state->getPlayer($playerId);
        if ($player !== null) {
            $state = $state->withPlayer($player->withHasActed(true)->withHasMoved(true));
        }

        return ActionResult::success($state, $events);
    }

    /**
     * Resolve automatic 1-die block for Ball & Chain.
     *
     * @param list<GameEvent> $events
     * @return array{0: GameState, 1: list<GameEvent>, 2: MatchPlayerDTO}
     */
    private function resolveAutoBlock(
        GameState $state,
        MatchPlayerDTO $bncPlayer,
        MatchPlayerDTO $target,
        array $events,
    ): array {
        $bncPos = $bncPlayer->getPosition();
        $targetPos = $target->getPosition();

        if ($bncPos === null || $targetPos === null) {
            return [$state, $events, $bncPlayer];
        }

        // 1-die block
        $roll = $this->dice->rollD6();
        $face = match ($roll) {
            1 => BlockDiceFace::ATTACKER_DOWN,
            2 => BlockDiceFace::BOTH_DOWN,
            3, 4 => BlockDiceFace::PUSHED,
            5 => BlockDiceFace::DEFENDER_STUMBLES,
            default => BlockDiceFace::DEFENDER_DOWN,
        };

        $faceValues = [$face->value];
        $events[] = GameEvent::blockAttempt(
            $bncPlayer->getId(), $target->getId(), 1, true, $faceValues, $face->value,
        );

        // Simplified block resolution for auto-block
        switch ($face) {
            case BlockDiceFace::ATTACKER_DOWN:
                // B&C player knocked down
                $events[] = GameEvent::playerFell($bncPlayer->getId());
                $bncPlayer = $bncPlayer->withState(PlayerState::PRONE);
                $state = $state->withPlayer($bncPlayer);

                $injResult = $this->injuryResolver->resolve($bncPlayer, $this->dice);
                $bncPlayer = $injResult['player'];
                $state = $state->withPlayer($bncPlayer);
                $events = array_merge($events, $injResult['events']);

                [$state, $events] = $this->ballResolver->handleBallOnPlayerDown($state, $bncPlayer, $events);
                break;

            case BlockDiceFace::BOTH_DOWN:
                // Both go down unless they have Block
                if (!$bncPlayer->hasSkill(SkillName::Block)) {
                    $events[] = GameEvent::playerFell($bncPlayer->getId());
                    $bncPlayer = $bncPlayer->withState(PlayerState::PRONE);
                    $state = $state->withPlayer($bncPlayer);
                    $injResult = $this->injuryResolver->resolve($bncPlayer, $this->dice);
                    $bncPlayer = $injResult['player'];
                    $state = $state->withPlayer($bncPlayer);
                    $events = array_merge($events, $injResult['events']);
                    [$state, $events] = $this->ballResolver->handleBallOnPlayerDown($state, $bncPlayer, $events);
                }
                if (!$target->hasSkill(SkillName::Block)) {
                    $events[] = GameEvent::playerFell($target->getId());
                    $target = $target->withState(PlayerState::PRONE);
                    $state = $state->withPlayer($target);
                    $injResult = $this->injuryResolver->resolve($target, $this->dice);
                    $target = $injResult['player'];
                    $state = $state->withPlayer($target);
                    $events = array_merge($events, $injResult['events']);
                    [$state, $events] = $this->ballResolver->handleBallOnPlayerDown($state, $target, $events);
                }
                break;

            case BlockDiceFace::PUSHED:
                // Push target away (simple: find first empty adjacent or crowd surf)
                [$state, $events] = $this->resolvePush($state, $bncPlayer, $target, $events);
                break;

            case BlockDiceFace::DEFENDER_STUMBLES:
                // Push + knockdown (unless Dodge without Tackle)
                $knockdown = !$target->hasSkill(SkillName::Dodge) || $bncPlayer->hasSkill(SkillName::Tackle);
                [$state, $events] = $this->resolvePush($state, $bncPlayer, $target, $events);
                if ($knockdown) {
                    $target = $state->getPlayer($target->getId());
                    if ($target !== null && $target->getPosition() !== null) {
                        $events[] = GameEvent::playerFell($target->getId());
                        $target = $target->withState(PlayerState::PRONE);
                        $state = $state->withPlayer($target);
                        $injResult = $this->injuryResolver->resolve($target, $this->dice);
                        $target = $injResult['player'];
                        $state = $state->withPlayer($target);
                        $events = array_merge($events, $injResult['events']);
                        [$state, $events] = $this->ballResolver->handleBallOnPlayerDown($state, $target, $events);
                    }
                }
                break;

            case BlockDiceFace::DEFENDER_DOWN:
            case BlockDiceFace::POW:
                // Push + knockdown
                [$state, $events] = $this->resolvePush($state, $bncPlayer, $target, $events);
                $target = $state->getPlayer($target->getId());
                if ($target !== null && $target->getPosition() !== null) {
                    $events[] = GameEvent::playerFell($target->getId());
                    $target = $target->withState(PlayerState::PRONE);
                    $state = $state->withPlayer($target);
                    $injResult = $this->injuryResolver->resolve($target, $this->dice);
                    $target = $injResult['player'];
                    $state = $state->withPlayer($target);
                    $events = array_merge($events, $injResult['events']);
                    [$state, $events] = $this->ballResolver->handleBallOnPlayerDown($state, $target, $events);
                }
                break;
        }

        return [$state, $events, $bncPlayer];
    }

    /**
     * Simple push: find empty adjacent square away from B&C player.
     *
     * @param list<GameEvent> $events
     * @return array{0: GameState, 1: list<GameEvent>}
     */
    private function resolvePush(
        GameState $state,
        MatchPlayerDTO $pusher,
        MatchPlayerDTO $target,
        array $events,
    ): array {
        $pusherPos = $pusher->getPosition();
        $targetPos = $target->getPosition();

        if ($pusherPos === null || $targetPos === null) {
            return [$state, $events];
        }

        // Stand Firm prevents push
        if ($target->hasSkill(SkillName::StandFirm)) {
            return [$state, $events];
        }

        $dx = $targetPos->getX() - $pusherPos->getX();
        $dy = $targetPos->getY() - $pusherPos->getY();
        $ndx = $dx === 0 ? 0 : ($dx > 0 ? 1 : -1);
        $ndy = $dy === 0 ? 0 : ($dy > 0 ? 1 : -1);

        // Try direct, then diagonals
        $candidates = [
            new Position($targetPos->getX() + $ndx, $targetPos->getY() + $ndy),
        ];
        if ($ndx === 0) {
            $candidates[] = new Position($targetPos->getX() - 1, $targetPos->getY() + $ndy);
            $candidates[] = new Position($targetPos->getX() + 1, $targetPos->getY() + $ndy);
        } elseif ($ndy === 0) {
            $candidates[] = new Position($targetPos->getX() + $ndx, $targetPos->getY() - 1);
            $candidates[] = new Position($targetPos->getX() + $ndx, $targetPos->getY() + 1);
        } else {
            $candidates[] = new Position($targetPos->getX() + $ndx, $targetPos->getY());
            $candidates[] = new Position($targetPos->getX(), $targetPos->getY() + $ndy);
        }

        $pushTo = null;
        foreach ($candidates as $pos) {
            if (!$pos->isOnPitch()) {
                // Crowd surf
                $events[] = GameEvent::playerPushed($target->getId(), (string) $targetPos, 'off-pitch');
                $events[] = GameEvent::crowdSurf($target->getId());

                if ($state->getBall()->getCarrierId() === $target->getId()) {
                    $state = $state->withBall(BallState::onGround($targetPos));
                }

                $target = $target->withPosition(null);
                $state = $state->withPlayer($target);
                $injResult = $this->injuryResolver->resolveCrowdSurf($target, $this->dice);
                $target = $injResult['player'];
                $state = $state->withPlayer($target);
                $events = array_merge($events, $injResult['events']);
                return [$state, $events];
            }
            if ($state->getPlayerAtPosition($pos) === null) {
                $pushTo = $pos;
                break;
            }
        }

        if ($pushTo !== null) {
            $events[] = GameEvent::playerPushed($target->getId(), (string) $targetPos, (string) $pushTo);
            $target = $target->withPosition($pushTo);
            $state = $state->withPlayer($target);

            if ($state->getBall()->getCarrierId() === $target->getId()) {
                $state = $state->withBall(BallState::carried($pushTo, $target->getId()));
            }
        }

        return [$state, $events];
    }
}
