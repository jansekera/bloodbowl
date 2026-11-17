<?php

declare(strict_types=1);

namespace App\Engine;

use App\DTO\GameEvent;
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

    /**
     * P294 krok 5d: otázka položená hlouběji (zvednutí, chytání v `BallResolver`) zná jen události svého hodu; volající
     * akce k nim přidá své dřívější (krok, předávka, přihrávka), aby kouč viděl průběh ve správném pořadí a po odpovědi
     * se v logu nic neopakovalo. Spustí `$fn`; otázku z ní pošle dál s `$pred` napřed.
     *
     * @template T
     * @param list<GameEvent> $pred
     * @param \Closure(): T $fn
     * @return T
     */
    public static function sPredchozimi(array $pred, \Closure $fn): mixed
    {
        try {
            return $fn();
        } catch (CoachQuestion $q) {
            $q->events = [...$pred, ...$q->events];

            throw $q;
        }
    }

    /** `$state` (stav k zobrazení) s čekající otázkou, která nese vše pro přehrání (`ReplayContext`). */
    abstract public function pending(GameState $state, ReplayContext $replay): GameState;
}
