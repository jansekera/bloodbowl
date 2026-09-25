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
use App\Enum\TeamSide;
use App\ValueObject\Position;
use App\Engine\BallResolver;
use App\Engine\DiceRollerInterface;
use App\Engine\InjuryResolver;
use App\Engine\PassResolver;
use App\Engine\ScatterCalculator;
use App\Engine\TacklezoneCalculator;

final class BombThrowHandler implements ActionHandlerInterface
{
    private readonly PassResolver $passResolver;

    public function __construct(
        private readonly DiceRollerInterface $dice,
        TacklezoneCalculator $tzCalc,
        private readonly ScatterCalculator $scatterCalc,
        private readonly InjuryResolver $injuryResolver,
        private readonly BallResolver $ballResolver,
    ) {
        // Bomba se hazi "using the rules for throwing the ball" (r. Bombardier) --
        //   presnost, modifikatory a fumble bereme z prihravky.
        $this->passResolver = new PassResolver($dice, $tzCalc, $scatterCalc, $ballResolver);
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

        // ⛔ OPRAVA 25.09.2026 (balik E, E10) podle `rules_bb2016.txt` r. 7948-7975:
        //   bomba NESPOTREBUJE tymovou akci Pass; presnost a fumble jako u prihravky;
        //   fumble vybuchne v poli HAZECE; v cilovem poli srazi vzdy, vedle na 4+;
        //   zasahne i hazece a i lezici/omracene; turnover = fumble nebo srazeny
        //   hrac tymu na tahu. Chytani a zachyceni bomby engine nehraje -- to
        //   odpovida legalni volbe "declined" (bomba pak vybuchne).
        $activeSide = $thrower->getTeamSide();
        $state = $state->withPlayer($thrower->withHasActed(true)->withHasMoved(true));

        $events = [];

        $distance = $throwerPos->distanceTo($targetPos);
        $range = PassRange::fromDistance($distance);
        if ($range === null) {
            throw new \InvalidArgumentException('Target is out of range');
        }
        $accuracyTarget = $this->passResolver->getAccuracyTarget($state, $thrower, $range);
        $modifier = $this->passResolver->getPassRollModifier($state, $thrower, $range);
        $roll = $this->dice->rollD6();
        $fumble = $roll === 1 || $roll + $modifier <= 1;
        $accurate = !$fumble && $roll >= $accuracyTarget;
        $resultStr = $fumble ? 'fumble' : ($accurate ? 'accurate' : 'inaccurate');
        $events[] = GameEvent::bombThrow($throwerId, $roll, $resultStr);

        if ($fumble) {
            $landingPos = $throwerPos;
        } elseif ($accurate) {
            $landingPos = $targetPos;
        } else {
            // Nepresne: tri rozptyly od cile, jako u prihravky
            $landingPos = $targetPos;
            for ($i = 0; $i < 3; $i++) {
                $landingPos = $this->scatterCalc->scatterOnce($landingPos, $this->dice->rollD8());
            }
        }

        // Do davu: vybuchne bez ucinku
        if (!$landingPos->isOnPitch()) {
            return ActionResult::success($state, $events);
        }

        $events[] = GameEvent::bombLanding((string) $landingPos);
        [$state, $events, $srazenNas] = $this->resolveExplosion($state, $landingPos, $activeSide, $events);

        return ($fumble || $srazenNas)
            ? ActionResult::turnover($state, $events)
            : ActionResult::success($state, $events);
    }

    /**
     * V cilovem poli srazi vzdy, v sousednich na 4+; plati i pro hazece a pro
     * lezici a omracene ("treated as Knocked Down even if already Prone or
     * Stunned"). Vraci i to, jestli padl hrac tymu na tahu (= turnover).
     *
     * @param list<GameEvent> $events
     * @return array{0: GameState, 1: list<GameEvent>, 2: bool}
     */
    private function resolveExplosion(
        GameState $state,
        Position $center,
        TeamSide $activeSide,
        array $events,
    ): array {
        $srazenNas = false;
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
                $vCentru = $dx === 0 && $dy === 0;
                if (!$vCentru && $this->dice->rollD6() < 4) {
                    continue;
                }

                $events[] = GameEvent::bombExplosion($player->getId());
                if ($player->getTeamSide() === $activeSide) {
                    $srazenNas = true;
                }
                $player = $player->withState(
                    $player->getState() === PlayerState::STUNNED ? PlayerState::STUNNED : PlayerState::PRONE,
                );
                $state = $state->withPlayer($player);
                $injResult = $this->injuryResolver->resolve($player, $this->dice);
                $player = $injResult['player'];
                $state = $state->withPlayer($player);
                $events = array_merge($events, $injResult['events']);
                [$state, $events] = $this->ballResolver->handleBallOnPlayerDown($state, $player, $events);
            }
        }

        return [$state, $events, $srazenNas];
    }
}
