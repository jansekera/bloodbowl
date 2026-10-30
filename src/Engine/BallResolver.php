<?php

declare(strict_types=1);

namespace App\Engine;

use App\DTO\BallState;
use App\DTO\GameEvent;
use App\DTO\GameState;
use App\DTO\MatchPlayerDTO;
use App\Enum\GamePhase;
use App\Enum\PlayerState;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use App\Enum\Weather;
use App\ValueObject\Position;

final class BallResolver
{
    private const MAX_BOUNCES = 5;

    public function __construct(
        private readonly DiceRollerInterface $dice,
        private readonly TacklezoneCalculator $tzCalc,
        private readonly ScatterCalculator $scatterCalc,
    ) {}

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
        bool $accuratePass = false,
        ?TeamSide $kickoffReceiving = null,
        int $depth = 0,
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
        // ⛔ DOPLNENO 16.10.2026 (audit parity, nalez 19; C++ `f64cb396`): hrac bez tackle zon
        //   (Bone-head, Really Stupid, Hypnotic Gaze) nechyta -- `rules_bb2016.txt` r. 7983-7986:
        //   „loses his tackle zones and may not catch, intercept or pass“. Hlidalo se jen u zachyceni.
        if (!$catcher->getState()->canAct() || $catcher->hasLostTacklezones()) {
            $bounceResult = $this->resolveBounce($state, $pos, $depth, $kickoffReceiving);

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
            $bounceResult = $this->resolveBounce($state, $pos, $depth, $kickoffReceiving);
            $events = array_merge($events, $bounceResult['events']);
            return ['state' => $bounceResult['state'], 'events' => $events, 'success' => false, 'teamRerollUsed' => false];
        }

        $target = $this->getCatchTarget($state, $catcher, $modifier, $accuratePass);
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
        $bounceResult = $this->resolveBounce($state, $pos, $depth, $kickoffReceiving);
        $events = array_merge($events, $bounceResult['events']);

