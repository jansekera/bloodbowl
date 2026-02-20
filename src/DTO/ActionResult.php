<?php
declare(strict_types=1);

namespace App\DTO;

final class ActionResult
{
    /**
     * @param list<GameEvent> $events
     */
    public function __construct(
        private readonly GameState $newState,
        private readonly bool $success,
        private readonly bool $turnover,
        private readonly array $events,
    ) {
    }

    /**
     * @param list<GameEvent> $events
     */
    public static function success(GameState $state, array $events = []): self
    {
        return new self($state, true, false, $events);
    }

    /**
     * @param list<GameEvent> $events
     */
    public static function turnover(GameState $state, array $events = []): self
    {
        return new self($state, false, true, $events);
    }

    /**
     * @param list<GameEvent> $events
     */
    public static function failure(GameState $state, array $events = []): self
    {
        return new self($state, false, false, $events);
    }

    public function getNewState(): GameState { return $this->newState; }
    public function isSuccess(): bool { return $this->success; }
    public function isTurnover(): bool { return $this->turnover; }
    /** @return list<GameEvent> */
    public function getEvents(): array { return $this->events; }

    /**
     * @return array<string, mixed>
     */
    public function toArray(): array
    {
        return [
            'success' => $this->success,
            'turnover' => $this->turnover,
            'events' => array_map(fn(GameEvent $e) => $e->toArray(), $this->events),
        ];
    }
}
