<?php
declare(strict_types=1);

namespace App\AI;

use App\DTO\GameState;
use App\Engine\RulesEngine;
use App\Enum\ActionType;
use App\Enum\TeamSide;

interface AICoachInterface
{
    /**
     * Decide the next action to take.
     *
     * @return array{action: ActionType, params: array<string, mixed>}
     */
    public function decideAction(GameState $state, RulesEngine $rules): array;

    /**
     * Place 11 players in valid setup positions.
     */
    public function setupFormation(GameState $state, TeamSide $side): GameState;
}
