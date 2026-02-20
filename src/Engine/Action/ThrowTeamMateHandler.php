<?php
declare(strict_types=1);

namespace App\Engine\Action;

use App\DTO\ActionResult;
use App\DTO\BallState;
use App\DTO\GameEvent;
use App\DTO\GameState;
use App\DTO\MatchPlayerDTO;
use App\Enum\PassRange;
use App\Enum\PlayerState;
use App\Enum\SkillName;
use App\ValueObject\Position;
use App\Engine\BallResolver;
use App\Engine\DiceRollerInterface;
use App\Engine\InjuryResolver;
use App\Engine\ScatterCalculator;
use App\Engine\TacklezoneCalculator;

final class ThrowTeamMateHandler implements ActionHandlerInterface
{
    public function __construct(
        private readonly DiceRollerInterface $dice,
        private readonly TacklezoneCalculator $tzCalc,
        private readonly ScatterCalculator $scatterCalc,
        private readonly InjuryResolver $injuryResolver,
        private readonly BallResolver $ballResolver,
    ) {
    }

    /**
     * @param array<string, mixed> $params {playerId, targetId, targetX, targetY}
     */
    public function resolve(GameState $state, array $params): ActionResult
    {
        $throwerId = (int) $params['playerId'];
        $targetId = (int) $params['targetId'];
        $targetX = (int) $params['targetX'];
        $targetY = (int) $params['targetY'];
        $landingTarget = new Position($targetX, $targetY);

        $thrower = $state->getPlayer($throwerId);
        $projectile = $state->getPlayer($targetId);

        if ($thrower === null || $projectile === null) {
            throw new \InvalidArgumentException('Player not found');
        }

        // Validate
        if (!$thrower->hasSkill(SkillName::ThrowTeamMate)) {
            throw new \InvalidArgumentException('Thrower must have Throw Team-Mate skill');
        }
        if (!$projectile->hasSkill(SkillName::RightStuff)) {
            throw new \InvalidArgumentException('Target must have Right Stuff skill');
        }

        $throwerPos = $thrower->getPosition();
        $projectilePos = $projectile->getPosition();

        if ($throwerPos === null || $projectilePos === null) {
            throw new \InvalidArgumentException('Players must be on pitch');
        }
        if ($throwerPos->distanceTo($projectilePos) !== 1) {
            throw new \InvalidArgumentException('Players must be adjacent');
        }

        // Mark pass used (shares slot with pass)
        $activeSide = $thrower->getTeamSide();
        $teamState = $state->getTeamState($activeSide);
        $state = $state->withTeamState($activeSide, $teamState->withPassUsed());

        // Mark thrower as acted
        $state = $state->withPlayer($thrower->withHasActed(true)->withHasMoved(true));

        $events = [];

        // Always Hungry: roll D6, on 1 = eat thrown player (removed, Injured)
        if ($thrower->hasSkill(SkillName::AlwaysHungry)) {
            $hungryRoll = $this->dice->rollD6();
            $eaten = $hungryRoll === 1;

            // Try team reroll on failure
            if ($eaten) {
                $teamState = $state->getTeamState($activeSide);
                if ($teamState->canUseReroll()) {
                    $state = $state->withTeamState($activeSide, $teamState->withRerollUsed());
                    $lonerBlocked = false;
                    if ($thrower->hasSkill(SkillName::Loner)) {
                        $lonerRoll = $this->dice->rollD6();
                        $lonerBlocked = $lonerRoll < 4;
                        $events[] = GameEvent::lonerCheck($thrower->getId(), $lonerRoll, !$lonerBlocked);
                    }
                    if (!$lonerBlocked) {
                        $hungryRoll = $this->dice->rollD6();
                        $eaten = $hungryRoll === 1;
                        $events[] = GameEvent::rerollUsed($thrower->getId(), 'Team Reroll');
                    }
                }
            }

            $events[] = GameEvent::alwaysHungry($thrower->getId(), $targetId, $hungryRoll, $eaten);

            if ($eaten) {
                // Projectile is removed (Injured)
                $projectile = $projectile->withState(PlayerState::INJURED)->withPosition(null);
                $state = $state->withPlayer($projectile);

                // Drop ball if carried
                if ($state->getBall()->getCarrierId() === $targetId) {
                    $throwerPos = $thrower->getPosition();
                    if ($throwerPos !== null) {
                        $state = $state->withBall(BallState::onGround($throwerPos));
                        $bounceResult = $this->ballResolver->resolveBounce($state, $throwerPos);
                        $events = array_merge($events, $bounceResult['events']);
                        $state = $bounceResult['state'];
                    }
                }

                // Not a turnover — action just fails
                return ActionResult::success($state, $events);
            }
        }

        // Calculate range and accuracy
        $distance = $throwerPos->distanceTo($landingTarget);
        $range = PassRange::fromDistance($distance);
        if ($range === null) {
            throw new \InvalidArgumentException('Target is out of range');
        }

        // Accuracy roll: 7 - AG + TZ + range modifier (always fumble on 1)
        $ag = $thrower->getStats()->getAgility();
        $tz = $thrower->hasSkill(SkillName::NervesOfSteel)
            ? 0
            : $this->tzCalc->countTacklezones($state, $throwerPos, $thrower->getTeamSide());
        $accuracyTarget = max(2, min(6, 7 - $ag + $tz - $range->modifier()));

        $roll = $this->dice->rollD6();
        $fumble = $roll === 1;
        $accurate = !$fumble && $roll >= $accuracyTarget;

        $resultStr = $fumble ? 'fumble' : ($accurate ? 'accurate' : 'inaccurate');
        $events[] = GameEvent::throwTeamMate($throwerId, $targetId, $roll, $resultStr);

        // Remove projectile from pitch temporarily
        $projectileHadBall = $state->getBall()->getCarrierId() === $targetId;

        if ($fumble) {
            // Scatter 1 square from thrower
            $direction = $this->dice->rollD8();
            $scatterPos = $this->scatterCalc->scatterOnce($throwerPos, $direction);
            return $this->resolveLanding($state, $projectile, $scatterPos, $projectileHadBall, $events);
        }

        if ($accurate) {
            return $this->resolveLanding($state, $projectile, $landingTarget, $projectileHadBall, $events);
        }

        // Inaccurate: scatter from target
        $direction = $this->dice->rollD8();
        $scatterPos = $this->scatterCalc->scatterOnce($landingTarget, $direction);
        return $this->resolveLanding($state, $projectile, $scatterPos, $projectileHadBall, $events);
    }

