<?php
declare(strict_types=1);

namespace App\Engine;

use App\DTO\ActionResult;
use App\DTO\GameEvent;
use App\DTO\GameState;
use App\Engine\Action\BallAndChainHandler;
use App\Engine\Action\BlitzHandler;
use App\Engine\Action\BlockHandler;
use App\Engine\Action\BombThrowHandler;
use App\Engine\Action\EndTurnHandler;
use App\Engine\Action\FoulHandler;
use App\Engine\Action\HandOffHandler;
use App\Engine\Action\HypnoticGazeHandler;
use App\Engine\Action\MoveHandler;
use App\Engine\Action\RerollHandler;
use App\Engine\Action\SetupHandler;
use App\Engine\Action\ThrowTeamMateHandler;
use App\Enum\ActionType;

final class ActionResolver
{
    private bool $interactiveBlocks = false;
    private bool $interactiveRerolls = false;
    private readonly MoveHandler $moveHandler;
    private readonly BlockHandler $blockHandler;
    private readonly BlitzHandler $blitzHandler;
    private readonly FoulHandler $foulHandler;
    private readonly HandOffHandler $handOffHandler;
    private readonly SetupHandler $setupHandler;
    private readonly EndTurnHandler $endTurnHandler;
    private readonly ThrowTeamMateHandler $ttmHandler;
    private readonly BombThrowHandler $bombThrowHandler;
    private readonly HypnoticGazeHandler $hypnoticGazeHandler;
    private readonly BallAndChainHandler $ballAndChainHandler;
    private readonly RerollHandler $rerollHandler;
    private ?PassResolver $passResolver = null;
    private readonly DiceRollerInterface $dice;
    private readonly TacklezoneCalculator $tzCalc;
    private readonly ScatterCalculator $scatterCalc;
    private readonly BallResolver $ballResolver;
    private readonly GameFlowResolver $gameFlowResolver;
    private readonly BigGuyCheckResolver $bigGuyCheckResolver;

    public function __construct(
        DiceRollerInterface $dice,
        ?TacklezoneCalculator $tzCalc = null,
        ?Pathfinder $pathfinder = null,
        ?StrengthCalculator $strCalc = null,
        ?InjuryResolver $injuryResolver = null,
        ?BallResolver $ballResolver = null,
        ?ScatterCalculator $scatterCalc = null,
        ?PassResolver $passResolver = null,
        ?KickoffResolver $kickoffResolver = null,
        ?GameFlowResolver $gameFlowResolver = null,
    ) {
        $this->dice = $dice;
        $this->tzCalc = $tzCalc ?? new TacklezoneCalculator();
        $this->scatterCalc = $scatterCalc ?? new ScatterCalculator();
        $pathfinder = $pathfinder ?? new Pathfinder($this->tzCalc);
        $strCalc = $strCalc ?? new StrengthCalculator();
        $injuryResolver = $injuryResolver ?? new InjuryResolver();
        $this->ballResolver = $ballResolver ?? new BallResolver($dice, $this->tzCalc, $this->scatterCalc);
        $this->passResolver = $passResolver;
        $kickoffResolver = $kickoffResolver ?? new KickoffResolver($dice, $this->scatterCalc, $this->ballResolver);
        $this->gameFlowResolver = $gameFlowResolver ?? new GameFlowResolver($dice);
        // ⭐ 11.09.2026 (PHP22): Bloodlust potrebuje hod na zraneni a odraz
        //   mice, takze uz to neni bezzavisla trida.
        $this->bigGuyCheckResolver = new BigGuyCheckResolver($injuryResolver, $this->ballResolver);

        $this->moveHandler = new MoveHandler($dice, $this->tzCalc, $pathfinder, $this->ballResolver);
        $this->blockHandler = new BlockHandler($dice, $strCalc, $this->tzCalc, $injuryResolver, $this->ballResolver);
        $this->blockHandler->setPassResolver($this->getPassResolver());
        $this->blitzHandler = new BlitzHandler($this->moveHandler, $this->blockHandler, $pathfinder);
        $this->foulHandler = new FoulHandler($dice, $injuryResolver, $this->ballResolver);
        $this->handOffHandler = new HandOffHandler($this->ballResolver, $dice);
        $this->ttmHandler = new ThrowTeamMateHandler($dice, $this->tzCalc, $this->scatterCalc, $injuryResolver, $this->ballResolver);
        $this->bombThrowHandler = new BombThrowHandler($dice, $this->tzCalc, $this->scatterCalc, $injuryResolver, $this->ballResolver);
        $this->hypnoticGazeHandler = new HypnoticGazeHandler($dice, $this->tzCalc);
        $this->ballAndChainHandler = new BallAndChainHandler($dice, $injuryResolver, $this->ballResolver, $this->scatterCalc);
        $this->rerollHandler = new RerollHandler($dice, $this->ballResolver);
        $this->setupHandler = new SetupHandler($kickoffResolver);
        $this->endTurnHandler = new EndTurnHandler();
    }

