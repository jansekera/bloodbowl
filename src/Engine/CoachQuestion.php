<?php

declare(strict_types=1);

namespace App\Engine;

use App\DTO\GameState;
use App\DTO\ReplayAnswers;
use App\Enum\ActionType;

/**
 * P294 krok 3: společný předek otázek kouči-člověku uprostřed akce (Diving Catch, kam hodit bombu, rozhodčí, přehoz
 * hodu). Akce se přeruší, `ActionResolver` vrátí stav PŘED ní s čekající otázkou (`pending`) a po odpovědi ji přehraje
 * se zapsanými hody a odpověďmi (`ReplayDiceRoller`, `ReplayAnswers`). Jeden `catch` místo čtyř.
 */
abstract class CoachQuestion extends \RuntimeException
{
    /**
     * Stav PŘED akcí s čekající otázkou, která nese vše pro přehrání.
     *
     * @param array<string, mixed> $params
     * @param list<int> $dice
     */
    abstract public function pending(GameState $state, ActionType $action, array $params, array $dice, ReplayAnswers $answers): GameState;
}
