<?php

declare(strict_types=1);

namespace App\Engine;

use App\DTO\ActionResult;
use App\DTO\BallState;
use App\DTO\GameEvent;
use App\DTO\GameState;
use App\DTO\MatchPlayerDTO;
use App\Enum\PassRange;
use App\Enum\PlayerState;
use App\Enum\SkillName;
use App\Enum\Weather;
use App\ValueObject\Position;

final class PassResolver
{
    public function __construct(
        private readonly DiceRollerInterface $dice,
        private readonly TacklezoneCalculator $tzCalc,
        private readonly ScatterCalculator $scatterCalc,
        private readonly BallResolver $ballResolver,
    ) {
    }

    /**
     * @param array<string, mixed> $params {playerId, targetX, targetY}
     */
    public function resolve(GameState $state, array $params): ActionResult
    {
        $playerId = (int) $params['playerId'];
        $targetX = (int) $params['targetX'];
        $targetY = (int) $params['targetY'];
        $target = new Position($targetX, $targetY);

        $thrower = $state->getPlayer($playerId);
        if ($thrower === null || $thrower->getPosition() === null) {
            throw new \InvalidArgumentException('Thrower not found or not on pitch');
        }

        $from = $thrower->getPosition();

        // Animosity check: if passer has Animosity and receiver has different raceName
        $animosityEvents = [];
        if ($thrower->hasSkill(SkillName::Animosity)) {
            $receiver = $state->getPlayerAtPosition($target);
            if ($receiver !== null && $this->isDifferentRace($thrower, $receiver)) {
                $animRoll = $this->dice->rollD6();
                $animSuccess = $animRoll >= 2;
                $animosityEvents[] = GameEvent::animosity($playerId, $receiver->getId(), $animRoll, $animSuccess);
                if (!$animSuccess) {
                    // Ball stays with passer, mark acted, not a turnover
                    $activeSide = $thrower->getTeamSide();
                    $teamState = $state->getTeamState($activeSide);
                    $state = $state->withTeamState($activeSide, $teamState->withPassUsed());
                    $state = $state->withPlayer($thrower->withHasActed(true)->withHasMoved(true));
                    return ActionResult::success($state, $animosityEvents);
                }
            }
        }

        $range = PassRange::fromDistance($from->distanceTo($target));
        $isHailMary = false;

        if ($range === null) {
            if ($thrower->hasSkill(SkillName::HailMaryPass)) {
                $isHailMary = true;
            } else {
                throw new \InvalidArgumentException('Target is out of range');
            }
        }

        // Strong Arm: reduce effective range by one band (not for HMP)
        if (!$isHailMary && $thrower->hasSkill(SkillName::StrongArm)) {
            $range = $range->reduced();
        }

        // Mark pass used this turn
        $activeSide = $thrower->getTeamSide();
        $teamState = $state->getTeamState($activeSide);
        $state = $state->withTeamState($activeSide, $teamState->withPassUsed());

        // Mark thrower as acted
        $state = $state->withPlayer($thrower->withHasActed(true)->withHasMoved(true));

        // Hail Mary Pass: special handling
        if ($isHailMary) {
            return $this->resolveHailMaryPass($state, $thrower, $from, $target);
        }

        // Pass Block: opposing players with PassBlock within 3 of thrower or target move toward target
        $passBlockResult = $this->resolvePassBlock($state, $thrower, $from, $target);
        $state = $passBlockResult['state'];
        $passBlockEvents = $passBlockResult['events'];

        // Check interceptions along the pass path
        $interceptResult = $this->checkInterceptions($state, $from, $target, $thrower);
        if ($interceptResult['intercepted']) {
            // Prepend pass block events to interception result
            $interceptEvents = array_merge($animosityEvents, $passBlockEvents, $interceptResult['result']->getEvents());
            return ActionResult::turnover($interceptResult['result']->getNewState(), $interceptEvents);
        }

        // Accuracy roll with reroll support
        $accuracyTarget = $this->getAccuracyTarget($state, $thrower, $range);
        $roll = $this->dice->rollD6();
        $accurate = $roll !== 1 && $roll >= $accuracyTarget;
        $fumble = $roll === 1;

        $events = array_merge($animosityEvents, $passBlockEvents, $interceptResult['events']);
        $resultStr = $fumble ? 'fumble' : ($accurate ? 'accurate' : 'inaccurate');
        $events[] = GameEvent::passAttempt($playerId, (string) $from, (string) $target, $range->value, $accuracyTarget, $roll, $resultStr);

        // Reroll if not accurate
        if (!$accurate) {
            $skillRerollUsed = false;
            $teamRerollUsed = false;

            // Pass skill reroll
            if ($thrower->hasSkill(SkillName::Pass)) {
                $skillRerollUsed = true;
                $roll = $this->dice->rollD6();
                $accurate = $roll !== 1 && $roll >= $accuracyTarget;
                $fumble = $roll === 1;
                $resultStr = $fumble ? 'fumble' : ($accurate ? 'accurate' : 'inaccurate');
                $events[] = GameEvent::rerollUsed($playerId, 'Pass');
                $events[] = GameEvent::passAttempt($playerId, (string) $from, (string) $target, $range->value, $accuracyTarget, $roll, $resultStr);
            }

            // Pro reroll (after skill reroll, before team reroll)
            if (!$accurate && $thrower->hasSkill(SkillName::Pro) && !$thrower->isProUsedThisTurn()) {
                $proRoll = $this->dice->rollD6();
                if ($proRoll >= 4) {
                    $roll = $this->dice->rollD6();
                    $accurate = $roll !== 1 && $roll >= $accuracyTarget;
                    $fumble = $roll === 1;
                    $resultStr = $fumble ? 'fumble' : ($accurate ? 'accurate' : 'inaccurate');
                    $events[] = GameEvent::proReroll($playerId, $proRoll, true, $roll);
                    $events[] = GameEvent::passAttempt($playerId, (string) $from, (string) $target, $range->value, $accuracyTarget, $roll, $resultStr);
                } else {
                    $events[] = GameEvent::proReroll($playerId, $proRoll, false, null);
                }
            }

            // Team reroll (only if no skill reroll was used)
            if (!$accurate && !$skillRerollUsed && $state->getTeamState($activeSide)->canUseReroll()) {
                $state = $state->withTeamState($activeSide, $state->getTeamState($activeSide)->withRerollUsed());
                $lonerBlocked = false;
                if ($thrower->hasSkill(SkillName::Loner)) {
                    $lonerRoll = $this->dice->rollD6();
                    $lonerBlocked = $lonerRoll < 4;
                    $events[] = GameEvent::lonerCheck($playerId, $lonerRoll, !$lonerBlocked);
                }
                if (!$lonerBlocked) {
                    $roll = $this->dice->rollD6();
                    $accurate = $roll !== 1 && $roll >= $accuracyTarget;
                    $fumble = $roll === 1;
                    $resultStr = $fumble ? 'fumble' : ($accurate ? 'accurate' : 'inaccurate');
                    $events[] = GameEvent::rerollUsed($playerId, 'Team Reroll');
                    $events[] = GameEvent::passAttempt($playerId, (string) $from, (string) $target, $range->value, $accuracyTarget, $roll, $resultStr);
                }
            }
        }

        // Handle fumble
        if ($fumble) {
            $events[] = GameEvent::turnover('Fumbled pass');

            // Ball bounces from thrower position
            $state = $state->withBall(BallState::onGround($from));
            $bounceResult = $this->ballResolver->resolveBounce($state, $from);
            $events = array_merge($events, $bounceResult['events']);

            return ActionResult::turnover(
                $bounceResult['state']->withTurnoverPending(true),
                $events,
            );
        }

        // Team reroll availability for catch (may have been used for accuracy)
        $catchTeamReroll = $state->getTeamState($activeSide)->canUseReroll();

        if ($accurate) {
            // Ball lands on target square - update ball position
            $state = $state->withBall(BallState::onGround($target));

            // Check if there's a player at target to catch
            $catcher = $state->getPlayerAtPosition($target);
            if ($catcher !== null && $catcher->getState() === PlayerState::STANDING) {
                $isFriendly = $catcher->getTeamSide() === $activeSide;
                $catchResult = $this->ballResolver->resolveCatch(
                    $state,
                    $catcher,
                    modifier: 1,
                    teamRerollAvailable: $isFriendly && $catchTeamReroll,
                );
                $events = array_merge($events, $catchResult['events']);
                $state = $catchResult['state'];

                if ($catchResult['teamRerollUsed']) {
                    $state = $state->withTeamState($activeSide, $state->getTeamState($activeSide)->withRerollUsed());
                }

                if (!$catchResult['success']) {
                    if (!$this->ballCaughtByTeam($state, $activeSide)) {
                        $events[] = GameEvent::turnover('Pass not caught');
                        return ActionResult::turnover($state->withTurnoverPending(true), $events);
                    }
                }

                return ActionResult::success($state, $events);
            }

            // No one at target: ball on ground, bounce
            $bounceResult = $this->ballResolver->resolveBounce($state, $target);
            $events = array_merge($events, $bounceResult['events']);
            $state = $bounceResult['state'];

            if (!$this->ballCaughtByTeam($state, $activeSide)) {
                $events[] = GameEvent::turnover('Pass not caught');
                return ActionResult::turnover($state->withTurnoverPending(true), $events);
            }

            return ActionResult::success($state, $events);
        }

        // Inaccurate pass - scatter from target
        $d8 = $this->dice->rollD8();
        $scatterDist = min(3, $this->dice->rollD6()); // max 3 scatter distance
        $landingPos = $this->scatterCalc->scatterWithDistance($target, $d8, $scatterDist);

        if (!$landingPos->isOnPitch()) {
            // Scattered off pitch
            $state = $state->withBall(BallState::onGround($target));
            $throwInResult = $this->ballResolver->resolveThrowIn($state, $target);
            $events = array_merge($events, $throwInResult['events']);
            $state = $throwInResult['state'];
        } else {
            $state = $state->withBall(BallState::onGround($landingPos));

            $playerAtLanding = $state->getPlayerAtPosition($landingPos);
            if ($playerAtLanding !== null && $playerAtLanding->getState() === PlayerState::STANDING) {
                $isFriendly = $playerAtLanding->getTeamSide() === $activeSide;
                $catchResult = $this->ballResolver->resolveCatch(
                    $state,
                    $playerAtLanding,
                    teamRerollAvailable: $isFriendly && $catchTeamReroll,
                );
                $events = array_merge($events, $catchResult['events']);
                $state = $catchResult['state'];

                if ($catchResult['teamRerollUsed']) {
                    $state = $state->withTeamState($activeSide, $state->getTeamState($activeSide)->withRerollUsed());
                }
            } else {
                // Diving Catch: check adjacent players before bounce
                $divingCatcher = $this->findDivingCatchPlayer($state, $landingPos);
                if ($divingCatcher !== null) {
                    $events[] = GameEvent::divingCatch($divingCatcher->getId());
                    // Move player to landing square
                    $dcPos = $divingCatcher->getPosition();
                    $divingCatcher = $divingCatcher->withPosition($landingPos);
                    $state = $state->withPlayer($divingCatcher);
                    $isFriendly = $divingCatcher->getTeamSide() === $activeSide;
                    $catchResult = $this->ballResolver->resolveCatch(
                        $state, $divingCatcher,
                        teamRerollAvailable: $isFriendly && $catchTeamReroll,
                    );
                    $events = array_merge($events, $catchResult['events']);
                    $state = $catchResult['state'];
                    if ($catchResult['teamRerollUsed']) {
                        $state = $state->withTeamState($activeSide, $state->getTeamState($activeSide)->withRerollUsed());
                    }
                } else {
                    // Empty square - bounce
                    $bounceResult = $this->ballResolver->resolveBounce($state, $landingPos);
                    $events = array_merge($events, $bounceResult['events']);
                    $state = $bounceResult['state'];
                }
            }
        }

        // Check if own team ended up with the ball
        if (!$this->ballCaughtByTeam($state, $activeSide)) {
            $events[] = GameEvent::turnover('Pass not caught');
            return ActionResult::turnover($state->withTurnoverPending(true), $events);
        }

        return ActionResult::success($state, $events);
    }

