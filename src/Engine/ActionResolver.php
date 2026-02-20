<?php
declare(strict_types=1);

namespace App\Engine;

use App\DTO\ActionResult;
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
use App\Engine\Action\SetupHandler;
use App\Engine\Action\ThrowTeamMateHandler;
use App\Enum\ActionType;

final class ActionResolver
{
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
        $this->bigGuyCheckResolver = new BigGuyCheckResolver();

        $this->moveHandler = new MoveHandler($dice, $this->tzCalc, $pathfinder, $this->ballResolver);
        $this->blockHandler = new BlockHandler($dice, $strCalc, $this->tzCalc, $injuryResolver, $this->ballResolver);
        $this->blockHandler->setPassResolver($this->getPassResolver());
        $this->blitzHandler = new BlitzHandler($this->moveHandler, $this->blockHandler, $pathfinder);
        $this->foulHandler = new FoulHandler($dice, $injuryResolver, $this->ballResolver);
        $this->handOffHandler = new HandOffHandler($this->ballResolver, $dice);
        $this->ttmHandler = new ThrowTeamMateHandler($dice, $this->tzCalc, $this->scatterCalc, $injuryResolver, $this->ballResolver);
        $this->bombThrowHandler = new BombThrowHandler($dice, $this->tzCalc, $this->scatterCalc, $injuryResolver, $this->ballResolver);
        $this->hypnoticGazeHandler = new HypnoticGazeHandler($dice, $this->tzCalc);
        $this->ballAndChainHandler = new BallAndChainHandler($dice, $strCalc, $this->tzCalc, $injuryResolver, $this->ballResolver, $this->scatterCalc);
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

    public function getBallResolver(): BallResolver
    {
        return $this->ballResolver;
    }

    public function getGameFlowResolver(): GameFlowResolver
    {
        return $this->gameFlowResolver;
    }

    /**
     * @param array<string, mixed> $params
     */
    public function resolve(GameState $state, ActionType $action, array $params): ActionResult
    {
        // Big Guy pre-action check for player actions
        if ($action->requiresPlayer() && isset($params['playerId'])) {
            $player = $state->getPlayer((int) $params['playerId']);
            if ($player !== null) {
                $checkResult = $this->bigGuyCheckResolver->resolvePreActionCheck(
                    $state, $player, $action, $this->dice,
                );
                if ($checkResult !== null) {
                    if (!empty($checkResult['proceed'])) {
                        // Bloodlust bite: state modified but action continues
                        $state = $checkResult['state'];
                        $preEvents = $checkResult['events'];
                    } else {
                        return ActionResult::success($checkResult['state'], $checkResult['events']);
                    }
                }
            }
        }
        $preEvents ??= [];

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
            ActionType::END_TURN => $this->endTurnHandler->resolve($state, $params),
            ActionType::SETUP_PLAYER => $this->setupHandler->resolve($state, $params),
            ActionType::END_SETUP => $this->setupHandler->resolveEndSetup($state),
        };

        // Prepend pre-action events (e.g. Bloodlust bite)
        if ($preEvents !== []) {
            $allEvents = array_merge($preEvents, $result->getEvents());
            if ($result->isTurnover()) {
                return ActionResult::turnover($result->getNewState(), $allEvents);
            }
            return ActionResult::success($result->getNewState(), $allEvents);
        }

        return $result;
    }
}
