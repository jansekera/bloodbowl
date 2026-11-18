<?php

declare(strict_types=1);

namespace App\Engine;

use App\DTO\GameState;
use App\DTO\ReplayContext;

/**
 * P294 krok 3: společný předek otázek kouči-člověku uprostřed akce (Diving Catch, kam hodit bombu, rozhodčí, přehoz
 * hodu). Akce se přeruší, `ActionResolver` vrátí stav v okamžiku otázky (od P294 kroku 5a; jen k zobrazení) s čekající
 * otázkou (`pending`) a po odpovědi ji přehraje z výchozího stavu akce (`ReplayContext::$base`) se zapsanými hody
 * a odpověďmi (`ReplayDiceRoller`, `ReplayAnswers`). Jeden `catch` místo čtyř.
 */
abstract class CoachQuestion extends \RuntimeException
{
    /**
     * P294 krok 5a (uživatel 17.11.2026: „je dobře označit správně situaci, která chce reroll“): stav v okamžiku
     * otázky — jen pro zobrazení (kde hráč stojí). Přehrává se z výchozího stavu akce (`ReplayContext::$base`). Null =
     * ukáže se stav před akcí. Co padlo, ukáže log akce (`GameEvent::log`, P306 A1).
     */
    public ?GameState $snapshot = null;

    public function naStavu(GameState $snapshot): static
    {
        $this->snapshot = $snapshot;

        return $this;
    }

    /** `$state` (stav k zobrazení) s čekající otázkou, která nese vše pro přehrání (`ReplayContext`). */
    abstract public function pending(GameState $state, ReplayContext $replay): GameState;
}
