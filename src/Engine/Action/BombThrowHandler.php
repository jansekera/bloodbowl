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

final class BombThrowHandler implements ActionHandlerInterface
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
     * @param array<string, mixed> $params {playerId, targetX, targetY}
     */
    public function resolve(GameState $state, array $params): ActionResult
    {
        $throwerId = (int) $params['playerId'];
        $targetX = (int) $params['targetX'];
        $targetY = (int) $params['targetY'];
        $targetPos = new Position($targetX, $targetY);

        $thrower = $state->getPlayer($throwerId);
        if ($thrower === null) {
            throw new \InvalidArgumentException('Player not found');
        }
        if (!$thrower->hasSkill(SkillName::Bombardier)) {
            throw new \InvalidArgumentException('Player must have Bombardier skill');
        }

        $throwerPos = $thrower->getPosition();
        if ($throwerPos === null) {
            throw new \InvalidArgumentException('Thrower must be on pitch');
        }

        // Mark pass used (shares slot with pass)
        $activeSide = $thrower->getTeamSide();
        $teamState = $state->getTeamState($activeSide);
        $state = $state->withTeamState($activeSide, $teamState->withPassUsed());

        // Mark thrower as acted
        $state = $state->withPlayer($thrower->withHasActed(true)->withHasMoved(true));

        $events = [];

        // Calculate accuracy (same formula as TTM/pass)
        $distance = $throwerPos->distanceTo($targetPos);
        $range = PassRange::fromDistance($distance);
        if ($range === null) {
            throw new \InvalidArgumentException('Target is out of range');
        }

        $ag = $thrower->getStats()->getAgility();
        $tz = $thrower->hasSkill(SkillName::NervesOfSteel)
            ? 0
            : $this->tzCalc->countTacklezones($state, $throwerPos, $thrower->getTeamSide());
        $accuracyTarget = max(2, min(6, 7 - $ag + $tz - $range->modifier()));

        $roll = $this->dice->rollD6();
        $fumble = $roll === 1;
        $accurate = !$fumble && $roll >= $accuracyTarget;

        $resultStr = $fumble ? 'fumble' : ($accurate ? 'accurate' : 'inaccurate');
        $events[] = GameEvent::bombThrow($throwerId, $roll, $resultStr);

        if ($fumble) {
            // Scatter 1 square from thrower
            $direction = $this->dice->rollD8();
            $landingPos = $this->scatterCalc->scatterOnce($throwerPos, $direction);
        } elseif ($accurate) {
            $landingPos = $targetPos;
        } else {
            // Inaccurate: scatter 3 times from target
            $landingPos = $targetPos;
            for ($i = 0; $i < 3; $i++) {
                $direction = $this->dice->rollD8();
                $landingPos = $this->scatterCalc->scatterOnce($landingPos, $direction);
            }
        }

        // If landing is off-pitch, bomb fizzles — no effect
        if (!$landingPos->isOnPitch()) {
            return ActionResult::success($state, $events);
        }

        $events[] = GameEvent::bombLanding((string) $landingPos);

        // Explosion: all players in 3x3 area around landing square get knocked down + armor roll
        [$state, $events] = $this->resolveExplosion($state, $landingPos, $throwerId, $events);

        // Bomb never causes turnover
        return ActionResult::success($state, $events);
    }

    /**
     * @param list<GameEvent> $events
     * @return array{0: GameState, 1: list<GameEvent>}
     */
    private function resolveExplosion(
        GameState $state,
        Position $center,
        int $throwerId,
        array $events,
    ): array {
        for ($dx = -1; $dx <= 1; $dx++) {
            for ($dy = -1; $dy <= 1; $dy++) {
                $pos = new Position($center->getX() + $dx, $center->getY() + $dy);
                if (!$pos->isOnPitch()) {
                    continue;
                }

                $player = $state->getPlayerAtPosition($pos);
                if ($player === null) {
                    continue;
                }

                // Thrower is not affected by own bomb
                if ($player->getId() === $throwerId) {
                    continue;
                }

                // Only standing players are knocked down
                if ($player->getState() !== PlayerState::STANDING) {
                    continue;
                }

                $events[] = GameEvent::bombExplosion($player->getId());

                // Knocked down + armor roll
                $player = $player->withState(PlayerState::PRONE);
                $state = $state->withPlayer($player);

                $injResult = $this->injuryResolver->resolve($player, $this->dice);
                $player = $injResult['player'];
                $state = $state->withPlayer($player);
                $events = array_merge($events, $injResult['events']);

                // Ball drops if carrier
                [$state, $events] = $this->ballResolver->handleBallOnPlayerDown($state, $player, $events);
            }
        }

        return [$state, $events];
    }
}
