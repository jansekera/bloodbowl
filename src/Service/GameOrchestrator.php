<?php

declare(strict_types=1);

namespace App\Service;

use App\AI\AICoachInterface;
use App\AI\GameLogger;
use App\DTO\ActionResult;
use App\DTO\GameEvent;
use App\DTO\GameState;
use App\Engine\ActionResolver;
use App\Engine\DiceRollerInterface;
use App\Engine\RulesEngine;
use App\Enum\ActionType;
use App\Enum\GamePhase;
use App\Exception\ValidationException;

final class GameOrchestrator
{
    private ?GameLogger $gameLogger = null;

    public function __construct(
        private readonly RulesEngine $rulesEngine,
        private readonly DiceRollerInterface $dice,
        private readonly AICoachInterface $aiCoach,
    ) {
    }

    public function setGameLogger(GameLogger $logger): void
    {
        $this->gameLogger = $logger;
    }

    /**
     * Execute an action and handle all post-action orchestration
     * (turnover, touchdown, half-time, AI turns).
     *
     * @param array<string, mixed> $params
     * @return array{state: GameState, events: list<GameEvent>, isSuccess: bool, isTurnover: bool}
     */
    public function executeAction(GameState $state, ActionType $action, array $params): array
    {
        // Log state before human decision (for learning from human games)
        if ($this->gameLogger !== null && $state->getPhase()->isPlayable()) {
            $this->gameLogger->logState($state, $state->getActiveTeam());
        }

        // Validate
        $errors = $this->rulesEngine->validate($state, $action, $params);
        if ($errors !== []) {
            throw new ValidationException($errors);
        }

        // Resolve
        $resolver = new ActionResolver($this->dice);
        $result = $resolver->resolve($state, $action, $params);
        $allEvents = $result->getEvents();
        $newState = $result->getNewState();

        // Handle turnover → END_TURN
        if ($result->isTurnover()) {
            $endTurnResult = $resolver->resolve($newState, ActionType::END_TURN, []);
            $newState = $endTurnResult->getNewState();
            $allEvents = array_merge($allEvents, $endTurnResult->getEvents());
        }

        // Touchdown check
        $gameFlow = $resolver->getGameFlowResolver();
        $scoringTeam = $gameFlow->checkTouchdown($newState);
        if ($scoringTeam !== null) {
            $tdResult = $gameFlow->resolveTouchdown($newState, $scoringTeam);
            $newState = $tdResult['state'];
            $allEvents = array_merge($allEvents, $tdResult['events']);

            $postResult = $gameFlow->resolvePostTouchdown($newState);
            $newState = $postResult['state'];
            $allEvents = array_merge($allEvents, $postResult['events']);
        }

        // Half-time
        if ($newState->getPhase() === GamePhase::HALF_TIME) {
            $htResult = $gameFlow->resolveHalfTime($newState);
            $newState = $htResult['state'];
            $allEvents = array_merge($allEvents, $htResult['events']);
        }

        // AI auto-play for setup
        $aiTeam = $newState->getAiTeam();
        if ($aiTeam !== null && $newState->getActiveTeam() === $aiTeam && $newState->getPhase()->isSetup()) {
            $newState = $this->aiCoach->setupFormation($newState, $aiTeam);
            $setupResult = $resolver->resolve($newState, ActionType::END_SETUP, []);
            $newState = $setupResult->getNewState();
            $allEvents = array_merge($allEvents, $setupResult->getEvents());
        }

        // AI auto-play for turn
        if ($aiTeam !== null && $newState->getActiveTeam() === $aiTeam && $newState->getPhase()->isPlayable()) {
            $aiTurnService = new AITurnService($this->aiCoach, $this->rulesEngine);
            if ($this->gameLogger !== null) {
                $aiTurnService->setGameLogger($this->gameLogger);
            }
            $aiResult = $aiTurnService->playTurn($newState, $this->dice);
            $newState = $aiResult['state'];
            $allEvents = array_merge($allEvents, $aiResult['events']);
        }

        // Log result at game over
        if ($this->gameLogger !== null && $newState->getPhase() === GamePhase::GAME_OVER) {
            $this->gameLogger->logResult($newState);
            $this->gameLogger->close();
        }

        return [
            'state' => $newState,
            'events' => $allEvents,
            'isSuccess' => $result->isSuccess(),
            'isTurnover' => $result->isTurnover(),
        ];
    }
}
