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
use App\Enum\Weather;
use App\ValueObject\Position;
use App\DTO\PendingRerollDTO;
use App\Engine\BallResolver;
use App\Engine\DiceRollerInterface;
use App\Engine\ProCheck;
use App\Engine\ThrowReroll;
use App\Engine\Pathfinder;
use App\Engine\InjuryResolver;
use App\Engine\TacklezoneCalculator;

final class MoveHandler implements ActionHandlerInterface
{
    private bool $interactiveRerolls = false;

    public function __construct(
        private readonly DiceRollerInterface $dice,
        private readonly TacklezoneCalculator $tzCalc,
        private readonly Pathfinder $pathfinder,
        private readonly BallResolver $ballResolver,
        // Povinny (review P186, L7): dokud byl `?InjuryResolver = null`, `knockDownAt` bez
        //   nej tise vynechal hod na brneni a zraneni (r. 498-499, 1702-1703).
        private readonly InjuryResolver $injuryResolver,
    ) {}


    /**
     * Hrac byl SRAZEN (Knocked Down) pri pohybu -- neuspesny dodge, GFI nebo leap.
     *
     * PRAVIDLA r. 496-500: "...then the player is Knocked Down in the square he
     * was dodging to AND A ROLL MUST BE MADE TO SEE IF HE WAS INJURED (See Knock
     * Downs & Injuries). If the player is Knocked Down then his team suffers a
     * turnover and their turn ends immediately."
     *
     * VADA PHP27 (nalezena 11.09.2026): do te doby se tady hazelo POUZE na dodge/GFI
     * a hrac si lehl BEZ hodu na brneni a bez zraneni -- `MoveHandler` dokonce
     * `InjuryResolver` vubec nedostaval (`ActionResolver.php:73`). Uzivatel:
     * "po turnover se samozrejme musi vyhodnotit brneni a tak."
     *
     * Poradi je stejne jako v `BlockHandler`: nejdriv zraneni, teprve pak mic.
     *
     * OPRAVENO 08.10.2026 (audit parity, nalezy 4a a 4b):
     *   4a -- po neuspesnem uhybu zustaval hrac lezet na VYCHOZIM poli (u GFI a Leapu
     *         se `withPosition($to)` delalo, u uhybu chybelo). Pravidla r. 497-498:
     *         "Knocked Down in the square he was dodging to"; GFI r. 1702-1703: "in the
     *         square that they moved to"; Leap r. 8280-8281: "in the square that he was
     *         leaping to".
     *   4b -- `RerollHandler` (pad po dialogu prehozu) mel vlastni kopii padu BEZ hodu na
     *         brneni a zraneni (r. 498-499, 1703).
     *   Proto je pad pri pohybu JEDNA funkce: polozi hrace na pole `$pole`, hodi brneni
     *   a zraneni a pusti mic. Volaji ji vsechny tri pady zde i `RerollHandler`.
     *
     * @param list<GameEvent> $events
     * @return array{0: GameState, 1: list<GameEvent>}
     */
    public function knockDownAt(
        GameState $state,
        MatchPlayerDTO $player,
        Position $pole,
        array $events,
    ): array {
        $fallenPlayer = $player
            ->withState(PlayerState::PRONE)
            ->withPosition($pole)
            ->withHasMoved(true)
            ->withHasActed(true)
            ->withMovementRemaining(0);
        $state = $state->withPlayer($fallenPlayer);

        // Nosic pousti mic "in the square where they fall" (r. 678-679) -- mic se s nim
        //   presouva na pole padu driv, nez ho zraneni pripadne odnese ze hriste.
        $wasBallCarrier = $state->getBall()->getCarrierId() === $fallenPlayer->getId();
        if ($wasBallCarrier) {
            $state = $state->withBall(BallState::carried($pole, $fallenPlayer->getId()));
        }

        $injResult = $this->injuryResolver->resolve($fallenPlayer, $this->dice);
        $fallenPlayer = $injResult['player'];
        $state = $state->withPlayer($fallenPlayer);
        $events = array_merge($events, $injResult['events']);

        if ($wasBallCarrier) {
            [$state, $events] = $this->ballResolver->handleBallOnPlayerDown($state, $fallenPlayer, $events);
        } else {
            // Spadl na pole, kde lezi volny mic (sel si pro nej a neuhnul / nedobehl):
            //   mic odskoci (r. 896-900) -- i kdyz hrace zraneni vzapeti odnese ze hriste.
            [$state, $events] = $this->ballResolver->bounceLooseBallUnderPlayer($state, $pole, $events);
        }

        return [$state, $events];
    }

