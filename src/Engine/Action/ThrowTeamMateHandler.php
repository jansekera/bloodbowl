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
                // `rules_bb2016.txt` r. 7786-7795: na 1 se ho POKUSI sezrat a hazi se
                //   znovu. Druha 1 = snezen: MRTVY bez lekarnika a Regeneration, mic se
                //   rozptyli jednou z JEHO pole. 2-6 = vysmekne se a hod je fumble.
                //   Turnover, kdyz mel mic (r. 382-384); jinak jako kazdy fumble.
                $druhyHod = $this->dice->rollD6();
                $snezen = $druhyHod === 1;
                $events[] = GameEvent::alwaysHungryEat($thrower->getId(), $targetId, $druhyHod, $snezen);
                $projectilePos = $projectile->getPosition();
                $meMic = $state->getBall()->getCarrierId() === $targetId;

                if (!$snezen) {
                    // Fumble: hozeny dopada na pole, kde stal (r. 8613).
                    $puvodni = $projectilePos ?? $throwerPos;
                    return $this->resolveLanding($state, $projectile, $puvodni, $meMic, $events, $puvodni);
                }

                $state = $state->withPlayer($projectile->withState(PlayerState::DEAD)->withPosition(null));
                if ($meMic && $projectilePos !== null) {
                    $state = $state->withBall(BallState::onGround($projectilePos));
                    $bounceResult = $this->ballResolver->resolveBounce($state, $projectilePos);
                    $events = array_merge($events, $bounceResult['events']);

                    return ActionResult::turnover($bounceResult['state'], $events);
                }

                return ActionResult::success($state, $events);
            }
        }

        // Calculate range and accuracy
        $distance = $throwerPos->distanceTo($landingTarget);
        $range = PassRange::fromDistance($distance);
        if ($range === null) {
            throw new \InvalidArgumentException('Target is out of range');
        }

        // ⛔ OPRAVA 21.09.2026 (1/3): `rules_bb2016.txt` r. 8609-8610 --
        //   "**Long Pass or Long Bomb range passes are not possible**."
        if ($range === PassRange::LONG_PASS || $range === PassRange::LONG_BOMB) {
            throw new \InvalidArgumentException('Throw Team-Mate: only quick or short pass range (r. 8609-8610)');
        }

        // ⛔ OPRAVA 21.09.2026 (2/3): r. 8607-8608 -- "The pass is worked out
        //   exactly the same as the player with Throw Team-Mate passing a ball,
        //   **except the player must subtract 1 from the D6 roll** when he
        //   passes the player." Ta -1 tady chybela.
        //   A protoze se hod resi "exactly the same", plati i r. 1742-1745:
        //   fumble je pri hodu 1 **nebo pri modifikovanem vysledku <= 1**.
        $ag = $thrower->getStats()->getAgility();
        $tz = $thrower->hasSkill(SkillName::NervesOfSteel)
            ? 0
            : $this->tzCalc->countTacklezones($state, $throwerPos, $thrower->getTeamSide());

        $modifikator = $range->modifier() - $tz - 1;
        $accuracyTarget = max(2, min(6, 7 - $ag - $modifikator));

        $roll = $this->dice->rollD6();
        $fumble = $roll === 1 || ($roll + $modifikator) <= 1;
        $accurate = !$fumble && $roll >= $accuracyTarget;

        $resultStr = $fumble ? 'fumble' : ($accurate ? 'accurate' : 'inaccurate');
        $events[] = GameEvent::throwTeamMate($throwerId, $targetId, $roll, $resultStr);

        // Remove projectile from pitch temporarily
        $projectileHadBall = $state->getBall()->getCarrierId() === $targetId;

        if ($fumble) {
            // ⛔ OPRAVA 21.09.2026 (3/3): r. 8613 -- "**A fumbled team-mate will
            //   land in the square he originally occupied.**" Do ted se
            //   rozptyloval o jedno pole od hazece.
            $puvodni = $projectile->getPosition() ?? $throwerPos;

            return $this->resolveLanding($state, $projectile, $puvodni, $projectileHadBall, $events, $puvodni);
        }

        // ⛔ OPRAVA 21.09.2026: `rules_bb2016.txt` r. 8609-8611 -- "In addition,
        //   **accurate passes are treated instead as inaccurate passes thus
        //   scattering the player three times** as players are heavier and
        //   harder to pass than a ball."
        //   Do ted: presny hod polozil hrace PRESNE na cil (zadny rozptyl)
        //   a nepresny rozptyloval jen JEDNOU.
        //   ⇒ Presny i nepresny konci stejne: tri rozptyly po jednom poli,
        //   stejne jako u nepresne prihravky (`PassResolver::scatterMissedPass`).
        [$scatterPos, $lastOnPitch] = $this->scatterThrownPlayer($landingTarget);

        return $this->resolveLanding($state, $projectile, $scatterPos, $projectileHadBall, $events, $lastOnPitch);
    }

    /**
     * Tri rozptyly po jednom poli (r. 8610-8611). Jakmile hozeny hrac opusti
     * hriste, dalsi rozptyly se uz nehazi -- dopada k davu a pripadny mic se
     * vhazuje od posledniho pole na hristi (r. 8614-8616 + 659-663).
     *
     * @return array{Position, Position} [kam dolet, posledni pole na hristi]
     */
    private function scatterThrownPlayer(Position $target): array
    {
        $pos = $target;
        for ($i = 0; $i < 3; $i++) {
            $next = $this->scatterCalc->scatterOnce($pos, $this->dice->rollD8());
            if (!$next->isOnPitch()) {
                return [$next, $pos];
            }
            $pos = $next;
        }

        return [$pos, $pos];
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
        ?Position $scatterOrigin = null,
    ): ActionResult {
        // Off pitch → crowd surf
        if (!$landingPos->isOnPitch()) {
            $events[] = GameEvent::crowdSurf($projectile->getId());
            $projectile = $projectile->withPosition(null);
            $state = $state->withPlayer($projectile);

            // ⛔ OPRAVA 21.09.2026 (`rules_bb2016.txt` r. 8614-8616 + 659-663):
            //   hozeny hrac u davu je „beaten up by the crowd **in the same
            //   manner as a player who has been pushed off the pitch**", a
            //   u vytlaceneho NOSICE plati: „the fans ... throw the ball back
            //   into play! The throw-in is centred on the last square the
            //   player was in before he was pushed off the pitch."
            //   Do ted tady mic ZMIZEL (`BallState::offPitch()`).
            $throwInFrom = $scatterOrigin;
            if ($hadBall) {
                if ($throwInFrom !== null && $throwInFrom->isOnPitch()) {
                    $state = $state->withBall(BallState::onGround($throwInFrom));
                    $throwIn = $this->ballResolver->resolveThrowIn($state, $throwInFrom, $landingPos);
                    $state = $throwIn['state'];
                    $events = array_merge($events, $throwIn['events']);
                } else {
                    // Bez znameho posledniho ctverce nemam odkud vhazovat.
                    $state = $state->withBall(BallState::offPitch());
                }
            }

            $injResult = $this->injuryResolver->resolveCrowdSurf($projectile, $this->dice);
            $projectile = $injResult['player'];
            $state = $state->withPlayer($projectile);
            $events = array_merge($events, $injResult['events']);

            // ⛔ OPRAVA 11.09.2026: turnover se vyhlasoval BEZ OHLEDU na to,
            //   jestli hozeny hrac mic mel -- pritom `$hadBall` je o par
            //   radku vys a pouziva se.
            //   `rules_bb2016.txt` r. 368-370 (bod 1): „being injured by the
            //   crowd ... **is not a turnover unless it is a player from the
            //   active team holding the ball**."
            //   A bod 6 (r. 381-384) mluvi taky jen o hraci **S MICEM**:
            //   „A player **with the ball** is thrown ... and fails to land
            //   successfully."
            // ⇒ Hozeny hrac bez mice, ktery skonci u davu, kolo nekonci.
            if (!$hadBall) {
                return ActionResult::success($state, $events);
            }

            $events[] = GameEvent::turnover('Thrown player off pitch');
            return ActionResult::turnover($state->withTurnoverPending(true), $events);
        }

        // Occupied square → scatter further
        $occupant = $state->getPlayerAtPosition($landingPos);
        if ($occupant !== null && $occupant->getId() !== $projectile->getId()) {
            $direction = $this->dice->rollD8();
            $newPos = $this->scatterCalc->scatterOnce($landingPos, $direction);
            return $this->resolveLanding($state, $projectile, $newPos, $hadBall, $events, $landingPos);
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