    public function getPassResolver(): PassResolver
    {
        if ($this->passResolver === null) {
            $this->passResolver = new PassResolver($this->dice, $this->tzCalc, $this->scatterCalc, $this->ballResolver);
        }
        return $this->passResolver;
    }

    public function setInteractiveBlocks(bool $interactive): void
    {
        $this->interactiveBlocks = $interactive;
    }

    public function setInteractiveRerolls(bool $interactive): void
    {
        $this->interactiveRerolls = $interactive;
        $this->moveHandler->setInteractiveRerolls($interactive);
    }

    public function getBlockHandler(): Action\BlockHandler
    {
        return $this->blockHandler;
    }

    public function getBallResolver(): BallResolver
    {
        return $this->ballResolver;
    }

    public function getGameFlowResolver(): GameFlowResolver
    {
        return $this->gameFlowResolver;
    }

        /**
     * Odecte TYMOVY limit u akce, ktera propadla pred svym provedenim.
     *
     * ⭐ MOVE a BLOCK zadny tymovy limit nemaji -- ty se neodecitaji.
     *   Seznam je uzavreny, stejne jako v C++ `consumeDeclaredTeamAction`
     *   (`engine/src/action_resolver.cpp:27-37`).
     */
    /**
     * ⭐ PHP25: hrac se vedome neaktivuje. Zadne kostky, zadna udalost na
     *    hristi -- jen se pro tenhle tah odepise, aby kouc mohl pokracovat
     *    dalsim hracem misto toho, aby musel ukoncit kolo celemu tymu.
     *
     * ⛔ Nesmi tu vzniknout turnover ani zadny hod: viz vyjimka z kontroly
     *    pred akci v `resolve()`.
     *
     * @param array<string, mixed> $params
     */
    private function resolveStandPat(GameState $state, array $params): ActionResult
    {
        $playerId = (int) ($params['playerId'] ?? 0);
        $player = $state->getPlayer($playerId);
        if ($player === null) {
            throw new \InvalidArgumentException('Player not found');
        }

        $newState = $state->withPlayer(
            $player->withHasActed(true)->withHasMoved(true),
        );

        return ActionResult::success($newState, [
            GameEvent::standPat($playerId, $player->getName()),
        ]);
    }

    private function consumeDeclaredTeamAction(
        GameState $state,
        \App\Enum\TeamSide $side,
        ActionType $type,
    ): GameState {
        $team = $state->getTeamState($side);

        $team = match ($type) {
            ActionType::BLITZ => $team->withBlitzUsed(),
            ActionType::PASS,
            ActionType::THROW_TEAM_MATE => $team->withPassUsed(),
            ActionType::HAND_OFF => $team->withHandOffUsed(),
            ActionType::FOUL => $team->withFoulUsed(),
            default => $team,
        };

        return $state->withTeamState($side, $team);
    }