    public function setInteractiveRerolls(bool $interactive): void
    {
        $this->interactiveRerolls = $interactive;
    }

    /**
     * @param array<string, mixed> $params
     */
    public function resolve(GameState $state, array $params): ActionResult
    {
        $playerId = (int) $params['playerId'];
        $targetX = (int) $params['x'];
        $targetY = (int) $params['y'];
        $destination = new Position($targetX, $targetY);
        $partial = (bool) ($params['partial'] ?? false);

        $player = $state->getPlayer($playerId);
        if ($player === null) {
            throw new \InvalidArgumentException("Player {$playerId} not found");
        }

        // Pohyb (i pouhé vstání) začal: od teď platí zákazy "may not move" -- akce Block
        //   (`rules_bb2016.txt` ř. 674-676), hod bombou (ř. 7953-7954). Nastavuje se HNED,
        //   ne až s `hasMoved` na konci: pohyb přerušený dialogem přehozu se neukončí.
        $player = $player->withMovedThisTurn(true);
        $state = $state->withPlayer($player);

        $events = [];

        // Handle PRONE player stand-up
        if ($player->getState() === PlayerState::PRONE) {
            $standUpResult = $this->resolveStandUp($state, $player);
            $events = array_merge($events, $standUpResult['events']);
            $state = $standUpResult['state'];

            if (!$standUpResult['success']) {
                // Failed stand-up: player stays prone, action done (not a turnover)
                return ActionResult::success($state, $events);
            }

            $player = $state->getPlayer($playerId);
            if ($player === null) {
                throw new \InvalidArgumentException("Player {$playerId} not found after stand-up");
            }

            // Stand in place only
            if ($player->getPosition() !== null && $player->getPosition()->equals($destination)) {
                return ActionResult::success($this->ukoncitKrok($state, $player, $partial), $events);
            }
        }

        $path = $this->pathfinder->findPathTo($state, $player, $destination);
        if ($path === null) {
            throw new \InvalidArgumentException("No valid path to ({$targetX},{$targetY})");
        }

        $currentState = $state;
        $currentPlayer = $player;

        // Track team reroll availability
        $activeSide = $currentState->getActiveTeam();
        $teamRerollUsed = false;

        foreach ($path->getSteps() as $step) {
            $from = $currentPlayer->getPosition();
            $to = $step->getPosition();

            // Leap (ř. 8264-8277). OPRAVENO 10.11.2026 (P277, audit goblinů; parita s C++ move_handler.cpp resolveLeap):
            //   dřív bez GFI, bez Tentacles / Shadowing, bez přehozu, zóny u cíle v hodu a skok jen v rámci jedné cesty.
            if ($step->isLeap()) {
                // Tentacles platí, když soupeř „attempts to dodge OR LEAP out of any of his tackle zones“ (ř. 8586-8587)
                if ($from !== null && $this->chapadlaDrzi($currentState, $currentPlayer, $from, $events)) {
                    return ActionResult::success($currentState->withPlayer($currentPlayer->withHasMoved(true)), $events);
                }
                $currentPlayer = $currentPlayer->withLeapUsedThisTurn(true);
                $currentState = $currentState->withPlayer($currentPlayer);
                // skok stojí 2 pole; každé pole nad zbylý pohyb = jeden GFI (ř. 1701)
                $gfiSkoku = max(0, min(2, 2 - max(0, $currentPlayer->getMovementRemaining())));

                $leapTarget = Pathfinder::cilSkoku($currentPlayer);
                $roll = $this->dice->rollD6();
                $success = $roll >= $leapTarget;
                $events[] = GameEvent::leap($playerId, $leapTarget, $roll, $success);
                if (!$success) {
                    // P289: kouč-člověk dostane nabídku přehozu (ř. 929-931)
                    $nabidka = $this->nabidkaPrehozu($currentState, $currentPlayer, 'leap', $leapTarget, $roll, $to, $gfiSkoku, $teamRerollUsed);
                    if ($nabidka !== null) {
                        return ActionResult::success($currentState->withPendingReroll($nabidka), $events);
                    }
                    $success = $this->prehozAi(
                        $currentState,
                        $currentPlayer,
                        $leapTarget,
                        $teamRerollUsed,
                        $events,
                        static fn(int $r, bool $ok) => GameEvent::leap($playerId, $leapTarget, $r, $ok),
                    );
                }
                if (!$success) {
                    return $this->skokSelhal($currentState, $currentPlayer, $to, $events);
                }
                $konec = $this->dokoncitSkok($currentState, $currentPlayer, $from, $to, $gfiSkoku, $teamRerollUsed, $events);
                if ($konec !== null) {
                    return $konec;
                }

                continue; // skip normal dodge/GFI for this step
            }

            // Tentacles check before dodge
            // ⛔ P85 (30.09.2026), vzor C++ f17802d1 (`rules_bb2016.txt` r. 8588-8595):
            //   "The opposing player rolls 2D6 adding their own player's ST and
            //   subtracting the Tentacles player's ST ... 5 or less = held firm"
            //   a "only ONE of the opposing players may attempt to grab him".
            //   PHP hazel D6 proti D6 a zkousel to KAZDY chapadlar zvlast.
            //   Z vic chapadlaru zkousi ten nejsilnejsi (nejlepsi sance pro soupere).
            if ($step->requiresDodge() && $from !== null && $this->chapadlaDrzi($currentState, $currentPlayer, $from, $events)) {
                // Movement ends, NOT a turnover
                return ActionResult::success($currentState->withPlayer($currentPlayer->withHasMoved(true)), $events);
            }

            // Dodge roll
            if ($step->requiresDodge()) {
                // ⛔ OPRAVA 21.09.2026 (`rules_bb2016.txt` r. 7988-7991): Break
                //   Tackle je 1x za kolo, takze cil uhybu uz nelze brat
                //   predpocitany z cesty -- ta se pocita pred prvnim krokem,
                //   kdy je skill jeste k dispozici. Pocita se znovu ze
                //   SOUCASNEHO stavu hrace.
                $breakTackleTed = $this->tzCalc->breakTackleAvailable($currentPlayer);
                $target = $from !== null
                    ? $this->tzCalc->calculateDodgeTarget($currentState, $currentPlayer, $to, $from)
                    : $step->getDodgeTarget();

                if ($breakTackleTed) {
                    $currentPlayer = $currentPlayer->withBreakTackleUsedThisTurn(true);
                    $currentState = $currentState->withPlayer($currentPlayer);
                }

                $roll = $this->dice->rollD6();
                $success = $roll >= $target;
                $events[] = GameEvent::dodgeAttempt($playerId, $target, $roll, $success);

                if (!$success) {
                    // Try Dodge skill reroll (negated by adjacent enemy with Tackle)
                    // ⛔ OPRAVA 21.09.2026 (`rules_bb2016.txt` r. 960-962, 8089-8090):
                    //   "the player may only re-roll one failed Dodge roll per turn".
                    //   Do ted slo Dodgem zachranit kazdy uhyb v tahu.
                    $skillRerollUsed = false;
                    if ($currentPlayer->hasSkill(SkillName::Dodge) && !$currentPlayer->isDodgeUsedThisTurn()) {
                        $tackleNegatesDodge = false;
                        if ($from !== null) {
                            foreach ($this->tzCalc->getMarkingPlayers($currentState, $from, $currentPlayer->getTeamSide()) as $marker) {
                                if ($marker->hasSkill(SkillName::Tackle)) {
                                    $tackleNegatesDodge = true;
                                    break;
                                }
                            }
                        }

                        if (!$tackleNegatesDodge) {
                            $skillRerollUsed = true;
                            $currentPlayer = $currentPlayer->withDodgeUsedThisTurn(true);
                            $currentState = $currentState->withPlayer($currentPlayer);
                            $roll = $this->dice->rollD6();
                            $success = $roll >= $target;
                            $events[] = GameEvent::rerollUsed($playerId, 'Dodge');
                            $events[] = GameEvent::dodgeAttempt($playerId, $target, $roll, $success);
                        }
                    }

                    if (!$success) {
                        // Check for interactive reroll opportunity
                        // ⛔ 18.09.2026: r. 926 -- po Dodge prehozu uz ani Pro
                        $proAvail = !$skillRerollUsed && $currentPlayer->hasSkill(SkillName::Pro) && !$currentPlayer->isProUsedThisTurn();
                        $teamRerollAvail = !$skillRerollUsed && !$teamRerollUsed && $currentState->getTeamState($activeSide)->canUseReroll();

                        if ($this->interactiveRerolls && ($proAvail || $teamRerollAvail)) {
                            $pending = new PendingRerollDTO(
                                rollType: 'dodge',
                                playerId: $playerId,
                                target: $target,
                                roll: $roll,
                                proAvailable: $proAvail,
                                teamRerollAvailable: $teamRerollAvail,
                                targetX: $to->getX(),
                                targetY: $to->getY(),
                            );
                            $currentState = $currentState->withPlayer($currentPlayer);
                            $currentState = $currentState->withPendingReroll($pending);
                            return ActionResult::success($currentState, $events);
                        }
                    }

                    // ⛔ OPRAVA 18.09.2026 (`rules_bb2016.txt` r. 8381-8387, 926): Pro jen
                    //   na neprehozenou kostku; po Pro tymovy prehoz jen hodu Pro (`ProCheck`).
                    $proUsed = false;
                    if (!$success && !$skillRerollUsed && $currentPlayer->hasSkill(SkillName::Pro) && !$currentPlayer->isProUsedThisTurn()) {
                        $proUsed = true;
                        $currentPlayer = $currentPlayer->withProUsedThisTurn(true);
                        $currentState = $currentState->withPlayer($currentPlayer);
                        $pro = ProCheck::roll(
                            $this->dice,
                            $currentPlayer,
                            !$teamRerollUsed && $currentState->getTeamState($activeSide)->canUseReroll(),
                            $events,
                        );
                        if ($pro['teamRerollUsed']) {
                            $teamRerollUsed = true;
                            $currentState = $currentState->withTeamState(
                                $activeSide,
                                $currentState->getTeamState($activeSide)->withRerollUsed(),
                            );
                        }
                        if ($pro['allowed']) {
                            $roll = $this->dice->rollD6();
                            $success = $roll >= $target;
                            $events[] = GameEvent::dodgeAttempt($playerId, $target, $roll, $success);
                        }
                    }

                    // Non-interactive: Try team reroll (only if no skill reroll and no Pro was used)
                    if (!$success && !$skillRerollUsed && !$proUsed && !$teamRerollUsed && $currentState->getTeamState($activeSide)->canUseReroll()) {
                        $teamRerollUsed = true;
                        $currentState = $currentState->withTeamState(
                            $activeSide,
                            $currentState->getTeamState($activeSide)->withRerollUsed(),
                        );
                        $lonerBlocked = false;
                        if ($currentPlayer->hasSkill(SkillName::Loner)) {
                            $lonerRoll = $this->dice->rollD6();
                            $lonerBlocked = $lonerRoll < 4;
                            $events[] = GameEvent::lonerCheck($playerId, $lonerRoll, !$lonerBlocked);
                        }
                        if (!$lonerBlocked) {
                            $roll = $this->dice->rollD6();
                            $success = $roll >= $target;
                            $events[] = GameEvent::rerollUsed($playerId, 'Team Reroll');
                            $events[] = GameEvent::dodgeAttempt($playerId, $target, $roll, $success);
                        }
                    }
                }

                if (!$success) {
                    // Failed dodge - player falls, turnover
                    $events[] = GameEvent::playerFell($playerId);
                    $events[] = GameEvent::turnover('Failed dodge');

                    // Pada na poli, KAM uhybal (r. 497-498) -- viz `knockDownAt`.
                    [$currentState, $events] = $this->knockDownAt($currentState, $currentPlayer, $to, $events);

                    return ActionResult::turnover(
                        $currentState->withTurnoverPending(true),
                        $events,
                    );
                }

                // Diving Tackle: after successful dodge, DT player goes prone
                if ($from !== null) {
                    foreach ($this->tzCalc->getMarkingPlayers($currentState, $from, $currentPlayer->getTeamSide()) as $marker) {
                        if (!$marker->hasSkill(SkillName::DivingTackle)) {
                            continue;
                        }
                        $dtPlayer = $marker->withState(PlayerState::PRONE);
                        $currentState = $currentState->withPlayer($dtPlayer);
                        $events[] = GameEvent::divingTackle($marker->getId());
                        [$currentState, $events] = $this->ballResolver->handleBallOnPlayerDown($currentState, $dtPlayer, $events);
                        break;
                    }
                }

                // Shadowing: opponent with Shadowing may follow after successful dodge
                if ($from !== null) {
                    $currentState = $this->stin($currentState, $currentPlayer, $from, $events);
                }
            }

            // GFI roll
            if ($step->isGfi()) {
                $gfiThreshold = $currentState->getWeather() === Weather::BLIZZARD ? 3 : 2;
                $roll = $this->dice->rollD6();
                $success = $roll >= $gfiThreshold;
                $events[] = GameEvent::gfiAttempt($playerId, $roll, $success);

                // Sure Feet: reroll failed GFI
                // ⛔ OPRAVA 18.09.2026: `rules_bb2016.txt` r. 8540-8541 -- "A player may
                //   only use the Sure Feet skill once per turn." Limit chybel.
                $skillRerollUsedGfi = false;
                if (!$success && $currentPlayer->hasSkill(SkillName::SureFeet) && !$currentPlayer->isSureFeetUsedThisTurn()) {
                    $skillRerollUsedGfi = true;
                    $currentPlayer = $currentPlayer->withSureFeetUsedThisTurn(true);
                    $currentState = $currentState->withPlayer($currentPlayer);
                    $roll = $this->dice->rollD6();
                    $success = $roll >= $gfiThreshold;
                    $events[] = GameEvent::rerollUsed($playerId, 'Sure Feet');
                    $events[] = GameEvent::gfiAttempt($playerId, $roll, $success);
                }

                if (!$success) {
                    // Check for interactive reroll opportunity
                    // ⛔ 18.09.2026: r. 926 -- po Sure Feet prehozu uz ani Pro
                    $proAvailGfi = !$skillRerollUsedGfi && $currentPlayer->hasSkill(SkillName::Pro) && !$currentPlayer->isProUsedThisTurn();
                    $teamRerollAvailGfi = !$skillRerollUsedGfi && !$teamRerollUsed && $currentState->getTeamState($activeSide)->canUseReroll();

                    if ($this->interactiveRerolls && ($proAvailGfi || $teamRerollAvailGfi)) {
                        $pending = new PendingRerollDTO(
                            rollType: 'gfi',
                            playerId: $playerId,
                            target: $gfiThreshold,
                            roll: $roll,
                            proAvailable: $proAvailGfi,
                            teamRerollAvailable: $teamRerollAvailGfi,
                            targetX: $to->getX(),
                            targetY: $to->getY(),
                        );
                        $currentState = $currentState->withPlayer($currentPlayer);
                        $currentState = $currentState->withPendingReroll($pending);
                        return ActionResult::success($currentState, $events);
                    }
                }

                // ⛔ OPRAVA 18.09.2026: tataz pravidla Pro jako u uhybu (r. 8381-8387, 926).
                $proUsedGfi = false;
                if (!$success && !$skillRerollUsedGfi && $currentPlayer->hasSkill(SkillName::Pro) && !$currentPlayer->isProUsedThisTurn()) {
                    $proUsedGfi = true;
                    $currentPlayer = $currentPlayer->withProUsedThisTurn(true);
                    $currentState = $currentState->withPlayer($currentPlayer);
                    $pro = ProCheck::roll(
                        $this->dice,
                        $currentPlayer,
                        !$teamRerollUsed && $currentState->getTeamState($activeSide)->canUseReroll(),
                        $events,
                    );
                    if ($pro['teamRerollUsed']) {
                        $teamRerollUsed = true;
                        $currentState = $currentState->withTeamState(
                            $activeSide,
                            $currentState->getTeamState($activeSide)->withRerollUsed(),
                        );
                    }
                    if ($pro['allowed']) {
                        $roll = $this->dice->rollD6();
                        $success = $roll >= $gfiThreshold;
                        $events[] = GameEvent::gfiAttempt($playerId, $roll, $success);
                    }
                }

                // Non-interactive: Try team reroll (only if no skill reroll and no Pro was used)
                if (!$success && !$skillRerollUsedGfi && !$proUsedGfi && !$teamRerollUsed && $currentState->getTeamState($activeSide)->canUseReroll()) {
                    $teamRerollUsed = true;
                    $currentState = $currentState->withTeamState(
                        $activeSide,
                        $currentState->getTeamState($activeSide)->withRerollUsed(),
                    );
                    $lonerBlocked = false;
                    if ($currentPlayer->hasSkill(SkillName::Loner)) {
                        $lonerRoll = $this->dice->rollD6();
                        $lonerBlocked = $lonerRoll < 4;
                        $events[] = GameEvent::lonerCheck($playerId, $lonerRoll, !$lonerBlocked);
                    }
                    if (!$lonerBlocked) {
                        $roll = $this->dice->rollD6();
                        $success = $roll >= $gfiThreshold;
                        $events[] = GameEvent::rerollUsed($playerId, 'Team Reroll');
                        $events[] = GameEvent::gfiAttempt($playerId, $roll, $success);
                    }
                }

                if (!$success) {
                    // Failed GFI - player falls, turnover
                    $events[] = GameEvent::playerFell($playerId);
                    $events[] = GameEvent::turnover('Failed Going For It');

                    // Pada na poli, kam dosel (r. 1702-1703) -- viz `knockDownAt`.
                    [$currentState, $events] = $this->knockDownAt($currentState, $currentPlayer, $to, $events);

                    return ActionResult::turnover(
                        $currentState->withTurnoverPending(true),
                        $events,
                    );
                }
            }

            // Move succeeded for this step
            $fromStr = $from !== null ? (string) $from : '?';
            $events[] = GameEvent::playerMove($playerId, $fromStr, (string) $to);

            $currentPlayer = $currentPlayer
                ->withPosition($to)
                ->withMovementRemaining($currentPlayer->getMovementRemaining() - 1);
            $currentState = $currentState->withPlayer($currentPlayer);

            // Move ball with carrier
            if ($currentState->getBall()->getCarrierId() === $playerId) {
                $currentState = $currentState->withBall(BallState::carried($to, $playerId));
            }

            // Check if player moved onto a loose ball
            $konec = $this->zvednoutNaPoli($currentState, $currentPlayer, $to, $teamRerollUsed, $events);
            if ($konec !== null) {
                return $konec;
            }
        }

        return ActionResult::success($this->ukoncitKrok($currentState, $currentPlayer, $partial), $events);
    }

