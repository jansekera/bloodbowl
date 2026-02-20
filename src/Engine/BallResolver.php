<?php

declare(strict_types=1);

namespace App\Engine;

use App\DTO\BallState;
use App\DTO\GameEvent;
use App\DTO\GameState;
use App\DTO\MatchPlayerDTO;
use App\Enum\SkillName;
use App\Enum\Weather;
use App\ValueObject\Position;

final class BallResolver
{
    private const MAX_BOUNCES = 5;

    public function __construct(
        private readonly DiceRollerInterface $dice,
        private readonly TacklezoneCalculator $tzCalc,
        private readonly ScatterCalculator $scatterCalc,
    ) {
    }

    /**
     * Attempt to pick up ball when player moves onto ball square.
     * @return array{state: GameState, events: list<GameEvent>, success: bool, teamRerollUsed: bool}
     */
    public function resolvePickup(GameState $state, MatchPlayerDTO $player, bool $teamRerollAvailable = false): array
    {
        $pos = $player->getPosition();
        if ($pos === null) {
            return ['state' => $state, 'events' => [], 'success' => false, 'teamRerollUsed' => false];
        }

        // No Hands: cannot pick up the ball at all
        if ($player->hasSkill(SkillName::NoHands)) {
            $events = [GameEvent::noHands($player->getId())];
            $bounceResult = $this->resolveBounce($state, $pos);
            $events = array_merge($events, $bounceResult['events']);
            return ['state' => $bounceResult['state'], 'events' => $events, 'success' => false, 'teamRerollUsed' => false];
        }

        $target = $this->getPickupTarget($state, $player);
        $roll = $this->dice->rollD6();
        $success = $roll >= $target;
        $events = [GameEvent::ballPickup($player->getId(), $target, $roll, $success)];
        $skillRerollUsed = false;
        $teamRerollUsed = false;

        // Sure Hands: reroll failed pickup
        if (!$success && $player->hasSkill(SkillName::SureHands)) {
            $skillRerollUsed = true;
            $roll = $this->dice->rollD6();
            $success = $roll >= $target;
            $events[] = GameEvent::rerollUsed($player->getId(), 'Sure Hands');
            $events[] = GameEvent::ballPickup($player->getId(), $target, $roll, $success);
        }

        // Pro reroll (after skill reroll, before team reroll)
        if (!$success && $player->hasSkill(SkillName::Pro) && !$player->isProUsedThisTurn()) {
            $proRoll = $this->dice->rollD6();
            // Note: caller must track proUsedThisTurn on state
            if ($proRoll >= 4) {
                $roll = $this->dice->rollD6();
                $success = $roll >= $target;
                $events[] = GameEvent::proReroll($player->getId(), $proRoll, true, $roll);
                $events[] = GameEvent::ballPickup($player->getId(), $target, $roll, $success);
            } else {
                $events[] = GameEvent::proReroll($player->getId(), $proRoll, false, null);
            }
        }

        // Team reroll (only if no skill reroll was used)
        if (!$success && !$skillRerollUsed && $teamRerollAvailable) {
            $teamRerollUsed = true;
            $lonerBlocked = false;
            if ($player->hasSkill(SkillName::Loner)) {
                $lonerRoll = $this->dice->rollD6();
                $lonerBlocked = $lonerRoll < 4;
                $events[] = GameEvent::lonerCheck($player->getId(), $lonerRoll, !$lonerBlocked);
            }
            if (!$lonerBlocked) {
                $roll = $this->dice->rollD6();
                $success = $roll >= $target;
                $events[] = GameEvent::rerollUsed($player->getId(), 'Team Reroll');
                $events[] = GameEvent::ballPickup($player->getId(), $target, $roll, $success);
            }
        }

        if ($success) {
            $state = $state->withBall(BallState::carried($pos, $player->getId()));
            return ['state' => $state, 'events' => $events, 'success' => true, 'teamRerollUsed' => $teamRerollUsed];
        }

        // Failed: bounce from this square
        $bounceResult = $this->resolveBounce($state, $pos);
        $events = array_merge($events, $bounceResult['events']);

        return ['state' => $bounceResult['state'], 'events' => $events, 'success' => false, 'teamRerollUsed' => $teamRerollUsed];
    }