    /**
     * Calculate accuracy target: 7 - AG + range_modifier + TZ, clamped 2-6.
     */
    public function getAccuracyTarget(GameState $state, MatchPlayerDTO $thrower, PassRange $range): int
    {
        $ag = $thrower->getStats()->getAgility();
        $pos = $thrower->getPosition();
        $tz = 0;
        if ($pos !== null && !$thrower->hasSkill(SkillName::NervesOfSteel)) {
            $tz = $this->tzCalc->countTacklezones($state, $pos, $thrower->getTeamSide());
        }

        $target = 7 - $ag + $tz - $range->modifier();

        // Disturbing Presence: +1 per DP enemy within 3 squares
        if ($pos !== null) {
            $target += $this->tzCalc->countDisturbingPresence($state, $pos, $thrower->getTeamSide());
        }

        // Weather modifier: +1 for Very Sunny, Pouring Rain, Blizzard
        if (in_array($state->getWeather(), [Weather::VERY_SUNNY, Weather::POURING_RAIN, Weather::BLIZZARD], true)) {
            $target++;
        }

        // Pass skill: -1 to target
        if ($thrower->hasSkill(SkillName::Pass)) {
            $target--;
        }

        // Accurate: -1 to target
        if ($thrower->hasSkill(SkillName::Accurate)) {
            $target--;
        }

        return max(2, min(6, $target));
    }

