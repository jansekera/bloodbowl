<?php

declare(strict_types=1);

namespace App\Engine;

use App\Enum\RollType;
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
    /**
     * Pojistka proti nekonečné rekurzi, ne pravidlo (jako C++ `ball_handler.cpp` `depth > 200`).
     * OPRAVENO 19.11.2026 (audit parity 08.10., nález 22) -- tady bylo 5: po pátém odrazu zůstal míč ležet
     * i pod stojícím nebo ležícím hráčem. Ř. 903-906: odráží se, „until it is either caught or bounces into
     * an empty square or off the pitch“.
     */
    private const MAX_BOUNCES = 200;


    public function __construct(
        private readonly DiceRollerInterface $dice,
        private readonly TacklezoneCalculator $tzCalc,
        private readonly ScatterCalculator $scatterCalc,
        // P294 review nález 5: kouč-člověk (strany + odpovědi z přehrání) z ActionResolveru; výchozí = nikdo (testy)
        private readonly Coach $coach = new Coach(),
    ) {}

    /**
     * Kdo skočí pro míč dopadající na prázdné `$landing` (null = nikdo). Skill je volba (ř. 8065 "can
     * attempt", ř. 985): hráče kouče-člověka se ptá -- bez odpovědi akci přeruší `DivingCatchQuestion`
     * (viz `ReplayDiceRoller`); AI skočí s jedním hráčem (nejvyšší šance chycení i s přehozem) z těch, pro
     * které `$aiJumps` vrátí true.
     *
     * OPRAVENO 30. 10. 2026 (uživatel, varianta B) — dřív dva a víc kandidátů ⇒ nikdo, bez otázky.
     *   Pravidla ř. 8069-8071 "If there are two or more players ATTEMPTING to use this skill then they
     *   get in each other's way and neither can use it" a ř. 986-990 "the coach whose turn is taking
     *   place must use his skill first" ⇒ napřed strana na tahu (`$naTahu`, výchozí aktivní tým) o každém
     *   svém kandidátovi, pak druhá; otázka nese počet dalších kandidátů (bez těch, kdo už odmítli —
     *   recenze 30.10. D/2) a kdo už skáče. Právě jeden pokus ⇒ chytá, dva a víc ⇒ nikdo (další otázky
     *   už nic nezmění, nekladou se).
     *
     * P239 (uživatel 2.11.2026): AI vybírá skokana podle šance chycení i s přehozem (`divingCatchChance`);
     *   `$aiJumps` dostane i to, jestli už někdo skáče (kopající na lajně pak skočí — viz KickoffResolver).
     *
     * @param callable(MatchPlayerDTO, bool): bool $aiJumps (hráč, už někdo skáče)
     * @param TeamSide|null $rerollSide tým, jehož týmový přehoz smí jít na chytání (jako `resolveLanding`)
     * @param int $catchModifier modifikátor skutečného chytání (+1 přesná přihrávka) — do šance (recenze P239 LOW-3)
     */
    public function divingCatchAttempt(
        GameState $state,
        Position $landing,
        callable $aiJumps,
        ?TeamSide $naTahu = null,
        ?TeamSide $rerollSide = null,
        int $catchModifier = 0,
    ): ?MatchPlayerDTO {
        $kandidati = $this->divingCatchCandidates($state, $landing);
        $naTahu ??= $state->getActiveTeam();
        /** @var list<MatchPlayerDTO> $skacou */
        $skacou = [];
        $odmitli = 0; // recenze 30.10. D/2: kdo už odmítl, do „dalších kandidátů“ v otázce nepatří
        foreach ([$naTahu, $naTahu->opponent()] as $strana) {
            $svoji = array_values(array_filter($kandidati, static fn(MatchPlayerDTO $p) => $p->getTeamSide() === $strana));
            if ($this->coach->decides($strana)) {
                foreach ($svoji as $hrac) {
                    if (count($skacou) >= 2) {
                        break;
                    }
                    $odpoved = $this->coach->divingCatch($hrac->getId(), $landing, count($kandidati) - 1 - $odmitli, ($skacou[0] ?? null)?->getId(), $state);
                    if ($odpoved) {
                        $skacou[] = $hrac;
                    } else {
                        $odmitli++;
                    }
                }
                continue;
            }
            $nejlepsi = null;
            $nejlepsiSance = ['sance' => 0.0, 'tymovy' => false];
            foreach ($svoji as $hrac) {
                if (!$aiJumps($hrac, $skacou !== [])) {
                    continue;
                }
                $sance = $this->divingCatchChance($state, $hrac, $rerollSide, $catchModifier);
                $lepsi = $sance['sance'] > $nejlepsiSance['sance'] + 1e-9
                    || ($sance['sance'] > $nejlepsiSance['sance'] - 1e-9 && $nejlepsiSance['tymovy'] && !$sance['tymovy']);
                if ($nejlepsi === null || $lepsi) {
                    $nejlepsi = $hrac;
                    $nejlepsiSance = $sance;
                }
            }
            $odmitli += count($svoji);
            if ($nejlepsi !== null) {
                $skacou[] = $nejlepsi;
                $odmitli--;
            }
        }

        return count($skacou) === 1 ? $skacou[0] : null;
    }

    /**
     * P239b (uživatel 2.11.2026): šance, že hráč skokem míč chytí, i s přehozem — podle té AI vybírá skokana.
     * Catch (ř. 7992-7995) ⇒ p + (1−p)·p. Bez Catch týmový přehoz, jen týmu `$rerollSide` s přehozem po ruce
     * (výkop a tah soupeře ne: ř. 928-936, 1261-1263, FAQ ř. 9253); Loner napřed 4+ (ř. 8287-8290).
     * `tymovy` = šance stojí na týmovém přehozu (uživatel: „ber to jako horší než hráč s catch“ ⇒ při shodě
     * skáče hráč s Catch). `$modifikator` = jako skutečné chytání (+1 přesná přihrávka, ř. 891). Pro
     * (ř. 8385-8389) se nepočítá — vědomé zjednodušení. Parita: C++ `divingCatchChance` (ball_handler.cpp).
     *
     * @return array{sance: float, tymovy: bool}
     */
    private function divingCatchChance(GameState $state, MatchPlayerDTO $hrac, ?TeamSide $rerollSide, int $modifikator): array
    {
        $jednou = (7 - max(2, min(6, $this->getCatchTarget($state, $hrac, $modifikator)))) / 6;
        if ($hrac->hasSkill(SkillName::Catch)) {
            return ['sance' => $jednou + (1 - $jednou) * $jednou, 'tymovy' => false];
        }
        if (!self::tymovyPrehozPoRuce($state, $hrac->getTeamSide(), $rerollSide)) {
            return ['sance' => $jednou, 'tymovy' => false];
        }
        $brana = $hrac->hasSkill(SkillName::Loner) ? 0.5 : 1.0;

        return ['sance' => $jednou + (1 - $jednou) * $brana * $jednou, 'tymovy' => true];
    }

    /**
     * Recenze P239 (2.11.2026, LOW-2): JEDINÉ místo podmínky „hráč strany `$strana` smí na chytání použít týmový
     * přehoz“ — jen tým `$rerollSide`, který určil volající (tým na tahu ve fázi hry; výkop a tah soupeře null,
     * ř. 929-933, 1261-1263), a jen s přehozem po ruce. Dřív tři kopie (resolveLanding, chytání odrazu, volba
     * Diving Catch). Parita: C++ `teamRerollAllowed` + `canUseReroll`.
     */
    private static function tymovyPrehozPoRuce(GameState $state, TeamSide $strana, ?TeamSide $rerollSide): bool
    {
        return $strana === $rerollSide && $state->getTeamState($strana)->canUseReroll();
    }

    /**
     * Attempt to pick up ball when player moves onto ball square.
     * @return array{state: GameState, events: list<GameEvent>, success: bool}
     */
    public function resolvePickup(GameState $state, MatchPlayerDTO $player, bool $teamRerollAvailable = false): array
    {
        $pos = $player->getPosition();
        if ($pos === null) {
            return ['state' => $state, 'events' => [], 'success' => false];
        }

        // No Hands: cannot pick up the ball at all
        if ($player->hasSkill(SkillName::NoHands)) {
            $events = [GameEvent::noHands($player->getId())];
            $bounceResult = $this->resolveBounce($state, $pos);
            $events = array_merge($events, $bounceResult['events']);
            return ['state' => $bounceResult['state'], 'events' => $events, 'success' => false];
        }

        $target = $this->getPickupTarget($state, $player);
        $roll = $this->dice->rollD6();
        $success = $roll >= $target;
        $events = [GameEvent::ballPickup($player->getId(), $target, $roll, $success)];

        // Sure Hands zdarma (ř. 8533), jinak Pro → týmový přehoz (Loner); kouč-člověk přehráním — `ThrowReroll::prehoditNaCil`.
        //   P294 krok 5d OPRAVENO 17.11.2026 — dřív vlastní kopie žebříčku, která přehazovala i za člověka (dialog 'pickup'
        //   se nikdy nenabídl), ř. 929-931: přehoz je volba kouče. Po Sure Hands nic (ř. 926, oprava 18.09.2026).
        if (!$success) {
            $res = ThrowReroll::prehoditNaCil(
                $this->dice,
                $state,
                $player,
                $teamRerollAvailable,
                $events,
                $this->coach,
                RollType::PICKUP,
                $roll,
                $target,
                $pos,
                static fn(int $r, bool $ok): GameEvent => GameEvent::ballPickup($player->getId(), $target, $r, $ok),
                SkillName::SureHands,
            );
            $state = $res['state'];
            $player = $res['thrower'];
            $success = $res['ok'];
        }

        if ($success) {
            $state = $state->withBall(BallState::carried($pos, $player->getId()));
            return ['state' => $state, 'events' => $events, 'success' => true];
        }

        // Failed: bounce from this square
        $bounceResult = $this->resolveBounce($state, $pos);
        $events = array_merge($events, $bounceResult['events']);

        return ['state' => $bounceResult['state'], 'events' => $events, 'success' => false];
    }

    /**
     * Hod na chycení: cíl, Catch, Pro, týmový přehoz (Loner). Stav míče nemění. Sdílí resolveCatch a bomba (P264,
     * review 9.11. M5 — bomba se chytá „using the same rules for catching the ball“, r. 7959-7961); C++ catchRoll.
     *
     * @return array{state: GameState, events: list<GameEvent>, success: bool}
     */
    public function hodNaChyceni(GameState $state, MatchPlayerDTO $catcher, int $modifier, bool $teamRerollAvailable, bool $accuratePass): array
    {
        $target = $this->getCatchTarget($state, $catcher, $modifier, $accuratePass);
        $roll = $this->dice->rollD6();
        $success = $roll >= $target;
        $events = [GameEvent::catchAttempt($catcher->getId(), $target, $roll, $success)];

        // Catch zdarma (ř. 7983), jinak Pro → týmový přehoz (Loner), přehoz se ve stavu odečte HNED; kouč-člověk přehráním.
        //   P294 krok 5d OPRAVENO 17.11.2026 — dřív vlastní kopie žebříčku (třetí ve `resolveCatchFromBounce`), přehazovala
        //   i za člověka a týmový přehoz jen ohlásila příznakem; volající ho odečetl až po odrazu nechyceného míče ⇒
        //   spoluhráč chytal odraz s DRUHÝM týmovým přehozem v tahu (ř. 934-935).
        if (!$success) {
            $res = ThrowReroll::prehoditNaCil(
                $this->dice,
                $state,
                $catcher,
                $teamRerollAvailable,
                $events,
                $this->coach,
                RollType::CATCH,
                $roll,
                $target,
                $catcher->getPosition(),
                static fn(int $r, bool $ok): GameEvent => GameEvent::catchAttempt($catcher->getId(), $target, $r, $ok),
                SkillName::Catch,
            );
            $state = $res['state'];
            $catcher = $res['thrower'];
            $success = $res['ok'];
        }

        return ['state' => $state, 'events' => $events, 'success' => $success];
    }

    /**
     * Attempt to catch ball (from pass, scatter landing, hand-off).
     * @return array{state: GameState, events: list<GameEvent>, success: bool}
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
            return ['state' => $state, 'events' => [], 'success' => false];
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
            ];
        }

        // No Hands: cannot catch the ball
        if ($catcher->hasSkill(SkillName::NoHands)) {
            $events = [GameEvent::noHands($catcher->getId())];
            $bounceResult = $this->resolveBounce($state, $pos, $depth, $kickoffReceiving);
            $events = array_merge($events, $bounceResult['events']);
            return ['state' => $bounceResult['state'], 'events' => $events, 'success' => false];
        }

        $hod = $this->hodNaChyceni($state, $catcher, $modifier, $teamRerollAvailable, $accuratePass);
        $state = $hod['state'];
        $events = $hod['events'];
        $success = $hod['success'];

        if ($success) {
            $state = $state->withBall(BallState::carried($pos, $catcher->getId()));
            return ['state' => $state, 'events' => $events, 'success' => true];
        }

        // Failed: bounce from catcher's square
        $bounceResult = $this->resolveBounce($state, $pos, $depth, $kickoffReceiving);
        $events = array_merge($events, $bounceResult['events']);

        return ['state' => $bounceResult['state'], 'events' => $events, 'success' => false];
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
        MatchPlayerDTO|false|null $divingCatcher = null,
        ?TeamSide $kickoffReceiving = null,
    ): array {
        $state = $state->withBall(BallState::onGround($landing));
        $events = [];

        $catcher = $state->getPlayerAtPosition($landing);
        $diving = false;
        if ($catcher === null) {
            // $divingCatcher: o skoku už rozhodl volající (výkop, viz KickoffResolver; false = nikdo);
            //   null = rozhodne se tu (kouč-člověk odpoví, AI skočí). Skill je volba (ř. 8065).
            $catcher = $divingCatcher ?? $this->divingCatchAttempt($state, $landing, static fn() => true, rerollSide: $rerollSide, catchModifier: $accuratePass ? 1 : 0);
            $catcher = $catcher === false ? null : $catcher;
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
        // Diving Catch (výše) je v logu akce dřív než hod na chycení ⇒ otázka na přehoz chytání ho ukáže (P306 A1)
        $catchResult = $this->resolveCatch(
            $state,
            $catcher,
            modifier: $accuratePass ? 1 : 0,
            teamRerollAvailable: self::tymovyPrehozPoRuce($state, $side, $rerollSide),
            accuratePass: $accuratePass && !$diving,
            kickoffReceiving: $kickoffReceiving,
            // OPRAVENO 30. 10. 2026 (P223 review LOW) — hloubka se tu ztrácela (odraz po neúspěchu
            //   začínal od 0) ⇒ řetěz „vhazování → nechytí → odraz ven“ obcházel pojistku MAX_BOUNCES.
            depth: $depth,
        );
        $state = $catchResult['state'];   // P294 krok 5d: týmový přehoz už odečetl `hodNaChyceni`

        return ['state' => $state, 'events' => array_merge($events, $catchResult['events'])];
    }

    /**
     * Hráči s Diving Catch, kteří smí chytit míč dopadající na sousední prázdné pole (kdo skočí: `divingCatchAttempt`).
     *
     * OPRAVENO 20. 10. 2026 (P221) — tady se hráč PŘESOUVAL na pole dopadu (a odtud se počítaly
     *   zóny i odskok), chytal i bez tackle zón, i na pole s ležícím hráčem, a ze dvou hráčů
     *   s Diving Catch se vrhl první nalezený. Pravidla ř. 8065-8071: "...that would land in an
     *   empty square in one of his tackle zones as if it had landed in his own square without
     *   leaving his current square. A failed catch will bounce from the Diving Catch player's
     *   square. If there are two or more players attempting to use this skill then they get in
     *   each other's way and neither can use it." Bez zón "may not catch" (ř. 7983-7986).
     *   Smí kterýkoli tým (i soupeř přihrávajícího, i sám přihrávající).
     *
     * @return list<MatchPlayerDTO>
     */
    public function divingCatchCandidates(GameState $state, Position $landingPos): array
    {
        if ($state->getPlayerAtPosition($landingPos) !== null) {
            return [];
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

        return $candidates;
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
        return self::cilChyceni($this->tzCalc, $state, $catcher, $modifier, $accuratePass);
    }

    /**
     * Cíl chytání bez kostek a bez resolveru — pro odhady AI (BombTarget, simplify 9.11.: dřív si kouč kvůli tomu stavěl
     * celý BallResolver s kostkou, kterou nehodil).
     */
    public static function cilChyceni(TacklezoneCalculator $tzCalc, GameState $state, MatchPlayerDTO $catcher, int $modifier = 0, bool $accuratePass = false): int
    {
        $pos = $catcher->getPosition();
        if ($pos === null) {
            return 6;
        }

        $ag = $catcher->getStats()->getAgility();
        $tz = $catcher->hasSkill(SkillName::NervesOfSteel)
            ? 0
            : $tzCalc->countTacklezones($state, $pos, $catcher->getTeamSide());

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
        $target += $tzCalc->countDisturbingPresence($state, $pos, $catcher->getTeamSide());

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

        // ⛔ OPRAVA 21.09.2026: Pro i týmový přehoz smí i chytání po odskoku (ř. 8381, 1263: "players may use the Catch
        //   or Pro skill to try to re-roll the catch roll"); týmový jen vlastní hráč ve vlastním kole (ř. 929-933), u míče
        //   dopadajícího po výkopu ne (ř. 1263). P294 krok 5d: týž hod jako jinde (`hodNaChyceni`), dřív vlastní kopie.
        $side = $catcher->getTeamSide();
        $hod = $this->hodNaChyceni(
            $state,
            $catcher,
            0,
            self::tymovyPrehozPoRuce($state, $side, $state->getPhase() === GamePhase::PLAY ? $state->getActiveTeam() : null),
            false,
        );
        $state = $hod['state'];
        $events = $hod['events'];
        $success = $hod['success'];

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
     * P294 krok 7: hráč na svém poli je sražen (Knocked Down) něčím jiným než pohybem — dopad hozeného hráče
     * (ř. 8617-8619), výbuch bomby (ř. 7957-7958), blok Ball & Chain: leží (omráčený zůstane omráčený, „roll for Armour
     * even if already Prone or Stunned“), hod na brnění a zranění, nesl-li míč, odskočí (ř. 678-679). Dřív tři kopie.
     * Událost pádu (playerFell / bombExplosion) zapíše volající.
     *
     * @param list<GameEvent> $events
     * @return array{0: GameState, 1: list<GameEvent>}
     */
    public function srazit(GameState $state, MatchPlayerDTO $hrac, InjuryResolver $zraneni, array $events, bool $mightyBlow = false): array
    {
        $hrac = $hrac->getState() === PlayerState::STUNNED ? $hrac : $hrac->withState(PlayerState::PRONE);
        $state = $state->withPlayer($hrac);
        $inj = $zraneni->resolve($hrac, $this->dice, mightyBlow: $mightyBlow);
        $state = $state->withPlayer($inj['player']);
        $events = array_merge($events, $inj['events']);
        return $this->handleBallOnPlayerDown($state, $inj['player'], $events);
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
