<?php

declare(strict_types=1);

namespace App\Engine;

use App\DTO\BallState;
use App\DTO\GameEvent;
use App\DTO\GameState;
use App\DTO\MatchPlayerDTO;
use App\Enum\PlayerState;
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

        // ⛔ OPRAVA 18.09.2026 (`rules_bb2016.txt` r. 8381-8387, 926): Pro jen
        //   na kostku, kterou jeste nic neprehodilo; pouziti se ZAPISE (1x za kolo);
        //   po Pro uz tymovy prehoz kostky ne -- jen hodu Pro (`ProCheck`).
        $proUsed = false;
        if (!$success && !$skillRerollUsed && $player->hasSkill(SkillName::Pro) && !$player->isProUsedThisTurn()) {
            $proUsed = true;
            $player = $player->withProUsedThisTurn(true);
            $state = $state->withPlayer($player);
            $pro = ProCheck::roll($this->dice, $player, $teamRerollAvailable, $events);
            $teamRerollUsed = $pro['teamRerollUsed'];
            if ($pro['allowed']) {
                $roll = $this->dice->rollD6();
                $success = $roll >= $target;
                $events[] = GameEvent::ballPickup($player->getId(), $target, $roll, $success);
            }
        }

        // Team reroll (only if no skill reroll and no Pro was used)
        if (!$success && !$skillRerollUsed && !$proUsed && $teamRerollAvailable) {
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

        // ⛔ DOPLNENO 11.09.2026: `rules_bb2016.txt` r. 857-858 --
        //   „**Prone and Stunned players may never attempt to catch the
        //   ball.**" Tahle straz tu CHYBELA: lezicimu hraci se hazel hod na
        //   chyceni, jako by stal.
        //   ⭐ `resolveBounce` (r. 211) tutez podminku uz mel
        //   (`$playerAtLanding->getState()->canAct()`), takze odrazova cesta
        //   byla spravne -- neslo o chybejici pravidlo, ale o JEDNU Z DVOU
        //   CEST, ktera ho nemela. Ted je na jednom miste pro vsechny
        //   volajici.
        if (!$catcher->getState()->canAct()) {
            $bounceResult = $this->resolveBounce($state, $pos);

            return [
                'state' => $bounceResult['state'],
                'events' => $bounceResult['events'],
                'success' => false,
                'teamRerollUsed' => false,
            ];
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

        // ⛔ OPRAVA 18.09.2026: tataz pravidla Pro jako u zvedani (r. 8381-8387, 926).
        $proUsed = false;
        if (!$success && !$skillRerollUsed && $catcher->hasSkill(SkillName::Pro) && !$catcher->isProUsedThisTurn()) {
            $proUsed = true;
            $catcher = $catcher->withProUsedThisTurn(true);
            $state = $state->withPlayer($catcher);
            $pro = ProCheck::roll($this->dice, $catcher, $teamRerollAvailable, $events);
            $teamRerollUsed = $pro['teamRerollUsed'];
            if ($pro['allowed']) {
                $roll = $this->dice->rollD6();
                $success = $roll >= $target;
                $events[] = GameEvent::catchAttempt($catcher->getId(), $target, $roll, $success);
            }
        }

        // Team reroll (only if no skill reroll and no Pro was used)
        if (!$success && !$skillRerollUsed && !$proUsed && $teamRerollAvailable) {
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
            $throwInResult = $this->resolveThrowIn($state, $from, $landingPos, $depth);
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

        // Lezici nebo omraceny hrac: mic odskakuje dal (r. 893-898)
        if ($playerAtLanding !== null) {
            $bounceResult = $this->resolveBounce($state, $landingPos, $depth + 1);
            return ['state' => $bounceResult['state'], 'events' => array_merge($events, $bounceResult['events'])];
        }

        // Empty square: ball lands on ground
        $state = $state->withBall(BallState::onGround($landingPos));
        return ['state' => $state, 'events' => $events];
    }

    /**
     * Throw-in -- rules_bb2016 r. 868-878.
     *  - sablona Throw-in (`ScatterCalculator::throwInOffset`) od posledniho pole na hristi,
     *    vzdalenost 2D6; mic se posouva po jednom poli, aby bylo znat posledni pole pred vyletem;
     *  - stojici hrac na cilovem poli MUSI chytat, prazdne pole nebo lezici/omraceny = odskok;
     *  - kdyz mic vyleti znovu, vhazuje se znovu „centred on the last square it was in".
     * Strop 8 vhozeni (jako C++): pak se mic polozi na posledni pole v hristi a odskoci --
     * to je nase pojistka, ne pravidlo.
     *
     * @param Position $offPitchExit pole za hranou, kam mic vyletel (urcuje stranu sablony)
     * @return array{state: GameState, events: list<GameEvent>}
     */
    public function resolveThrowIn(GameState $state, Position $lastOnPitch, Position $offPitchExit, int $depth = 0): array
    {
        $events = [];
        $origin = $lastOnPitch;
        $exitAt = $offPitchExit;

        for ($attempt = 0; $attempt < 8; $attempt++) {
            $templateRoll = $this->dice->rollD6();
            [$dx, $dy] = $this->scatterCalc->throwInOffset($origin, $exitAt, $templateRoll);
            $distance = $this->dice->roll2D6();

            $dest = $origin;
            $lastInside = $origin;
            $leftPitch = false;
            for ($step = 0; $step < $distance; $step++) {
                $dest = new Position($dest->getX() + $dx, $dest->getY() + $dy);
                if (!$dest->isOnPitch()) {
                    $leftPitch = true;
                    break;
                }
                $lastInside = $dest;
            }

            $events[] = GameEvent::throwIn((string) $origin, (string) $dest, $templateRoll, $distance);

            if ($leftPitch) {
                $origin = $lastInside;
                $exitAt = $dest;
                continue;
            }

            $state = $state->withBall(BallState::onGround($dest));
            $playerAtLanding = $state->getPlayerAtPosition($dest);
            if ($playerAtLanding !== null && $playerAtLanding->getState() === PlayerState::STANDING) {
                $catchResult = $this->resolveCatchFromBounce($state, $playerAtLanding, $depth);
                return ['state' => $catchResult['state'], 'events' => array_merge($events, $catchResult['events'])];
            }

            $bounceResult = $this->resolveBounce($state, $dest, $depth + 1);
            return ['state' => $bounceResult['state'], 'events' => array_merge($events, $bounceResult['events'])];
        }

        $state = $state->withBall(BallState::onGround($origin));
        $bounceResult = $this->resolveBounce($state, $origin, $depth + 1);
        return ['state' => $bounceResult['state'], 'events' => array_merge($events, $bounceResult['events'])];
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
        // ⛔⛔ OPRAVENO 15.09.2026 -- Nerves of Steel tu byl navic.
        //   `rules_bb2016.txt` r. 8315-8317: NoS ignoruje zony jen "when he
        //   attempts to pass, catch or intercept" -- zvedani v tom vyctu neni.
        //   Pri zvedani zony ignoruje JEN Big Hand (r. 7835-7839).
        $tz = $player->hasSkill(SkillName::BigHand)
            ? 0
            : $this->tzCalc->countTacklezones($state, $pos, $player->getTeamSide());

        $target = 7 - $ag - 1 + $tz;

        // Extra Arms: -1 to pickup target
        if ($player->hasSkill(SkillName::ExtraArms)) {
            $target--;
        }

        // Weather modifier: +1 for Pouring Rain
        // ⛔ OPRAVENO 15.09.2026 -- Blizzard sem nepatri (r. 1490-1494: jen GFI
        //   a omezeni dosahu prihravky, zadny modifikator k chytani ani zvedani).
        // ⛔ A Big Hand ignoruje pri zvedani i dest (r. 7837-7839: "ignores
        //   modifier(s) for enemy tackle zones OR POURING RAIN weather").
        if ($state->getWeather() === Weather::POURING_RAIN && !$player->hasSkill(SkillName::BigHand)) {
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

        // Weather modifier: +1 for Pouring Rain
        // ⛔ OPRAVENO 15.09.2026 -- Blizzard sem nepatri (r. 1490-1494: jen GFI
        //   a omezeni dosahu prihravky, zadny modifikator k chytani ani zvedani).
        if ($state->getWeather() === Weather::POURING_RAIN) {
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

        // ⛔⛔⛔ OPRAVA 12.09.2026 (PHP34): TADY MIC MIZEL ZE HRY.
        //   Kdyz srazeny nosic nemel pozici -- protoze ho prave odstranilo
        //   zraneni, KO nebo crowd surf -- nastavil se `offPitch` a NIC ho
        //   nevratilo. Zbytek pule se pak hral BEZ MICE: nikdo ho nemohl
        //   zvednout a skorovat uz neslo.
        // ⭐ ZMERENO PRED OPRAVOU (`cli/diag_cage_20260912.php`, 10 her):
        //   16 kol z 317 (5 %) zacalo v HRATELNE fazi s micem `offPitch`.
        // ⭐ MIC PRITOM VI, KDE BYL: `BallState::carried()` nese pozici.
        //   Odskakuje se tedy z pole, ktere hrac zabiral -- a kdyz to pole
        //   lezi mimo hriste, `resolveBounce` z nej udela throw-in.
        $pos = $fallenPlayer->getPosition() ?? $state->getBall()->getPosition();
        if ($pos === null) {
            return [$state->withBall(BallState::offPitch()), $events];
        }

        $state = $state->withBall(BallState::onGround($pos));
        $bounceResult = $this->resolveBounce($state, $pos);
        $events = array_merge($events, $bounceResult['events']);

        return [$bounceResult['state'], $events];
    }
}