    /**
     * Get squares along the pass path (simplified Bresenham).
     * @return list<Position>
     */
    public function getPassPath(Position $from, Position $to): array
    {
        $path = [];
        $x0 = $from->getX();
        $y0 = $from->getY();
        $x1 = $to->getX();
        $y1 = $to->getY();

        $dx = abs($x1 - $x0);
        $dy = abs($y1 - $y0);
        $sx = $x0 < $x1 ? 1 : -1;
        $sy = $y0 < $y1 ? 1 : -1;
        $err = $dx - $dy;

        $cx = $x0;
        $cy = $y0;

        while (true) {
            // Skip start and end positions
            if (!($cx === $x0 && $cy === $y0) && !($cx === $x1 && $cy === $y1)) {
                $path[] = new Position($cx, $cy);
            }

            if ($cx === $x1 && $cy === $y1) {
                break;
            }

            $e2 = 2 * $err;
            if ($e2 > -$dy) {
                $err -= $dy;
                $cx += $sx;
            }
            if ($e2 < $dx) {
                $err += $dx;
                $cy += $sy;
            }
        }

        return $path;
    }

    /**
     * Check for interceptions along the pass path.
     * First eligible enemy standing in the path can attempt intercept (AG-2 roll).
     *
     * @return array{intercepted: bool, result: ?ActionResult, events: list<GameEvent>}
     */
    private function checkInterceptions(GameState $state, Position $from, Position $to, MatchPlayerDTO $thrower): array
    {
        $passPath = $this->getPassPath($from, $to);
        $enemySide = $thrower->getTeamSide()->opponent();

        foreach ($passPath as $pathPos) {
            $player = $state->getPlayerAtPosition($pathPos);
            if ($player === null || $player->getTeamSide() !== $enemySide || $player->getState() !== PlayerState::STANDING) {
                continue;
            }

            // Interception attempt: AG roll with -2 modifier
            $ag = $player->getStats()->getAgility();
            $target = max(2, min(6, 7 - $ag + 2));
            // Very Long Legs: -1 to interception target
            if ($player->hasSkill(SkillName::VeryLongLegs)) {
                $target = max(2, $target - 1);
            }
            $roll = $this->dice->rollD6();
            $success = $roll >= $target;

            $events = [GameEvent::interception($player->getId(), $target, $roll, $success)];

            if ($success && $thrower->hasSkill(SkillName::SafeThrow)) {
                // Safe Throw: force interception reroll
                $reroll = $this->dice->rollD6();
                $saved = $reroll < $target;
                $events[] = GameEvent::safeThrow($thrower->getId(), $reroll, $saved);
                if ($saved) {
                    $success = false; // interception nullified
                }
            }

            if ($success) {
                // Intercepted! Ball caught by enemy
                $state = $state->withBall(BallState::carried($pathPos, $player->getId()));
                $events[] = GameEvent::turnover('Pass intercepted');
                return ['intercepted' => true, 'result' => ActionResult::turnover($state->withTurnoverPending(true), $events), 'events' => $events];
            }

            // Only one interception attempt per pass — pass continues with events
            return ['intercepted' => false, 'result' => null, 'events' => $events];
        }

        return ['intercepted' => false, 'result' => null, 'events' => []];
    }