    /**
     * Konec pohybu k cíli. P296 (11.11.2026): dílčí krok kouče (`partial`) pohyb neukončí — hráč smí táhnout dál
     * jako táž akce (`GameState::openMovePlayerId`); aktivace se uzavře, až jedná jiný hráč nebo skončí kolo
     * (`rules_bb2016.txt` ř. 423-429, `ActionResolver::resolveAkce`). Jinak je pohyb ukončen (i pokračování P81).
     */
    private function ukoncitKrok(GameState $state, MatchPlayerDTO $hrac, bool $partial): GameState
    {
        // dílčí krok: pohyb zůstává otevřený (aktivní hráč bez `hasMoved`, `GameState::openMovePlayerId`)
        return $state->withPlayer($partial ? $hrac : $hrac->withHasMoved(true));
    }

    /**
     * Tentacles (`rules_bb2016.txt` ř. 8586-8595): úhyb nebo skok z jeho zóny; 2D6 + ST soupeře − ST chapadláře,
     * 5 a míň = drží (pohyb končí, ne turnover). Z víc chapadlářů zkouší nejsilnější. Vrací true = drží.
     *
     * @param list<GameEvent> $events
     */
    private function chapadlaDrzi(GameState $state, MatchPlayerDTO $hrac, Position $from, array &$events): bool
    {
        $grabber = null;
        foreach ($this->tzCalc->getMarkingPlayers($state, $from, $hrac->getTeamSide()) as $marker) {
            if ($marker->hasSkill(SkillName::Tentacles)
                && ($grabber === null || $marker->getStats()->getStrength() > $grabber->getStats()->getStrength())) {
                $grabber = $marker;
            }
        }
        if ($grabber === null) {
            return false;
        }
        $roll = $this->dice->rollD6() + $this->dice->rollD6();
        $total = $roll + $hrac->getStats()->getStrength() - $grabber->getStats()->getStrength();
        $escaped = $total > 5;
        $events[] = GameEvent::tentacles($hrac->getId(), $grabber->getId(), $roll, $total, $escaped);
        return !$escaped;
    }

