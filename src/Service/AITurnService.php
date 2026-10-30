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

    /** @param list<\App\Enum\TeamSide> $interactiveApothecary P206 (b): strany kouče-člověka (P226: i Stand Firm) */
    public function __construct(
        private readonly AICoachInterface $aiCoach,
        private readonly RulesEngine $rulesEngine,
        private readonly array $interactiveApothecary = [],
    ) {}

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
        $resolver->setInteractiveApothecary($this->interactiveApothecary);
        $resolver->setInteractiveStandFirm($this->interactiveApothecary);
        $resolver->setInteractiveDivingCatch($this->interactiveApothecary);
        $gameFlow = $resolver->getGameFlowResolver();
        $allEvents = [];
        $aiTeam = $state->getAiTeam();
        $maxActions = 100;

        for ($i = 0; $i < $maxActions; $i++) {
            if ($state->getActiveTeam() !== $aiTeam || !$state->getPhase()->isPlayable()
                || $state->getPendingApothecary() !== null || $state->getPendingStandFirm() !== null
                || $state->getPendingDivingCatch() !== null) {
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

            // P206 (b): zranil se hráč kouče-člověka ⇒ tah AI stojí, dokud kouč nerozhodne o lékárníkovi
            //   (turnover akce nese PendingApothecaryDTO a orchestrátor ho dořeší po odpovědi).
            // P226: kouč-člověk odpovídá na Stand Firm svého hráče ⇒ tah AI stojí (blok čeká v pendingBlock)
            // 30.10.2026: kouč-člověk odpovídá na Diving Catch svého hráče ⇒ tah AI stojí (akce se po odpovědi přehraje)
            if ($state->getPendingApothecary() !== null || $state->getPendingStandFirm() !== null
                || $state->getPendingDivingCatch() !== null) {
                break;
            }

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
