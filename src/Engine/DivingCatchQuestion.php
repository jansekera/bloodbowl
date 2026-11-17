<?php

declare(strict_types=1);

namespace App\Engine;

use App\DTO\PendingDivingCatchDTO;
use App\DTO\GameState;
use App\DTO\ReplayAnswers;
use App\Enum\ActionType;
use App\ValueObject\Position;

/**
 * Akce narazila na otázku Diving Catch pro kouče-člověka a odpověď ještě nemá (viz `ReplayDiceRoller`).
 * Chytá ji jen `ActionResolver::resolve`, který vrátí stav PŘED akcí s `PendingDivingCatchDTO`.
 */
final class DivingCatchQuestion extends CoachQuestion
{
    public function __construct(
        public readonly int $playerId,
        public readonly Position $landing,
        public readonly int $otherCandidates = 0,
        public readonly ?int $attemptingId = null,
    ) {
        parent::__construct('Diving Catch question pending');
    }

    public function pending(GameState $state, ActionType $action, array $params, array $dice, ReplayAnswers $answers): GameState
    {
        return $state->withPendingDivingCatch(new PendingDivingCatchDTO(
            $this->playerId,
            $this->landing,
            $action,
            $params,
            $dice,
            $answers,
            $this->otherCandidates,
            $this->attemptingId,
        ));
    }
}