    /**
     * Shadowing (ř. 8456-8464): po opuštění zóny ze `$from` jeden soupeř se Shadowing hodí 2D6 + MA hráče − své MA,
     * 7 a míň = následuje na `$from`. P85 (30.09.2026), vzor C++ f17802d1.
     *
     * @param list<GameEvent> $events
     */
    private function stin(GameState $state, MatchPlayerDTO $hrac, Position $from, array &$events): GameState
    {
        foreach ($this->tzCalc->getMarkingPlayers($state, $from, $hrac->getTeamSide()) as $marker) {
            if (!$marker->hasSkill(SkillName::Shadowing)) {
                continue;
            }
            $shadowRoll = $this->dice->rollD6() + $this->dice->rollD6();
            $followed = ($shadowRoll + $hrac->getStats()->getMovement() - $marker->getStats()->getMovement()) <= 7;
            $events[] = GameEvent::shadowing($marker->getId(), $hrac->getId(), $shadowRoll, $followed);
            if ($followed) {
                $state = $state->withPlayer($marker->withPosition($from));
            }
            break; // jen jeden pokus
        }
        return $state;
    }

    /**
     * Vstoupil hráč na pole s volným míčem ⇒ musí ho zvednout (`rules_bb2016.txt` ř. 436-438); týmový přehoz smí jen
     * jednou za kolo. Nepovedené zvednutí = turnover (vrací výsledek), jinak `null` a hráč (případně s míčem) ve stavu.
     *
     * @param list<GameEvent> $events
     */
    private function zvednoutNaPoli(GameState &$state, MatchPlayerDTO &$hrac, Position $to, bool &$teamRerollUsed, array &$events): ?ActionResult
    {
        $ball = $state->getBall();
        if ($ball->isHeld() || $ball->getPosition() === null || !$ball->getPosition()->equals($to)) {
            return null;
        }
        $playerId = $hrac->getId();
        $strana = $state->getActiveTeam();
        $hrac = $state->getPlayer($playerId) ?? $hrac;
        $pickupResult = $this->ballResolver->resolvePickup($state, $hrac, !$teamRerollUsed && $state->getTeamState($strana)->canUseReroll());
        $events = array_merge($events, $pickupResult['events']);
        $state = $pickupResult['state'];
        if ($pickupResult['teamRerollUsed']) {
            $teamRerollUsed = true;
            $state = $state->withTeamState($strana, $state->getTeamState($strana)->withRerollUsed());
        }
        if (!$pickupResult['success']) {
            $events[] = GameEvent::turnover('Failed pickup');
            $state = $state->withPlayer(($state->getPlayer($playerId) ?? $hrac)->withHasMoved(true)->withHasActed(true));

            return ActionResult::turnover($state->withTurnoverPending(true), $events);
        }
        $hrac = $state->getPlayer($playerId) ?? $hrac;

        return null;
    }