    /**
     * Attempt to catch ball (from pass, scatter landing, hand-off).
     * @return array{state: GameState, events: list<GameEvent>, success: bool, teamRerollUsed: bool}
     */
    public function resolveCatch(
        GameState $state,
        MatchPlayerDTO $catcher,
        int $modifier = 0,
        bool $teamRerollAvailable = false,
    ): array {
        $pos = $catcher->getPosition();
        if ($pos === null) {
            return ['state' => $state, 'events' => [], 'success' => false, 'teamRerollUsed' => false];
        }

        // No Hands: cannot catch the ball
        if ($catcher->hasSkill(SkillName::NoHands)) {
            $events = [GameEvent::noHands($catcher->getId())];
            $bounceResult = $this->resolveBounce($state, $pos);
            $events = array_merge($events, $bounceResult['events']);
            return ['state' => $bounceResult['state'], 'events' => $events, 'success' => false, 'teamRerollUsed' => false];
        }

        $target = $this->getCatchTarget($state, $catcher, $modifier);
        $roll = $this->dice->rollD6();
        $success = $roll >= $target;
        $events = [GameEvent::catchAttempt($catcher->getId(), $target, $roll, $success)];
        $skillRerollUsed = false;
        $teamRerollUsed = false;

        // Catch skill: reroll failed catch
        if (!$success && $catcher->hasSkill(SkillName::Catch)) {
            $skillRerollUsed = true;
            $roll = $this->dice->rollD6();
            $success = $roll >= $target;
            $events[] = GameEvent::rerollUsed($catcher->getId(), 'Catch');
            $events[] = GameEvent::catchAttempt($catcher->getId(), $target, $roll, $success);
        }

        // Pro reroll (after skill reroll, before team reroll)
        if (!$success && $catcher->hasSkill(SkillName::Pro) && !$catcher->isProUsedThisTurn()) {
            $proRoll = $this->dice->rollD6();
            if ($proRoll >= 4) {
                $roll = $this->dice->rollD6();
                $success = $roll >= $target;
                $events[] = GameEvent::proReroll($catcher->getId(), $proRoll, true, $roll);
                $events[] = GameEvent::catchAttempt($catcher->getId(), $target, $roll, $success);
            } else {
                $events[] = GameEvent::proReroll($catcher->getId(), $proRoll, false, null);
            }
        }

        // Team reroll (only if no skill reroll was used)
        if (!$success && !$skillRerollUsed && $teamRerollAvailable) {
            $teamRerollUsed = true;
            $lonerBlocked = false;
            if ($catcher->hasSkill(SkillName::Loner)) {
                $lonerRoll = $this->dice->rollD6();
                $lonerBlocked = $lonerRoll < 4;
                $events[] = GameEvent::lonerCheck($catcher->getId(), $lonerRoll, !$lonerBlocked);
            }
            if (!$lonerBlocked) {
                $roll = $this->dice->rollD6();
                $success = $roll >= $target;
                $events[] = GameEvent::rerollUsed($catcher->getId(), 'Team Reroll');
                $events[] = GameEvent::catchAttempt($catcher->getId(), $target, $roll, $success);
            }
        }

        if ($success) {
            $state = $state->withBall(BallState::carried($pos, $catcher->getId()));
            return ['state' => $state, 'events' => $events, 'success' => true, 'teamRerollUsed' => $teamRerollUsed];
        }

        // Failed: bounce from catcher's square
        $bounceResult = $this->resolveBounce($state, $pos);
        $events = array_merge($events, $bounceResult['events']);

        return ['state' => $bounceResult['state'], 'events' => $events, 'success' => false, 'teamRerollUsed' => $teamRerollUsed];
    }