        return ['state' => $bounceResult['state'], 'events' => $events, 'success' => false, 'teamRerollUsed' => $teamRerollUsed];
    }

    /**
     * Míč DOPADÁ ZE VZDUCHU na pole `$landing` na hřišti — přihrávka (přesná i nepřesná), Hail Mary,
     * Dump-Off, výkop, vhazování. NE odraz (`resolveBounce`). Jediné místo, kde se rozhoduje, kdo
     * dopadající míč chytá: stojící hráč na poli; na prázdné pole hráč s Diving Catch; jinak odraz.
     *
     * DOPLNĚNO 20. 10. 2026 (P223) — Diving Catch byl jen u nepřesné přihrávky (P221, kopie
     *   v `PassResolver`). Pravidla ř. 8065-8067: "the player can attempt to catch ANY PASS, KICK OFF
     *   OR CROWD THROW-IN, but not bouncing ball, that would land in an empty square in one of his
     *   tackle zones". Přesná přihrávka chycená skokem dostane +1 "Catching an accurate pass"
     *   (ř. 891), ale ne +1 z Diving Catch — ten je jen "from an accurate pass targeted to his
     *   square" (ř. 8063-8064).
     *
     * @param TeamSide|null $rerollSide tým, jehož týmový přehoz smí jít na chytání (jen jeho hráč);
     *                                  null = žádný (výkop ř. 1261-1263, Dump-Off v soupeřově tahu)
     * @param TeamSide|null $kickoffReceiving jen u výkopu: přijímající tým — viz `resolveBounce`
     * @return array{state: GameState, events: list<GameEvent>}
     */
    public function resolveLanding(
        GameState $state,
        Position $landing,
        bool $accuratePass = false,
        ?TeamSide $rerollSide = null,
        int $depth = 0,
        bool $divingCatch = true,
        ?TeamSide $kickoffReceiving = null,
    ): array {
        $state = $state->withBall(BallState::onGround($landing));
        $events = [];

        $catcher = $state->getPlayerAtPosition($landing);
        $diving = false;
        if ($catcher === null) {
            // $divingCatch = false: kandidát skok odmítl (skill je volba, ř. 8065) — viz KickoffResolver
            $catcher = $divingCatch ? $this->findDivingCatchPlayer($state, $landing) : null;
            $diving = $catcher !== null;
        } elseif ($catcher->getState() !== PlayerState::STANDING) {
            $catcher = null;
        }

        if ($catcher === null) {
            return $this->resolveBounce($state, $landing, $depth, $kickoffReceiving);
        }

        if ($diving) {
            $events[] = GameEvent::divingCatch($catcher->getId());
        }
        $side = $catcher->getTeamSide();
        $catchResult = $this->resolveCatch(
            $state,
            $catcher,
            modifier: $accuratePass ? 1 : 0,
            teamRerollAvailable: $rerollSide === $side && $state->getTeamState($side)->canUseReroll(),
            accuratePass: $accuratePass && !$diving,
            kickoffReceiving: $kickoffReceiving,
            // OPRAVENO 30. 10. 2026 (P223 review LOW) — hloubka se tu ztrácela (odraz po neúspěchu
            //   začínal od 0) ⇒ řetěz „vhazování → nechytí → odraz ven“ obcházel pojistku MAX_BOUNCES.
            depth: $depth,
        );
        $state = $catchResult['state'];
        if ($catchResult['teamRerollUsed']) {
            $state = $state->withTeamState($side, $state->getTeamState($side)->withRerollUsed());
        }

        return ['state' => $state, 'events' => array_merge($events, $catchResult['events'])];
    }

    /**
     * Hráč s Diving Catch, který smí chytit míč dopadající na sousední prázdné pole (null = nikdo).
     *
     * OPRAVENO 20. 10. 2026 (P221) — tady se hráč PŘESOUVAL na pole dopadu (a odtud se počítaly
     *   zóny i odskok), chytal i bez tackle zón, i na pole s ležícím hráčem, a ze dvou hráčů
     *   s Diving Catch se vrhl první nalezený. Pravidla ř. 8065-8071: "...that would land in an
     *   empty square in one of his tackle zones as if it had landed in his own square without
     *   leaving his current square. A failed catch will bounce from the Diving Catch player's
     *   square. If there are two or more players attempting to use this skill then they get in
     *   each other's way and neither can use it." Bez zón "may not catch" (ř. 7983-7986).
     *   Smí kterýkoli tým (i soupeř přihrávajícího, i sám přihrávající).
     */
    public function findDivingCatchPlayer(GameState $state, Position $landingPos): ?MatchPlayerDTO
    {
        if ($state->getPlayerAtPosition($landingPos) !== null) {
            return null;
        }

        $candidates = [];
        foreach ($state->getPlayers() as $player) {
            $playerPos = $player->getPosition();
            if ($playerPos !== null
                && $player->hasSkill(SkillName::DivingCatch)
                && $player->getState() === PlayerState::STANDING
                && !$player->hasLostTacklezones()
                && $playerPos->distanceTo($landingPos) === 1) {
                $candidates[] = $player;
            }
        }

        return count($candidates) === 1 ? $candidates[0] : null;
    }

    /**
     * Bounce ball from a position. If lands on player, they try to catch.
     *
     * @param TeamSide|null $kickoffReceiving jen u výkopu: přijímající tým. Odraz mimo hřiště nebo
     *   do kopající poloviny se pak NEvhazuje ani nechytá — míč zůstane na tom poli a výkop
     *   (`KickoffResolver`) dá touchback.
     * @return array{state: GameState, events: list<GameEvent>}
     */
    public function resolveBounce(GameState $state, Position $from, int $depth = 0, ?TeamSide $kickoffReceiving = null): array
    {
        if ($depth >= self::MAX_BOUNCES) {
            // Safety: leave ball on ground
            $state = $state->withBall(BallState::onGround($from));
            return ['state' => $state, 'events' => []];
        }

        $direction = $this->dice->rollD8();
        $landingPos = $this->scatterCalc->scatterOnce($from, $direction);

        $events = [GameEvent::ballBounce((string) $from, (string) $landingPos, $direction)];

        // OPRAVENO 30. 10. 2026 (P223 review, parita s C++ `landBall`) — u výkopu se míč po odrazu
        //   ven vhazoval a po odrazu do kopající poloviny tam zůstal ležet (nebo ho chytil soupeř).
        //   Pravidla ř. 280-282: "If the ball scatters or BOUNCES off the pitch or into the kicking
        //   team's half, the receiving coach is awarded a 'touchback'"; FAQ ř. 9315-9318: "any event
        //   that causes the ball to go out of bounds or over the line of scrimmage during a kick-off
        //   results in a touchback".
        if ($kickoffReceiving !== null && !$this->scatterCalc->isInReceivingHalf($landingPos, $kickoffReceiving)) {
            return ['state' => $state->withBall(BallState::onGround($landingPos)), 'events' => $events];
        }

        // Off pitch? Throw-in
        if (!$landingPos->isOnPitch()) {
            $throwInResult = $this->resolveThrowIn($state, $from, $landingPos, $depth);
            $events = array_merge($events, $throwInResult['events']);
            return ['state' => $throwInResult['state'], 'events' => $events];
        }

        // Player on landing square? Try to catch
        $playerAtLanding = $state->getPlayerAtPosition($landingPos);
        if ($playerAtLanding !== null && $playerAtLanding->getState()->canAct() && !$playerAtLanding->hasLostTacklezones()) {
            $state = $state->withBall(BallState::onGround($landingPos));
            $catchResult = $this->resolveCatchFromBounce($state, $playerAtLanding, $depth, $kickoffReceiving);
            $events = array_merge($events, $catchResult['events']);
            return ['state' => $catchResult['state'], 'events' => $events];
        }

        // Lezici nebo omraceny hrac: mic odskakuje dal (r. 893-898)
        if ($playerAtLanding !== null) {
            $bounceResult = $this->resolveBounce($state, $landingPos, $depth + 1, $kickoffReceiving);
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
            // OPRAVENO 14.10.2026 (P197, převod C++ T5.24) -- tady bylo `$step < $distance`, míč letěl
            // o pole dál. FAQ ř. 9307-9314: pole s logem šablony je PRVNÍ pole pohybu, při 2D6 = 2
            // se míč posune o JEDNO pole.
            for ($step = 0; $step < $distance - 1; $step++) {
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

            // Týmový přehoz chytání jen ve vlastním tahu (ř. 929-933); hráč bez zón nechytá
            // (review 16.10., nález 19) -- hlídá `resolveCatch`.
            $landingResult = $this->resolveLanding(
                $state,
                $dest,
                rerollSide: $state->getPhase() === GamePhase::PLAY ? $state->getActiveTeam() : null,
                depth: $depth + 1,
            );
            return ['state' => $landingResult['state'], 'events' => array_merge($events, $landingResult['events'])];
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
    public function getCatchTarget(GameState $state, MatchPlayerDTO $catcher, int $modifier = 0, bool $accuratePass = false): int
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

        // OPRAVENO 07.10.2026 (P165) — tady bylo „-1 v zóně soupeře“. Pravidla ř. 8063-8065:
        // "The player may add 1 to any catch roll from an accurate pass targeted to his square."
        // ⇒ jen přesná přihrávka na hráčovo pole; zóny soupeře s tím nesouvisí.
        if ($accuratePass && $catcher->hasSkill(SkillName::DivingCatch)) {
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
    private function resolveCatchFromBounce(GameState $state, MatchPlayerDTO $catcher, int $depth, ?TeamSide $kickoffReceiving): array
    {
        $pos = $catcher->getPosition();
        if ($pos === null) {
            return ['state' => $state, 'events' => []];
        }

        // No Hands: cannot catch, bounce again
        if ($catcher->hasSkill(SkillName::NoHands)) {
            $events = [GameEvent::noHands($catcher->getId())];
            $bounceResult = $this->resolveBounce($state, $pos, $depth + 1, $kickoffReceiving);
            $events = array_merge($events, $bounceResult['events']);
            return ['state' => $bounceResult['state'], 'events' => $events];
        }

        $target = $this->getCatchTarget($state, $catcher);
        $roll = $this->dice->rollD6();
        $success = $roll >= $target;
        $events = [GameEvent::catchAttempt($catcher->getId(), $target, $roll, $success)];

        // ⛔ OPRAVA 21.09.2026 (1/2): prehoz z Catch se tu delal POTICHU --
        //   vydala se jen jedna udalost `catch` s uz prehozenym vysledkem,
        //   takze prvni pokus ani pouziti skillu nebylo v zaznamu videt
        //   (a zadny histogram je nemohl spocitat).
        $skillRerollUsed = false;
        if (!$success && $catcher->hasSkill(SkillName::Catch)) {
            $skillRerollUsed = true;
            $roll = $this->dice->rollD6();
            $success = $roll >= $target;
            $events[] = GameEvent::rerollUsed($catcher->getId(), 'Catch');
            $events[] = GameEvent::catchAttempt($catcher->getId(), $target, $roll, $success);
        }

        // ⛔ OPRAVA 21.09.2026 (2/2): `rules_bb2016.txt` r. 8381 -- Pro smi
        //   prehodit "any one dice roll he has made other than Armour, Injury
        //   or Casualty", tedy i chytani po odskoku a po vhazeni; r. 1263 to
        //   rika u dopadajiciho mice vyslovne: "players may use the Catch or
        //   Pro skill to try to re-roll the catch roll".
        //   ⛔ Po prehozu z Catch uz Pro neprichazi (r. 925-926: tataz kostka
        //   se neprehazuje dvakrat).
        //
        // ⭐ DOPLNENO 21.09.2026: tymovy prehoz. r. 929-933: "A coach may use a
        //   team re-roll to re-roll any dice roll ... made by a player in their
        //   own team and who is still on the pitch **during their own turn**."
        //   ⇒ podminky: vlastni hrac, vlastni kolo, faze hry a volny prehoz.
        //   ⛔ U mice DOPADAJICIHO po vykopu tymovy prehoz nejde (r. 1263) --
        //   ten se resi v `resolveCatch()`, kam `teamRerollAvailable` posila
        //   volajici a `KickoffResolver` ho necha na `false`.
        $side = $catcher->getTeamSide();
        $teamRerollAvailable = $state->getPhase() === GamePhase::PLAY
            && $state->getActiveTeam() === $side
            && $state->getTeamState($side)->canUseReroll();

        $proUsed = false;
        if (!$success && !$skillRerollUsed
            && $catcher->hasSkill(SkillName::Pro) && !$catcher->isProUsedThisTurn()) {
            $proUsed = true;
            $catcher = $catcher->withProUsedThisTurn(true);
            $state = $state->withPlayer($catcher);
            $pro = ProCheck::roll($this->dice, $catcher, $teamRerollAvailable, $events);
            if ($pro['teamRerollUsed']) {
                $state = $state->withTeamState($side, $state->getTeamState($side)->withRerollUsed());
                $teamRerollAvailable = false;
            }
            if ($pro['allowed']) {
                $roll = $this->dice->rollD6();
                $success = $roll >= $target;
                $events[] = GameEvent::catchAttempt($catcher->getId(), $target, $roll, $success);
            }
        }

        // Tymovy prehoz samotneho chytani (jen kdyz se neprehazovalo skillem ani Pro)
        if (!$success && !$skillRerollUsed && !$proUsed && $teamRerollAvailable) {
            $state = $state->withTeamState($side, $state->getTeamState($side)->withRerollUsed());
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
            return ['state' => $state, 'events' => $events];
        }

        // Failed: bounce again
        $bounceResult = $this->resolveBounce($state, $pos, $depth + 1, $kickoffReceiving);
        $events = array_merge($events, $bounceResult['events']);
        return ['state' => $bounceResult['state'], 'events' => $events];
    }

    /**
     * Hráč se na pole `$pos` dostal jinak než vlastním krokem (odtlačení, pád při úhybu /
     * GFI / skoku) a leží tam volný míč ⇒ míč odskočí o pole; zvednout ho hráč nesmí.
     *
     * DOPLNĚNO 08.10.2026 (audit parity, nález 13) -- tohle v PHP chybělo: řešil se jen míč,
     *   který hráč NESL (`handleBallOnPlayerDown`), volný míč zůstal ležet pod hráčem.
     *   Pravidla ř. 441-444: "Players that move into the square with the ball at other
     *   times (e.g., when pushed back, thrown by another player with Throw Team-Mate, etc.)
     *   cannot pick up the ball, and instead it will bounce one square. This does not cause
     *   a turnover." ř. 640-641: "a player pushed to it will cause the ball to bounce";
     *   ř. 896-900: "a player is pushed to or lands in the ball's square ... it will bounce".
     *
     * @param list<GameEvent> $events
     * @return array{0: GameState, 1: list<GameEvent>}
     */
    public function bounceLooseBallUnderPlayer(GameState $state, Position $pos, array $events): array
    {
        $ball = $state->getBall();
        $ballPos = $ball->getPosition();
        if ($ball->isHeld() || $ballPos === null || !$ballPos->equals($pos)) {
            return [$state, $events];
        }

        $bounceResult = $this->resolveBounce($state, $pos);

        return [$bounceResult['state'], array_merge($events, $bounceResult['events'])];
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