    /** P289: nabídka přehozu kouči-člověku (jen v interaktivním režimu); AI dostane `null`. */
    private function nabidkaPrehozu(GameState $state, MatchPlayerDTO $hrac, string $druh, int $cil, int $hod, Position $kam, int $gfiZbyva, bool $teamRerollUsed): ?PendingRerollDTO
    {
        return $this->interactiveRerolls
            ? ThrowReroll::offer($state, $hrac, $druh, $cil, $hod, $kam, !$teamRerollUsed, $gfiZbyva)
            : null;
    }

    /**
     * Nepovedený skok nebo jeho GFI: sražen v cílovém poli + brnění (ř. 8274-8276, 1702-1703), turnover.
     *
     * @param list<GameEvent> $events
     */
    public function skokSelhal(GameState $state, MatchPlayerDTO $hrac, Position $to, array $events): ActionResult
    {
        $events[] = GameEvent::playerFell($hrac->getId());
        $events[] = GameEvent::turnover('Failed leap');
        [$state, $events] = $this->knockDownAt($state, $hrac, $to, $events);

        return ActionResult::turnover($state->withTurnoverPending(true), $events);
    }

    /**
     * Po povedeném hodu na skok (i přehozeném v dialogu, P289): GFI skoku, dopad, Shadowing, míč.
     *
     * @param list<GameEvent> $events
     */
    public function dokoncitSkokPoPrehozu(GameState $state, int $playerId, Position $to, int $gfi, array $events): ActionResult
    {
        $hrac = $state->requirePlayer($playerId);
        $teamRerollUsed = false;
        $konec = $this->dokoncitSkok($state, $hrac, $hrac->getPosition(), $to, $gfi, $teamRerollUsed, $events);

        // P296: pohyb po skoku zůstává otevřený (movedThisTurn bez hasMoved) — kouč smí táhnout dál, táž akce
        return $konec ?? ActionResult::success($state, $events);
    }