    /**
     * Bounce ball from a position. If lands on player, they try to catch.
     * @return array{state: GameState, events: list<GameEvent>}
     */
    public function resolveBounce(GameState $state, Position $from, int $depth = 0): array
    {
        if ($depth >= self::MAX_BOUNCES) {
            // Safety: leave ball on ground
            $state = $state->withBall(BallState::onGround($from));
            return ['state' => $state, 'events' => []];
        }

        $direction = $this->dice->rollD8();
        $landingPos = $this->scatterCalc->scatterOnce($from, $direction);

        $events = [GameEvent::ballBounce((string) $from, (string) $landingPos, $direction)];

        // Off pitch? Throw-in
        if (!$landingPos->isOnPitch()) {
            $throwInResult = $this->resolveThrowIn($state, $from);
            $events = array_merge($events, $throwInResult['events']);
            return ['state' => $throwInResult['state'], 'events' => $events];
        }

        // Player on landing square? Try to catch
        $playerAtLanding = $state->getPlayerAtPosition($landingPos);
        if ($playerAtLanding !== null && $playerAtLanding->getState()->canAct()) {
            $state = $state->withBall(BallState::onGround($landingPos));
            $catchResult = $this->resolveCatchFromBounce($state, $playerAtLanding, $depth);
            $events = array_merge($events, $catchResult['events']);
            return ['state' => $catchResult['state'], 'events' => $events];
        }

        // Empty square: ball lands on ground
        $state = $state->withBall(BallState::onGround($landingPos));
        return ['state' => $state, 'events' => $events];
    }

    /**
     * Throw-in from sideline when ball goes off pitch.
     * @return array{state: GameState, events: list<GameEvent>}
     */
    public function resolveThrowIn(GameState $state, Position $lastOnPitch): array
    {
        for ($attempt = 0; $attempt < 3; $attempt++) {
            $direction = $this->dice->rollD8();
            $distance = $this->dice->rollD6();
            $landingPos = $this->scatterCalc->scatterWithDistance($lastOnPitch, $direction, $distance);

            $events = [GameEvent::throwIn((string) $lastOnPitch, (string) $landingPos, $direction, $distance)];

            if ($landingPos->isOnPitch()) {
                $playerAtLanding = $state->getPlayerAtPosition($landingPos);
                if ($playerAtLanding !== null && $playerAtLanding->getState()->canAct()) {
                    $state = $state->withBall(BallState::onGround($landingPos));
                    $catchResult = $this->resolveCatchFromBounce($state, $playerAtLanding, 0);
                    $events = array_merge($events, $catchResult['events']);
                    return ['state' => $catchResult['state'], 'events' => $events];
                }

                $state = $state->withBall(BallState::onGround($landingPos));
                return ['state' => $state, 'events' => $events];
            }
        }

        // After 3 failed throw-in attempts, place ball at last known position
        $state = $state->withBall(BallState::onGround($lastOnPitch));
        return ['state' => $state, 'events' => [GameEvent::throwIn((string) $lastOnPitch, (string) $lastOnPitch, 0, 0)]];
    }

    /**
     * Calculate pickup target: 7 - AG - 1 (pickup bonus) + TZ, clamped 2-6.
     */
    public function getPickupTarget(GameState $state, MatchPlayerDTO $player): int
    {
        $pos = $player->getPosition();
        if ($pos === null) {
            return 6;
        }

        $ag = $player->getStats()->getAgility();
        $tz = ($player->hasSkill(SkillName::NervesOfSteel) || $player->hasSkill(SkillName::BigHand))
            ? 0
            : $this->tzCalc->countTacklezones($state, $pos, $player->getTeamSide());

        $target = 7 - $ag - 1 + $tz;

        // Extra Arms: -1 to pickup target
        if ($player->hasSkill(SkillName::ExtraArms)) {
            $target--;
        }

        // Weather modifier: +1 for Pouring Rain and Blizzard
        if (in_array($state->getWeather(), [Weather::POURING_RAIN, Weather::BLIZZARD], true)) {
            $target++;
        }

        return max(2, min(6, $target));
    }

