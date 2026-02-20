<?php
declare(strict_types=1);

namespace App\AI;

use App\DTO\GameState;
use App\Engine\ActionResolver;
use App\Enum\ActionType;

final class ActionSimulator
{
    public function __construct(private readonly ActionResolver $resolver)
    {
    }

    /**
     * Simulate an action and return the resulting GameState (or null on failure).
     *
     * @param array<string, mixed> $params
     */
    public function simulate(GameState $state, ActionType $type, array $params): ?GameState
    {
        try {
            $result = $this->resolver->resolve($state, $type, $params);
            return $result->getNewState();
        } catch (\Throwable) {
            return null;
        }
    }
}
