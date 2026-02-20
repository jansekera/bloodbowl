<?php

declare(strict_types=1);

namespace App\Service;

use App\AI\AICoachInterface;
use App\AI\GameLogger;
use App\DTO\GameEvent;
use App\DTO\GameState;
use App\Engine\ActionResolver;
use App\Engine\DiceRollerInterface;
use App\Engine\RulesEngine;
use App\Enum\ActionType;

final class AITurnService
{
    private ?GameLogger $gameLogger = null;

    public function __construct(
        private readonly AICoachInterface $aiCoach,
        private readonly RulesEngine $rulesEngine,
    ) {
    }

    public function setGameLogger(GameLogger $logger): void
    {
        $this->gameLogger = $logger;
    }

    /**
     * Play an entire AI turn automatically.
     *
     * @return array{state: GameState, events: list<GameEvent>}
     */
    public function playTurn(GameState $state, DiceRollerInterface $dice): array
    {
        $resolver = new ActionResolver($dice);
        $gameFlow = $resolver->getGameFlowResolver();
        $allEvents = [];
        $aiTeam = $state->getAiTeam();
        $maxActions = 100;

        for ($i = 0; $i < $maxActions; $i++) {
            if ($state->getActiveTeam() !== $aiTeam || !$state->getPhase()->isPlayable()) {
                break;
            }

            // Log state before AI decision
            if ($this->gameLogger !== null) {
                $this->gameLogger->logState($state, $aiTeam);
            }

            $decision = $this->aiCoach->decideAction($state, $this->rulesEngine);
            $action = $decision['action'];
            $params = $decision['params'];

            $result = $resolver->resolve($state, $action, $params);
            $state = $result->getNewState();
            $allEvents = array_merge($allEvents, $result->getEvents());

            if ($result->isTurnover()) {
                $endTurnResult = $resolver->resolve($state, ActionType::END_TURN, []);
                $state = $endTurnResult->getNewState();
                $allEvents = array_merge($allEvents, $endTurnResult->getEvents());
                break;
            }

            $scoringTeam = $gameFlow->checkTouchdown($state);
            if ($scoringTeam !== null) {
                $tdResult = $gameFlow->resolveTouchdown($state, $scoringTeam);
                $state = $tdResult['state'];
                $allEvents = array_merge($allEvents, $tdResult['events']);

                $postResult = $gameFlow->resolvePostTouchdown($state);
                $state = $postResult['state'];
                $allEvents = array_merge($allEvents, $postResult['events']);
                break;
            }

            if ($action === ActionType::END_TURN) {
                break;
            }
        }

        return ['state' => $state, 'events' => $allEvents];
    }
}