    /**
     * Calculate catch target: 7 - AG + TZ - modifier, clamped 2-6.
     */
    public function getCatchTarget(GameState $state, MatchPlayerDTO $catcher, int $modifier = 0): int
    {
        $pos = $catcher->getPosition();
        if ($pos === null) {
            return 6;
        }

        $ag = $catcher->getStats()->getAgility();
        $tz = $catcher->hasSkill(SkillName::NervesOfSteel)
            ? 0
            : $this->tzCalc->countTacklezones($state, $pos, $catcher->getTeamSide());

        $target = 7 - $ag + $tz - $modifier;

        // Extra Arms: -1 to catch target
        if ($catcher->hasSkill(SkillName::ExtraArms)) {
            $target--;
        }

        // Diving Catch: -1 in enemy tackle zone
        if ($catcher->hasSkill(SkillName::DivingCatch) && $tz > 0) {
            $target--;
        }

        // Disturbing Presence: +1 per DP enemy within 3 squares
        $target += $this->tzCalc->countDisturbingPresence($state, $pos, $catcher->getTeamSide());

        // Weather modifier: +1 for Pouring Rain and Blizzard
        if (in_array($state->getWeather(), [Weather::POURING_RAIN, Weather::BLIZZARD], true)) {
            $target++;
        }

        return max(2, min(6, $target));
    }

    /**
     * Internal: catch attempt from bounce (no modifier, recurse on failure).
     * @return array{state: GameState, events: list<GameEvent>}
     */
    private function resolveCatchFromBounce(GameState $state, MatchPlayerDTO $catcher, int $depth): array
    {
        $pos = $catcher->getPosition();
        if ($pos === null) {
            return ['state' => $state, 'events' => []];
        }

        // No Hands: cannot catch, bounce again
        if ($catcher->hasSkill(SkillName::NoHands)) {
            $events = [GameEvent::noHands($catcher->getId())];
            $bounceResult = $this->resolveBounce($state, $pos, $depth + 1);
            $events = array_merge($events, $bounceResult['events']);
            return ['state' => $bounceResult['state'], 'events' => $events];
        }

        $target = $this->getCatchTarget($state, $catcher);
        $roll = $this->dice->rollD6();
        $success = $roll >= $target;

        if (!$success && $catcher->hasSkill(SkillName::Catch)) {
            $roll = $this->dice->rollD6();
            $success = $roll >= $target;
        }

        $events = [GameEvent::catchAttempt($catcher->getId(), $target, $roll, $success)];

        if ($success) {
            $state = $state->withBall(BallState::carried($pos, $catcher->getId()));
            return ['state' => $state, 'events' => $events];
        }

        // Failed: bounce again
        $bounceResult = $this->resolveBounce($state, $pos, $depth + 1);
        $events = array_merge($events, $bounceResult['events']);
        return ['state' => $bounceResult['state'], 'events' => $events];
    }

    /**
     * Handle ball when a player goes down (drop and bounce).
     *
     * @param list<GameEvent> $events
     * @return array{0: GameState, 1: list<GameEvent>}
     */
    public function handleBallOnPlayerDown(GameState $state, MatchPlayerDTO $fallenPlayer, array $events = []): array
    {
        $ball = $state->getBall();
        if ($ball->getCarrierId() !== $fallenPlayer->getId()) {
            return [$state, $events];
        }

        $pos = $fallenPlayer->getPosition();
        if ($pos === null) {
            return [$state->withBall(BallState::offPitch()), $events];
        }

        $state = $state->withBall(BallState::onGround($pos));
        $bounceResult = $this->resolveBounce($state, $pos);
        $events = array_merge($events, $bounceResult['events']);

        return [$bounceResult['state'], $events];
    }
}