    /**
     * GFI skoku a dopad. Vrací výsledek, když tah skončí (pád, nabídka přehozu, nezvednutý míč), jinak `null`
     * a stav s hráčem v cílovém poli.
     *
     * @param list<GameEvent> $events
     */
    private function dokoncitSkok(GameState &$state, MatchPlayerDTO &$hrac, ?Position $from, Position $to, int $gfi, bool &$teamRerollUsed, array &$events): ?ActionResult
    {
        $playerId = $hrac->getId();
        for ($g = 0; $g < $gfi; $g++) {
            $gfiCil = $state->getWeather() === Weather::BLIZZARD ? 3 : 2;
            $roll = $this->dice->rollD6();
            $success = $roll >= $gfiCil;
            $events[] = GameEvent::gfiAttempt($playerId, $roll, $success);
            if (!$success && $hrac->hasSkill(SkillName::SureFeet) && !$hrac->isSureFeetUsedThisTurn()) {
                $hrac = $hrac->withSureFeetUsedThisTurn(true);
                $state = $state->withPlayer($hrac);
                $roll = $this->dice->rollD6();
                $success = $roll >= $gfiCil;
                $events[] = GameEvent::rerollUsed($playerId, 'Sure Feet');
                $events[] = GameEvent::gfiAttempt($playerId, $roll, $success);
            } elseif (!$success) {
                $nabidka = $this->nabidkaPrehozu($state, $hrac, 'leap_gfi', $gfiCil, $roll, $to, $gfi - $g - 1, $teamRerollUsed);
                if ($nabidka !== null) {
                    return ActionResult::success($state->withPendingReroll($nabidka), $events);
                }
                $success = $this->prehozAi(
                    $state,
                    $hrac,
                    $gfiCil,
                    $teamRerollUsed,
                    $events,
                    static fn(int $r, bool $ok) => GameEvent::gfiAttempt($playerId, $r, $ok),
                );
            }
            if (!$success) {
                return $this->skokSelhal($state, $hrac, $to, $events);
            }
        }

        // Successful leap: move player
        $fromStr = $from !== null ? (string) $from : '?';
        $events[] = GameEvent::playerMove($playerId, $fromStr, (string) $to);

        $hrac = $hrac
            ->withPosition($to)
            ->withMovementRemaining($hrac->getMovementRemaining() - 2);
        $state = $state->withPlayer($hrac);
        // Shadowing platí při opuštění zóny „for any reason“ (ř. 8456-8458), i po skoku
        if ($from !== null) {
            $state = $this->stin($state, $hrac, $from, $events);
        }

        if ($state->getBall()->getCarrierId() === $playerId) {
            $state = $state->withBall(BallState::carried($to, $playerId));
        }

        // Check pickup on landing
        $konec = $this->zvednoutNaPoli($state, $hrac, $to, $teamRerollUsed, $events);
        if ($konec !== null) {
            return $konec;
        }

        return null;
    }