    /**
     * Resolve Hail Mary Pass: always inaccurate, no interception, scatter 3x from target.
     * Fumble on natural 1.
     */
    private function resolveHailMaryPass(GameState $state, MatchPlayerDTO $thrower, Position $from, Position $target): ActionResult
    {
        $activeSide = $thrower->getTeamSide();
        $events = [GameEvent::hailMaryPass($thrower->getId(), (string) $from, (string) $target)];

        // Roll D6 for fumble check
        $roll = $this->dice->rollD6();
        if ($roll === 1) {
            // Fumble
            $events[] = GameEvent::passAttempt($thrower->getId(), (string) $from, (string) $target, 'hail_mary', 2, $roll, 'fumble');
            $events[] = GameEvent::turnover('Fumbled Hail Mary pass');
            $state = $state->withBall(BallState::onGround($from));
            $bounceResult = $this->ballResolver->resolveBounce($state, $from);
            $events = array_merge($events, $bounceResult['events']);
            return ActionResult::turnover($bounceResult['state']->withTurnoverPending(true), $events);
        }

        $events[] = GameEvent::passAttempt($thrower->getId(), (string) $from, (string) $target, 'hail_mary', 2, $roll, 'inaccurate');

        // Always inaccurate: scatter 3 times from target
        $landingPos = $target;
        for ($i = 0; $i < 3; $i++) {
            $d8 = $this->dice->rollD8();
            $landingPos = $this->scatterCalc->scatterOnce($landingPos, $d8);
        }

        if (!$landingPos->isOnPitch()) {
            $state = $state->withBall(BallState::onGround($target));
            $throwInResult = $this->ballResolver->resolveThrowIn($state, $target);
            $events = array_merge($events, $throwInResult['events']);
            $state = $throwInResult['state'];
        } else {
            $state = $state->withBall(BallState::onGround($landingPos));
            $playerAtLanding = $state->getPlayerAtPosition($landingPos);
            if ($playerAtLanding !== null && $playerAtLanding->getState() === PlayerState::STANDING) {
                $isFriendly = $playerAtLanding->getTeamSide() === $activeSide;
                $catchResult = $this->ballResolver->resolveCatch(
                    $state, $playerAtLanding,
                    teamRerollAvailable: $isFriendly && $state->getTeamState($activeSide)->canUseReroll(),
                );
                $events = array_merge($events, $catchResult['events']);
                $state = $catchResult['state'];
            } else {
                $bounceResult = $this->ballResolver->resolveBounce($state, $landingPos);
                $events = array_merge($events, $bounceResult['events']);
                $state = $bounceResult['state'];
            }
        }

        if (!$this->ballCaughtByTeam($state, $activeSide)) {
            $events[] = GameEvent::turnover('Hail Mary pass not caught');
            return ActionResult::turnover($state->withTurnoverPending(true), $events);
        }

        return ActionResult::success($state, $events);
    }