    /**
     * @param list<GameEvent> $events
     */
    private function resolveLanding(
        GameState $state,
        MatchPlayerDTO $projectile,
        Position $landingPos,
        bool $hadBall,
        array $events,
    ): ActionResult {
        // Off pitch → crowd surf
        if (!$landingPos->isOnPitch()) {
            $events[] = GameEvent::crowdSurf($projectile->getId());
            $projectile = $projectile->withPosition(null);
            $state = $state->withPlayer($projectile);

            if ($hadBall) {
                $state = $state->withBall(BallState::offPitch());
            }

            $injResult = $this->injuryResolver->resolveCrowdSurf($projectile, $this->dice);
            $projectile = $injResult['player'];
            $state = $state->withPlayer($projectile);
            $events = array_merge($events, $injResult['events']);

            $events[] = GameEvent::turnover('Thrown player off pitch');
            return ActionResult::turnover($state->withTurnoverPending(true), $events);
        }

        // Occupied square → scatter further
        $occupant = $state->getPlayerAtPosition($landingPos);
        if ($occupant !== null && $occupant->getId() !== $projectile->getId()) {
            $direction = $this->dice->rollD8();
            $newPos = $this->scatterCalc->scatterOnce($landingPos, $direction);
            return $this->resolveLanding($state, $projectile, $newPos, $hadBall, $events);
        }

        // Place player at landing position
        $projectile = $projectile->withPosition($landingPos)->withHasMoved(true)->withHasActed(true);
        $state = $state->withPlayer($projectile);

        // Move ball with player if carried
        if ($hadBall) {
            $state = $state->withBall(BallState::carried($landingPos, $projectile->getId()));
        }

        // Landing roll: 7 - AG + TZ (2+ to 6+)
        $ag = $projectile->getStats()->getAgility();
        $tz = $this->tzCalc->countTacklezones($state, $landingPos, $projectile->getTeamSide());
        $landingTarget = max(2, min(6, 7 - $ag + $tz));

        $landingRoll = $this->dice->rollD6();
        $landingSuccess = $landingRoll >= $landingTarget;
        $events[] = GameEvent::ttmLanding($projectile->getId(), $landingRoll, $landingSuccess);

        if (!$landingSuccess) {
            // Failed landing: prone + armor roll
            $projectile = $projectile->withState(PlayerState::PRONE);
            $state = $state->withPlayer($projectile);

            $injResult = $this->injuryResolver->resolve($projectile, $this->dice);
            $projectile = $injResult['player'];
            $state = $state->withPlayer($projectile);
            $events = array_merge($events, $injResult['events']);

            // Ball drops if carried
            if ($hadBall) {
                $state = $state->withBall(BallState::onGround($landingPos));
                $bounceResult = $this->ballResolver->resolveBounce($state, $landingPos);
                $events = array_merge($events, $bounceResult['events']);
                $state = $bounceResult['state'];
            }
        }

        return ActionResult::success($state, $events);
    }
}