    /**
     * P277: přehoz nepovedeného hodu bez dialogu (AI; ř. 929-931) — Pro (ř. 8381-8387, ProCheck), jinak týmový přehoz
     * (Loner 4+, ř. 8287-8290); jeden hod nejvýš jednou (ř. 925-927). Kouč-člověk (interaktivní přehozy) dostane
     * místo toho nabídku (`nabidkaPrehozu`, P289).
     *
     * @param list<GameEvent> $events
     * @param callable(int, bool): GameEvent $udalost
     */
    private function prehozAi(GameState &$state, MatchPlayerDTO &$hrac, int $cil, bool &$teamRerollUsed, array &$events, callable $udalost): bool
    {
        if ($this->interactiveRerolls) {
            return false;
        }
        $strana = $hrac->getTeamSide();
        $tymPred = $state->getTeamState($strana)->canUseReroll();
        $res = ThrowReroll::proThenTeam($this->dice, $state, $hrac, $strana, !$teamRerollUsed, $events);
        $state = $res['state'];
        $hrac = $res['thrower'];
        if ($tymPred && !$state->getTeamState($strana)->canUseReroll()) {
            $teamRerollUsed = true;
        }
        if (!$res['rerolled']) {
            return false;
        }
        $roll = $this->dice->rollD6();
        $ok = $roll >= $cil;
        $events[] = $udalost($roll, $ok);
        return $ok;
    }