    /**
     * Resolve Dump-Off: quick pass before block (no rerolls, just accuracy + catch).
     * @return array{state: GameState, events: list<GameEvent>}
     */
    public function resolveDumpOff(GameState $state, MatchPlayerDTO $thrower, Position $target): array
    {
        $from = $thrower->getPosition();
        if ($from === null) {
            return ['state' => $state, 'events' => []];
        }

        $events = [GameEvent::dumpOff($thrower->getId())];

        $range = PassRange::fromDistance($from->distanceTo($target));
        if ($range === null) {
            // Out of range — dump-off fails
            return ['state' => $state, 'events' => $events];
        }

        $accuracyTarget = $this->getAccuracyTarget($state, $thrower, $range);
        $roll = $this->dice->rollD6();
        $accurate = $roll !== 1 && $roll >= $accuracyTarget;
        $fumble = $roll === 1;

        $resultStr = $fumble ? 'fumble' : ($accurate ? 'accurate' : 'inaccurate');
        $events[] = GameEvent::passAttempt($thrower->getId(), (string) $from, (string) $target, $range->value, $accuracyTarget, $roll, $resultStr);

        if ($fumble) {
            // Ball bounces from thrower
            $state = $state->withBall(BallState::onGround($from));
            $bounceResult = $this->ballResolver->resolveBounce($state, $from);
            $events = array_merge($events, $bounceResult['events']);
            return ['state' => $bounceResult['state'], 'events' => $events];
        }

        if ($accurate) {
            $state = $state->withBall(BallState::onGround($target));
            $catcher = $state->getPlayerAtPosition($target);
            if ($catcher !== null && $catcher->getState() === PlayerState::STANDING) {
                // Catch attempt (no rerolls for dump-off)
                $catchResult = $this->ballResolver->resolveCatch($state, $catcher, modifier: 1);
                $events = array_merge($events, $catchResult['events']);
                return ['state' => $catchResult['state'], 'events' => $events];
            }
            $bounceResult = $this->ballResolver->resolveBounce($state, $target);
            $events = array_merge($events, $bounceResult['events']);
            return ['state' => $bounceResult['state'], 'events' => $events];
        }

        // Inaccurate
        $d8 = $this->dice->rollD8();
        $scatterDist = min(3, $this->dice->rollD6());
        $landingPos = $this->scatterCalc->scatterWithDistance($target, $d8, $scatterDist);

        if (!$landingPos->isOnPitch()) {
            $state = $state->withBall(BallState::onGround($target));
            $throwInResult = $this->ballResolver->resolveThrowIn($state, $target);
            $events = array_merge($events, $throwInResult['events']);
            return ['state' => $throwInResult['state'], 'events' => $events];
        }

        $state = $state->withBall(BallState::onGround($landingPos));
        $playerAtLanding = $state->getPlayerAtPosition($landingPos);
        if ($playerAtLanding !== null && $playerAtLanding->getState() === PlayerState::STANDING) {
            $catchResult = $this->ballResolver->resolveCatch($state, $playerAtLanding);
            $events = array_merge($events, $catchResult['events']);
            return ['state' => $catchResult['state'], 'events' => $events];
        }

        $bounceResult = $this->ballResolver->resolveBounce($state, $landingPos);
        $events = array_merge($events, $bounceResult['events']);
        return ['state' => $bounceResult['state'], 'events' => $events];
    }

