<?php

declare(strict_types=1);

namespace App\DTO;

use App\Enum\ActionType;

/**
 * P294 krok 4: co je potřeba k přehrání akce přerušené otázkou kouči — akce, její parametry, dosud padlé hody
 * (`ReplayDiceRoller`) a odpovědi z dřívějších přerušení (`ReplayAnswers`). Jedno místo místo kopie čtyř polí
 * v každé čekající otázce (Diving Catch, kam hodit bombu, rozhodčí, přehoz hodu).
 *
 * P294 krok 5a: zobrazený stav může být okamžik otázky (kde hráč stojí, co padlo) ⇒ přehrává se z `base` (stav před
 * akcí); `shown` = události, které kouč už viděl — po odpovědi se v logu znovu neukážou.
 */
final class ReplayContext
{
    /**
     * @param array<string, mixed> $params
     * @param list<int> $dice
     * @param list<array<string, mixed>> $shown události už ukázané kouči (GameEvent::toArray)
     */
    public function __construct(
        public readonly ActionType $action,
        public readonly array $params,
        public readonly array $dice = [],
        public readonly ReplayAnswers $answers = new ReplayAnswers(),
        public readonly ?GameState $base = null,
        public readonly array $shown = [],
    ) {}

    /** Totéž přehrání s další odpovědí kouče. */
    public function withAnswers(ReplayAnswers $answers): self
    {
        return new self($this->action, $this->params, $this->dice, $answers, $this->base, $this->shown);
    }

    /** @return array<string, mixed> */
    public function toArray(): array
    {
        return [
            'action' => $this->action->value,
            'params' => $this->params,
            'dice' => $this->dice,
            'answers' => $this->answers->toArray(),
            'base' => $this->base?->toArray(),
            'shown' => $this->shown,
        ];
    }

    /** @param array<string, mixed> $data */
    public static function fromArray(array $data): self
    {
        return new self(
            ActionType::from((string) $data['action']),
            (array) ($data['params'] ?? []),
            array_values(array_map('intval', (array) ($data['dice'] ?? []))),
            ReplayAnswers::fromArray((array) ($data['answers'] ?? [])),
            isset($data['base']) ? GameState::fromArray((array) $data['base']) : null,
            array_values(array_map(static fn(mixed $e): array => (array) $e, (array) ($data['shown'] ?? []))),
        );
    }
}
