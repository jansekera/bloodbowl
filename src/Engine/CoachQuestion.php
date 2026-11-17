<?php

declare(strict_types=1);

namespace App\Engine;

use App\DTO\GameEvent;
use App\DTO\GameState;
use App\DTO\ReplayContext;

/**
 * P294 krok 3: společný předek otázek kouči-člověku uprostřed akce (Diving Catch, kam hodit bombu, rozhodčí, přehoz
 * hodu). Akce se přeruší, `ActionResolver` vrátí stav PŘED ní s čekající otázkou (`pending`) a po odpovědi ji přehraje
 * se zapsanými hody a odpověďmi (`ReplayDiceRoller`, `ReplayAnswers`). Jeden `catch` místo čtyř.
 */
abstract class CoachQuestion extends \RuntimeException
{
    /**
     * P294 krok 5a (uživatel 17.11.2026: „je dobře označit správně situaci, která chce reroll“): stav a události
     * v okamžiku otázky — jen pro zobrazení (kde hráč stojí, co padlo). Přehrává se z výchozího stavu akce
     * (`ReplayContext::$base`). Null = otázka průběh nenese, ukáže se stav před akcí.
     */
    public ?GameState $snapshot = null;
    /** @var list<GameEvent> */
    public array $events = [];

    /** @param list<GameEvent> $events */
    public function naStavu(GameState $snapshot, array $events = []): static
    {
        $this->snapshot = $snapshot;
        $this->events = $events;

        return $this;
    }

    /** `$state` (stav k zobrazení) s čekající otázkou, která nese vše pro přehrání (`ReplayContext`). */
    abstract public function pending(GameState $state, ReplayContext $replay): GameState;
}