    /**
     * @param array<string, mixed> $params
     */
    public function resolve(GameState $state, ActionType $action, array $params): ActionResult
    {
        // Big Guy pre-action check for player actions
        /** @var list<\App\DTO\GameEvent> $preEvents */
        $preEvents = [];
        // ⛔⛔⛔ STAND_PAT SE Z KONTROLY PRED AKCI VYJIMA (PHP25, 11.09.2026).
        //   Uzivatel: "pro big guye to navic znamena se neaktivovat -- tak
        //   nemusi hazet po aktivaci." Kontrola pred akci je dusledek
        //   AKTIVACE hrace; kdyz hrac zustava stat, zadna aktivace neprobehla,
        //   takze se NEHAZI Bone Head, Really Stupid, Wild Animal, Bloodlust
        //   ani Take Root. Kdyby se sem STAND_PAT pustil, stalo by "nic
        //   nedelat" tolik co akce -- a big guy by se mohl omracit tim, ze
        //   se rozhodl nehrat.
        if ($action !== ActionType::STAND_PAT
            && $action->requiresPlayer() && isset($params['playerId'])) {
            $player = $state->getPlayer((int) $params['playerId']);
            if ($player !== null) {
                $checkResult = $this->bigGuyCheckResolver->resolvePreActionCheck(
                    $state, $player, $action, $this->dice,
                );
                if ($checkResult !== null) {
                    // ⛔ PHP22: kontrola pred akci umi skoncit i TURNOVEREM --
                    //   Bloodlust, kdyz upir nema koho kousnout (r. 7942-7943),
                    //   nebo kdyz kousnuty Thrall drzel mic (r. 7941-7942).
                    //   Do 11.09. tahle vetev neexistovala a kolo beželo dal.
                    if (!empty($checkResult['turnover'])) {
                        return ActionResult::turnover(
                            $checkResult['state']->withTurnoverPending(true),
                            $checkResult['events'],
                        );
                    }
                    if (!empty($checkResult['proceed'])) {
                        // Bloodlust bite: state modified but action continues
                        $state = $checkResult['state'];
                        $preEvents = $checkResult['events'];
                    } else {
                        $blockedState = $checkResult['state'];
                        // ⛔ PHP15 (11.09.2026): r. 8398-8401 -- „The player
                        //   can't do anything for the turn, and **the player's
                        //   team loses the declared Action for that turn**
                        //   (for example if a Really Stupid player declares
                        //   a Blitz Action and fails the Really Stupid roll,
                        //   then the team cannot declare another Blitz Action
                        //   that turn)." U Wild Animal r. 8668-8669 stejne:
                        //   „**the Action is wasted**."
                        //   PHP to nedelalo u ZADNE ze ctyr dovednosti ⇒ Big
                        //   Guy, ktery sel k zemi na blitzu, tym o blitz
                        //   NEPRIPRAVIL a ten si ho zahral znovu jinym hracem.
                        //   Odecita se TADY, protoze do `match` niz (kde se
                        //   tymovy limit jinak nastavuje) uz se nedostaneme.
                        // ⚠️ C++ to ma jako `wastesTeamAction`
                        //   (`action_resolver.cpp:276-283`); PHP kopie ne.
                        if (!empty($checkResult['wastesTeamAction'])) {
                            $blockedState = $this->consumeDeclaredTeamAction(
                                $blockedState, $player->getTeamSide(), $action,
                            );
                        }

                        return ActionResult::success($blockedState, $checkResult['events']);
                    }
                }
            }
        }

        $result = match ($action) {
            ActionType::MOVE => $this->moveHandler->resolve($state, $params),
            ActionType::BLOCK => $this->blockHandler->resolve($state, $params),
            ActionType::BLITZ => $this->blitzHandler->resolve($state, $params),
            ActionType::PASS => $this->getPassResolver()->resolve($state, $params),
            ActionType::HAND_OFF => $this->handOffHandler->resolve($state, $params),
            ActionType::THROW_TEAM_MATE => $this->ttmHandler->resolve($state, $params),
            ActionType::BOMB_THROW => $this->bombThrowHandler->resolve($state, $params),
            ActionType::HYPNOTIC_GAZE => $this->hypnoticGazeHandler->resolve($state, $params),
            ActionType::BALL_AND_CHAIN => $this->ballAndChainHandler->resolve($state, $params),
            ActionType::MULTIPLE_BLOCK => $this->blockHandler->resolveMultipleBlock($state, $params),
            ActionType::FOUL => $this->foulHandler->resolve($state, $params),
            ActionType::STAND_PAT => $this->resolveStandPat($state, $params),
            ActionType::END_TURN => $this->endTurnHandler->resolve($state, $params),
            ActionType::SETUP_PLAYER => $this->setupHandler->resolve($state, $params),
            ActionType::END_SETUP => $this->setupHandler->resolveEndSetup($state),
            ActionType::AUTO_SETUP => $this->setupHandler->resolveAutoSetup($state, $params),
            ActionType::CHOOSE_BLOCK_DIE => $this->blockHandler->resolveBlockChoice($state, $params),
            ActionType::REROLL_BLOCK => $this->blockHandler->resolveBlockReroll($state, $params),
            ActionType::RESOLVE_REROLL => $this->rerollHandler->resolve($state, $params),
        };

        // Prepend pre-action events (e.g. Bloodlust bite)
        if ($preEvents !== []) {
            $allEvents = array_merge($preEvents, $result->getEvents());
            if ($result->isTurnover()) {
                $result = ActionResult::turnover($result->getNewState(), $allEvents);
            } else {
                $result = ActionResult::success($result->getNewState(), $allEvents);
            }
        }

        // Auto-resolve pending blocks (unless interactive mode for human players)
        if (!$this->interactiveBlocks) {
            while ($result->getNewState()->getPendingBlock() !== null) {
                $autoResult = $this->blockHandler->autoResolvePendingBlock($result->getNewState());
                $mergedEvents = array_merge($result->getEvents(), $autoResult->getEvents());
                if ($autoResult->isTurnover()) {
                    $result = ActionResult::turnover($autoResult->getNewState(), $mergedEvents);
                    break;
                }
                $result = ActionResult::success($autoResult->getNewState(), $mergedEvents);
            }
        }

        // Auto-resolve pending rerolls (unless interactive mode for human players)
        if (!$this->interactiveRerolls) {
            while ($result->getNewState()->getPendingReroll() !== null) {
                $autoResult = $this->rerollHandler->autoResolve($result->getNewState());
                $mergedEvents = array_merge($result->getEvents(), $autoResult->getEvents());
                if ($autoResult->isTurnover()) {
                    $result = ActionResult::turnover($autoResult->getNewState(), $mergedEvents);
                    break;
                }
                $result = ActionResult::success($autoResult->getNewState(), $mergedEvents);
            }
        }

        return $result;
    }
}
