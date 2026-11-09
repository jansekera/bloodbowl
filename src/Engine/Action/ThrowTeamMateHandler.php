<?php

declare(strict_types=1);

namespace App\Engine\Action;

use App\Engine\ThrowTarget;
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
use App\Engine\ThrowReroll;
use App\Engine\TacklezoneCalculator;

final class ThrowTeamMateHandler implements ActionHandlerInterface
{
    private readonly \App\Engine\PassResolver $passResolver;

    public function __construct(
        private readonly DiceRollerInterface $dice,
        private readonly TacklezoneCalculator $tzCalc,
        private readonly ScatterCalculator $scatterCalc,
        private readonly InjuryResolver $injuryResolver,
        private readonly BallResolver $ballResolver,
    ) {
        // P256: hod se počítá „exactly the same as … passing a ball“ (r. 8605-8607) — modifikátory z přihrávky
        //   (jako BombThrowHandler).
        $this->passResolver = new \App\Engine\PassResolver($dice, $tzCalc, $scatterCalc, $ballResolver);
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
        // OPRAVENO 9.11.2026 (simplify, parita s C++ P268) — dosah se kontroloval až po spotřebě přihrávky a hodu na
        //   Always Hungry. r. 8608-8609: jen rychlá/krátká.
        $range = ThrowTarget::ttm($throwerPos, $landingTarget);
        if ($range === null) {
            throw new \InvalidArgumentException('Throw Team-Mate: only quick or short pass range (r. 8608-8609)');
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

            // P259 (review dávky 7.11.2026): napřed Pro, teprve pak týmový přehoz s Lonerem — jako C++ (attemptRoll);
            //   společný žebříček ThrowReroll (jeden hod = jeden přehoz, r. 925-927).
            if ($eaten) {
                $res = ThrowReroll::proThenTeam($this->dice, $state, $thrower, $activeSide, true, $events);
                $state = $res['state'];
                $thrower = $res['thrower'];
                if ($res['rerolled']) {
                    $hungryRoll = $this->dice->rollD6();
                    $eaten = $hungryRoll === 1;
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

        // Accuracy ($range ověřen na začátku — ThrowTarget::ttm; OPRAVA 21.09.2026 1/3 tam)

        // ⛔ OPRAVA 21.09.2026 (2/3): r. 8607-8608 -- "The pass is worked out
        //   exactly the same as the player with Throw Team-Mate passing a ball,
        //   **except the player must subtract 1 from the D6 roll** when he
        //   passes the player." Ta -1 tady chybela.
        //   A protoze se hod resi "exactly the same", plati i r. 1742-1745:
        //   fumble je pri hodu 1 **nebo pri modifikovanem vysledku <= 1**.
        //   OPRAVENO 7.11.2026 (P256, review dávky): „exactly the same“ ⇒ i Disturbing Presence, Very Sunny,
        //   Accurate a Strong Arm — dřív jen pásmo a zóny. Parita s C++ ttm_handler.cpp (throwRollMods − 1).
        $ag = $thrower->getStats()->getAgility();
        $modifikator = $this->passResolver->getThrowRollModifierForRulerBand($state, $thrower, $range) - 1;
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
        // OPRAVENO 9.11.2026 (P269) — tady stálo ->withHasMoved(true)->withHasActed(true), takže hozený hráč už
        //   nesměl táhnout. Right Stuff: „If the player is not injured during his landing he may take an Action
        //   later this turn if he has not already done so.“ ⇒ stav aktivace se nemění (jako C++ ttm_handler).
        $projectile = $projectile->withPosition($landingPos);
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

        // Hozeny dopadl na pole s volnym micem: zvednout ho nesmi, mic odskoci a turnover
        //   to neni (`rules_bb2016.txt` r. 441-444) -- viz `bounceLooseBallUnderPlayer`.
        if (!$hadBall) {
            [$state, $events] = $this->ballResolver->bounceLooseBallUnderPlayer($state, $landingPos, $events);
        }

        return ActionResult::success($state, $events);
    }
}