    /**
     * Resolve stand-up for a prone player.
     * Jump Up: free stand-up (0 MA cost).
     * MA >= 3: automatic, costs 3 MA. MA < 3: roll 4+ required.
     *
     * @return array{state: GameState, events: list<GameEvent>, success: bool}
     */
    private function resolveStandUp(GameState $state, MatchPlayerDTO $player): array
    {
        $playerId = $player->getId();

        // Jump Up: stand up for free (0 MA cost)
        if ($player->hasSkill(SkillName::JumpUp)) {
            $events = [GameEvent::standUp($playerId)];
            $player = $player->withState(PlayerState::STANDING);
            $state = $state->withPlayer($player);
            return ['state' => $state, 'events' => $events, 'success' => true];
        }

        if ($player->getStats()->getMovement() < 3) {
            // Low MA: roll 4+ to stand
            $roll = $this->dice->rollD6();
            $success = $roll >= 4;
            $events = [GameEvent::standUp($playerId, $roll, $success)];

            if (!$success) {
                // Failed: player stays prone, action done
                $player = $player->withHasMoved(true)->withHasActed(true);
                $state = $state->withPlayer($player);
                return ['state' => $state, 'events' => $events, 'success' => false];
            }

            // Success: stand up with 0 MA remaining
            $player = $player
                ->withState(PlayerState::STANDING)
                ->withMovementRemaining(0);
            $state = $state->withPlayer($player);
            return ['state' => $state, 'events' => $events, 'success' => true];
        }

        // Normal stand-up: costs 3 MA
        $events = [GameEvent::standUp($playerId)];
        $player = $player
            ->withState(PlayerState::STANDING)
            ->withMovementRemaining($player->getMovementRemaining() - 3);
        $state = $state->withPlayer($player);

        return ['state' => $state, 'events' => $events, 'success' => true];
    }
}