    /**
     * Find a player with Diving Catch adjacent to the landing position.
     * Must be standing, within 1 square, and the landing square must be empty.
     */
    private function findDivingCatchPlayer(GameState $state, Position $landingPos): ?MatchPlayerDTO
    {
        foreach ($state->getPlayers() as $player) {
            if (!$player->hasSkill(SkillName::DivingCatch)) {
                continue;
            }
            if ($player->getState() !== PlayerState::STANDING) {
                continue;
            }
            $playerPos = $player->getPosition();
            if ($playerPos === null) {
                continue;
            }
            if ($playerPos->distanceTo($landingPos) === 1) {
                return $player;
            }
        }
        return null;
    }

    /**
     * Pass Block: opposing standing players within 3 Chebyshev of thrower or target
     * may move up to 3 squares toward the target (no dodge rolls).
     * @return array{state: GameState, events: list<GameEvent>}
     */
    private function resolvePassBlock(GameState $state, MatchPlayerDTO $thrower, Position $from, Position $target): array
    {
        $enemySide = $thrower->getTeamSide()->opponent();

        // Find the best PB candidate (closest to target)
        $bestPlayer = null;
        $bestDist = PHP_INT_MAX;

        foreach ($state->getPlayers() as $player) {
            if ($player->getTeamSide() !== $enemySide) {
                continue;
            }
            if ($player->getState() !== PlayerState::STANDING) {
                continue;
            }
            if (!$player->hasSkill(SkillName::PassBlock)) {
                continue;
            }
            $pos = $player->getPosition();
            if ($pos === null) {
                continue;
            }

            $distToThrower = $pos->distanceTo($from);
            $distToTarget = $pos->distanceTo($target);
            if ($distToThrower > 3 && $distToTarget > 3) {
                continue;
            }

            if ($distToTarget < $bestDist) {
                $bestDist = $distToTarget;
                $bestPlayer = $player;
            }
        }

        if ($bestPlayer === null) {
            return ['state' => $state, 'events' => []];
        }

        $pos = $bestPlayer->getPosition();
        $currentPos = $pos;
        for ($step = 0; $step < 3; $step++) {
            $nextPos = $this->stepToward($currentPos, $target);
            if ($nextPos === null || $nextPos->equals($currentPos)) {
                break;
            }
            if (!$nextPos->isOnPitch()) {
                break;
            }
            if ($state->getPlayerAtPosition($nextPos) !== null) {
                break;
            }
            $currentPos = $nextPos;
        }

        if ($currentPos->equals($pos)) {
            return ['state' => $state, 'events' => []];
        }

        $events = [GameEvent::passBlock($bestPlayer->getId(), (string) $pos, (string) $currentPos)];
        $state = $state->withPlayer($bestPlayer->withPosition($currentPos));

        return ['state' => $state, 'events' => $events];
    }

    private function stepToward(Position $from, Position $to): ?Position
    {
        $dx = $to->getX() - $from->getX();
        $dy = $to->getY() - $from->getY();

        $sx = $dx === 0 ? 0 : ($dx > 0 ? 1 : -1);
        $sy = $dy === 0 ? 0 : ($dy > 0 ? 1 : -1);

        if ($sx === 0 && $sy === 0) {
            return null;
        }

        return new Position($from->getX() + $sx, $from->getY() + $sy);
    }

    private function isDifferentRace(MatchPlayerDTO $a, MatchPlayerDTO $b): bool
    {
        $raceA = $a->getRaceName();
        $raceB = $b->getRaceName();
        // Both null = same race (default), one null and one set = different
        if ($raceA === null && $raceB === null) {
            return false;
        }
        return $raceA !== $raceB;
    }

    private function ballCaughtByTeam(GameState $state, \App\Enum\TeamSide $side): bool
    {
        $ball = $state->getBall();
        if (!$ball->isHeld()) {
            return false;
        }

        $carrierId = $ball->getCarrierId();
        if ($carrierId === null) {
            return false;
        }
        $carrier = $state->getPlayer($carrierId);
        return $carrier !== null && $carrier->getTeamSide() === $side;
    }
}
