<?php

declare(strict_types=1);

namespace App\Engine\Action;

use App\DTO\ActionResult;
use App\DTO\BallState;
use App\DTO\GameEvent;
use App\DTO\GameState;
use App\DTO\MatchPlayerDTO;
use App\Enum\PlayerState;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use App\ValueObject\Position;
use App\Engine\BallResolver;
use App\Engine\DiceRollerInterface;
use App\Engine\InjuryResolver;
use App\Engine\PassResolver;
use App\Engine\ThrowReroll;
use App\Engine\ScatterCalculator;
use App\Engine\TacklezoneCalculator;
use App\Engine\ThrowTarget;

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
        //   hrac tymu na tahu. P264 (9.11.2026): zachyceni a chyceni bomby uz engine hraje (letBomby).
        // Nelegalni cil akci nespotrebuje (jako C++, P268).
        $chyba = ThrowTarget::posud($state, $thrower, $throwerPos, $targetPos)['chyba'];
        if ($chyba !== null) {
            throw new \InvalidArgumentException($chyba);
        }
        $state = $state->withPlayer($thrower->withHasActed(true)->withHasMoved(true));

        $events = [];
        [$state, $events, $turnover] = $this->letBomby($state, $throwerId, $targetPos, $events, 0);

        return $turnover ? ActionResult::turnover($state, $events) : ActionResult::success($state, $events);
    }

    /** P264: řetěz chycení / zachycení je v praxi krátký; strop jen pojistka proti smyčce (C++ kMaxBombHops). */
    private const MAX_PRESKOKU = 8;

    /**
     * Jeden let bomby od `$throwerId` na `$targetPos`; po chycení / zachycení hází dál ten, kdo ji má.
     * Parita s C++ bomb_handler.cpp throwBombFrom.
     *
     * @param list<GameEvent> $events
     * @return array{0: GameState, 1: list<GameEvent>, 2: bool} stav, události, turnover týmu na tahu
     */
    private function letBomby(GameState $state, int $throwerId, Position $targetPos, array $events, int $skok): array
    {
        $thrower = $state->requirePlayer($throwerId);
        $throwerPos = $thrower->requirePosition();
        $activeSide = $state->getActiveTeam();

        // OPRAVENO 7.11.2026 (P256, review dávky) — bomba je hod míčem (r. 7952-7958: „including weather effects“,
        //   „all skills that may be used when a ball is thrown“): ve vánici jen rychlá/krátká a Strong Arm platí.
        //   Dřív ani jedno. Parita s C++ bomb_handler.cpp (passTargetLegal, throwRollMods).
        // P263 (7.11.2026, uživatel „OK“): Hail Mary u bomby — r. 7952-7954 „including … use of Hail Mary Pass“,
        //   r. 8164-8171: mimo pravítko jen s HMP a ne ve vánici; D6: 1 = fumble, 2-6 projde, ale „never accurate“
        //   ⇒ vždy 3 rozptyly od cíle; bez modifikátorů. Dřív se bomba mimo pravítko odmítla.
        ['range' => $range, 'hailMary' => $hailMary, 'chyba' => $chyba] = ThrowTarget::posud($state, $thrower, $throwerPos, $targetPos);
        if ($chyba !== null) {
            throw new \InvalidArgumentException($chyba);
        }

        // P264 (uživatel 9.11.2026: „přidej možnost intercept“) — r. 7959-7963: bombu lze zachytit jako míč a kdo ji
        //   zachytí, hned ji hodí dál. Zachycení před hodem (r. 1773-1774); Hail Mary ne (r. 8167). Zkusí jen hráč,
        //   jehož týmu výbuch v cíli hrozí. Nepovedené ⇒ bomba letí dál (uživatel). Zachycená není turnover (r. 7955).
        if (!$hailMary) {
            $z = $this->passResolver->najdiZachycujiciho($state, $thrower, $throwerPos, $targetPos);
            if ($z !== null && $this->tymuHroziVybuch($state, $z->getTeamSide(), $targetPos)
                && $this->passResolver->hodZachyceni($state, $thrower, $z, $events)) {
                return $this->hodDal($state, $z->getId(), $events, $skok + 1);
            }
        }

        $modifier = $range === null ? 0 : $this->passResolver->getThrowRollModifierForRulerBand($state, $thrower, $range);
        $accuracyTarget = max(2, min(6, 7 - $thrower->getStats()->getAgility() - $modifier));
        $roll = $this->dice->rollD6();
        $fumble = $roll === 1 || $roll + $modifier <= 1;
        $accurate = !$hailMary && !$fumble && $roll >= $accuracyTarget;
        $resultStr = $fumble ? 'fumble' : ($accurate ? 'accurate' : 'inaccurate');
        $events[] = GameEvent::bombThrow($throwerId, $roll, $resultStr);

        // PRIDANO 6.11.2026 (P251, uzivatel: „bombu jen na fumble“) -- driv se fumble bomby
        //   neprehazoval vubec. R. 7957-7958: "All skills that may be used when a ball is thrown
        //   may be used when a bomb is thrown also" => Pass (r. 8336-8337, zdarma), pak Pro
        //   (r. 8385-8387, po neuspesnem hodu Pro smi tymovy prehoz jen hod Pro), pak tymovy
        //   prehoz (r. 929-936, Loner 4+ r. 8287-8290). Jeden hod = nejvys jeden prehoz (r. 925-927).
        //   NEPRESNA bomba se neprehazuje (volba uzivatele). Hazec s Lonerem tymovy prehoz na bombu
        //   VUBEC nepouzije, ani na fumble (uzivatel 6.11.: „jestli ma bomber loner - tak se
        //   prikloň k neprehozeni ani fumble“); Pass a Pro (zdarma) mu zustavaji.
        //   P264: hod chycené bomby hráčem týmu, který není na tahu, týmový přehoz nesmí.
        //   Parita s C++ (bomb_handler.cpp, rerollThrow).
        if ($fumble) {
            $skillRerolled = false;
            if ($thrower->hasSkill(SkillName::Pass)) {
                $skillRerolled = true;
                $events[] = GameEvent::rerollUsed($throwerId, 'Pass');
            } else {
                $res = ThrowReroll::proThenTeam(
                    $this->dice,
                    $state,
                    $thrower,
                    $thrower->getTeamSide(),
                    !$thrower->hasSkill(SkillName::Loner) && $thrower->getTeamSide() === $activeSide,
                    $events,
                );
                $state = $res['state'];
                $thrower = $res['thrower'];
                $skillRerolled = $res['rerolled'];
            }
            if ($skillRerolled) {
                $roll = $this->dice->rollD6();
                $fumble = $roll === 1 || $roll + $modifier <= 1;
                $accurate = !$hailMary && !$fumble && $roll >= $accuracyTarget;
                $resultStr = $fumble ? 'fumble' : ($accurate ? 'accurate' : 'inaccurate');
                $events[] = GameEvent::bombThrow($throwerId, $roll, $resultStr);
            }
        }

        if ($fumble) {
            // r. 7967-7968: fumble vybuchne v poli házeče
            $events[] = GameEvent::bombLanding((string) $throwerPos);
            [$state, $events, $srazenNas] = $this->resolveExplosion($state, $throwerPos, $activeSide, $events);
            return [$state, $events, $srazenNas || $thrower->getTeamSide() === $activeSide];
        }
        $landingPos = $targetPos;
        if (!$accurate) {
            // Nepresne (a Hail Mary vzdy, P263): tri rozptyly od cile, jako u prihravky
            for ($i = 0; $i < 3; $i++) {
                $landingPos = $this->scatterCalc->scatterOnce($landingPos, $this->dice->rollD8());
            }
        }

        // Do davu: vybuchne bez ucinku
        if (!$landingPos->isOnPitch()) {
            return [$state, $events, false];
        }

        // P264: hráč v poli dopadu bombu chytá jako míč (přesná +1, Diving Catch jen přesná; Catch přehodí neúspěch).
        //   Pokus je vždy aspoň tak dobrý jako odmítnutí (obojí jinak = výbuch, r. 7964-7966). Rozhoduje engine.
        $chytajici = $state->getPlayerAtPosition($landingPos);
        if ($chytajici !== null && $chytajici->getState() === PlayerState::STANDING
            && !$chytajici->hasLostTacklezones() && !$chytajici->hasSkill(SkillName::NoHands)) {
            $cil = $this->ballResolver->getCatchTarget($state, $chytajici, $accurate ? 1 : 0, $accurate);
            $hod = $this->dice->rollD6();
            $chytil = $hod >= $cil;
            if (!$chytil && $chytajici->hasSkill(SkillName::Catch)) {
                $events[] = GameEvent::rerollUsed($chytajici->getId(), 'Catch');
                $hod = $this->dice->rollD6();
                $chytil = $hod >= $cil;
            }
            $events[] = GameEvent::catchAttempt($chytajici->getId(), $cil, $hod, $chytil);
            if ($chytil) {
                return $this->hodDal($state, $chytajici->getId(), $events, $skok + 1);
            }
        }

        $events[] = GameEvent::bombLanding((string) $landingPos);
        [$state, $events, $srazenNas] = $this->resolveExplosion($state, $landingPos, $activeSide, $events);
        return [$state, $events, $srazenNas];
    }

    /**
     * P264: chycenou / zachycenou bombu hráč hned hodí dál (r. 7961-7963, mimořádná akce). Cíl volí engine za oba
     * kouče (vyberCilHoduDal); když legální cíl není, vybuchne mu v ruce. C++ rethrowBomb.
     *
     * @param list<GameEvent> $events
     * @return array{0: GameState, 1: list<GameEvent>, 2: bool}
     */
    private function hodDal(GameState $state, int $drziId, array $events, int $skok): array
    {
        $drzi = $state->requirePlayer($drziId);
        $cil = $skok > self::MAX_PRESKOKU ? null : $this->vyberCilHoduDal($state, $drzi);
        if ($cil === null) {
            $pos = $drzi->requirePosition();
            $events[] = GameEvent::bombLanding((string) $pos);
            return $this->resolveExplosion($state, $pos, $state->getActiveTeam(), $events);
        }
        return $this->letBomby($state, $drziId, $cil, $events, $skok);
    }

    private function tymuHroziVybuch(GameState $state, TeamSide $strana, Position $cil): bool
    {
        foreach ($state->getPlayersOnPitch($strana) as $p) {
            $pos = $p->getPosition();
            if ($pos !== null && $pos->distanceTo($cil) <= 1) {
                return true;
            }
        }
        return false;
    }

    /**
     * Cena výbuchu na poli `$c` pro stranu `$strana` (C++ helpers.cpp bombSquareValue): cílové pole vždy, sousední
     * na 4+; stojící soupeř +1 (ležící 0,3), náš −2 (turnover) × šance zásahu.
     *
     * @return array{0: float, 1: bool} cena, zasáhne našeho
     */
    private function cenaVybuchu(GameState $state, Position $c, TeamSide $strana): array
    {
        $v = 0.0;
        $nas = false;
        for ($dx = -1; $dx <= 1; $dx++) {
            for ($dy = -1; $dy <= 1; $dy++) {
                $q = new Position($c->getX() + $dx, $c->getY() + $dy);
                if (!$q->isOnPitch()) {
                    continue;
                }
                $p = $state->getPlayerAtPosition($q);
                if ($p === null) {
                    continue;
                }
                $zasah = ($dx === 0 && $dy === 0) ? 1.0 : 0.5;
                if ($p->getTeamSide() === $strana) {
                    $nas = true;
                    $v -= 2.0 * $zasah;
                    continue;
                }
                $v += $zasah * ($p->getState() === PlayerState::STANDING ? 1.0 : 0.3);
            }
        }
        return [$v, $nas];
    }

    /** Cíl povinného hodu dál (C++ chooseBombRethrowTarget): nejvyšší cena, při shodě nejdál od našich. */
    private function vyberCilHoduDal(GameState $state, MatchPlayerDTO $drzi): ?Position
    {
        $odkud = $drzi->requirePosition();
        $nej = -1e9;
        $nejDal = -1;
        $cil = null;
        for ($x = 0; $x < Position::PITCH_WIDTH; $x++) {
            for ($y = 0; $y < Position::PITCH_HEIGHT; $y++) {
                $c = new Position($x, $y);
                if ($c->equals($odkud) || ThrowTarget::posud($state, $drzi, $odkud, $c)['chyba'] !== null) {
                    continue;
                }
                [$v, $nas] = $this->cenaVybuchu($state, $c, $drzi->getTeamSide());
                if ($nas && $v <= 0.0) {
                    continue;
                }
                $dal = 99;
                foreach ($state->getPlayersOnPitch($drzi->getTeamSide()) as $p) {
                    $pp = $p->getPosition();
                    if ($pp !== null) {
                        $dal = min($dal, $pp->distanceTo($c));
                    }
                }
                if ($v > $nej || ($v === $nej && $dal > $nejDal)) {
                    $nej = $v;
                    $nejDal = $dal;
                    $cil = $c;
                }
            }
        }
        return $cil;
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
