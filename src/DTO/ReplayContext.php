<?php

declare(strict_types=1);

namespace App\DTO;

use App\Enum\ActionType;

/**
 * P294 krok 4: co je potřeba k přehrání akce přerušené otázkou kouči — akce, její parametry, dosud padlé hody
 * (`ReplayDiceRoller`) a odpovědi z dřívějších přerušení (`ReplayAnswers`). Jedno místo místo kopie čtyř polí
 * v každé čekající otázce (Diving Catch, kam hodit bombu, rozhodčí, přehoz hodu).
 */
final class ReplayContext
{
    /**
     * @param array<string, mixed> $params
     * @param list<int> $dice
     */
    public function __construct(
        public readonly ActionType $action,
        public readonly array $params,
        public readonly array $dice = [],
        public readonly ReplayAnswers $answers = new ReplayAnswers(),
    ) {}

    /** Totéž přehrání s další odpovědí kouče. */
    public function withAnswers(ReplayAnswers $answers): self
    {
        return new self($this->action, $this->params, $this->dice, $answers);
    }

    /** @return array<string, mixed> */
    public function toArray(): array
    {
        return [
            'action' => $this->action->value,
            'params' => $this->params,
            'dice' => $this->dice,
            'answers' => $this->answers->toArray(),
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
        );
    }
}
