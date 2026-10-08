<?php

declare(strict_types=1);

namespace App\Engine\Action;

use App\DTO\ActionResult;
use App\DTO\GameEvent;
use App\DTO\GameState;
use App\Enum\GamePhase;

final class EndTurnHandler implements ActionHandlerInterface
{
    /**
     * @param array<string, mixed> $params
     */
    public function resolve(GameState $state, array $params): ActionResult
    {
        $teamState = $state->getTeamState($state->getActiveTeam());
        $events = [GameEvent::endTurn($teamState->getName())];

        // OPRAVENO 08.10.2026 (audit parity, nález 1): omráčení se otáčeli na ZAČÁTKU kola
        //   svého týmu (`GameState::resetPlayersForNewTurn`). Pravidla ř. 703-707: lícem
        //   nahoru "at the end of their team's next turn, even if a turnover takes place"
        //   -- proto bezpodmínečně tady, na konci kola týmu, který ho právě dohrál.
        $state = $state->turnStunnedFaceUp($state->getActiveTeam());

        // Switch active team
        $newActiveTeam = $state->getActiveTeam()->opponent();
        $newTeamState = $state->getTeamState($newActiveTeam);

        // Check if we've exceeded 8 turns per half
        if ($teamState->getTurnNumber() >= 8) {
            if ($newTeamState->getTurnNumber() >= 8) {
                // Both teams used all turns
                if ($state->getHalf() >= 2) {
                    // Game over
                    return ActionResult::success(
                        $state->withPhase(GamePhase::GAME_OVER),
                        $events,
                    );
                }

                // Half time
                return ActionResult::success(
                    $state
                        ->withPhase(GamePhase::HALF_TIME)
                        ->withHalf(2),
                    $events,
                );
            }
        }

        // Reset players for the new turn
        $newState = $state
            ->withActiveTeam($newActiveTeam)
            ->resetPlayersForNewTurn($newActiveTeam)
            ->withTurnoverPending(false);

        // Increment turn counter for the team that just started
        $updatedTeamState = $state->getTeamState($newActiveTeam)->resetForNewTurn();
        $newState = $newState->withTeamState($newActiveTeam, $updatedTeamState);

        return ActionResult::success($newState, $events);
    }
}
