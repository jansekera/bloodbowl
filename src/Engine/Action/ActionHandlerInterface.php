<?php

declare(strict_types=1);

namespace App\Engine\Action;

use App\DTO\ActionResult;
use App\DTO\GameState;

interface ActionHandlerInterface
{
    /**
     * @param array<string, mixed> $params
     */
    public function resolve(GameState $state, array $params): ActionResult;
}
