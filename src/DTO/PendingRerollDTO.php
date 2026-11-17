<?php

declare(strict_types=1);

namespace App\DTO;

use App\Enum\ActionType;

/**
 * Captures a failed roll awaiting player reroll decision (dodge, GFI, pickup).
 *
 * P289 (10.11.2026): i hody velkých hráčů (`bone_head`, `really_stupid`, `wild_animal`) a Leap (`leap`,
 * `leap_gfi`). U velkého hráče nese ohlášenou akci (`resumeAction`, `resumeParams`), která po povedeném
 * přehozu doběhne. Leap od P294 kroku 5c přehráním (`replay`).
 *
 * P292 (10.11.2026): přehoz hodu přes ThrowReroll (přihrávka `pass`, bomba `bomb`, hod spoluhráčem `ttm`, přistání
 * `ttm_landing`, Always Hungry `always_hungry`). Stav je pak stav PŘED akcí; po odpovědi se akce přehraje
 * (`replay`: akce, zapsané hody a odpovědi + nová; jako Diving Catch).
 */
final class PendingRerollDTO
{
    public function __construct(
        private readonly string $rollType,
        private readonly int $playerId,
        private readonly int $target,
        private readonly int $roll,
        private readonly bool $proAvailable,
        private readonly bool $teamRerollAvailable,
        private readonly int $targetX,
        private readonly int $targetY,
        // ⭐ 18.09.2026: hod Pro padl 1-3 => puvodni vysledek plati a tymovy
        //   prehoz smi prehodit uz jen HOD PRO (`rules_bb2016.txt` r. 8385-8387).
        private readonly bool $proFailed = false,
        private readonly ?ActionType $resumeAction = null,
        /** @var array<string, mixed> */
        private readonly array $resumeParams = [],
        private readonly ?ReplayContext $replay = null,
    ) {}

    public function getRollType(): string
    {
        return $this->rollType;
    }
    public function getPlayerId(): int
    {
        return $this->playerId;
    }
    public function getTarget(): int
    {
        return $this->target;
    }
    public function getRoll(): int
    {
        return $this->roll;
    }
    public function isProAvailable(): bool
    {
        return $this->proAvailable;
    }
    public function isTeamRerollAvailable(): bool
    {
        return $this->teamRerollAvailable;
    }
    public function getTargetX(): int
    {
        return $this->targetX;
    }
    public function getTargetY(): int
    {
        return $this->targetY;
    }
    public function isProFailed(): bool
    {
        return $this->proFailed;
    }

    public function getResumeAction(): ?ActionType
    {
        return $this->resumeAction;
    }

    /** @return array<string, mixed> */
    public function getResumeParams(): array
    {
        return $this->resumeParams;
    }

    /** P294 krok 4: přehrání akce (přehoz přes ThrowReroll, P292); null = dialog uprostřed akce (resume*) */
    public function getReplay(): ?ReplayContext
    {
        return $this->replay;
    }

    public function withProFailed(): self
    {
        return new self(
            $this->rollType,
            $this->playerId,
            $this->target,
            $this->roll,
            proAvailable: false,
            teamRerollAvailable: $this->teamRerollAvailable,
            targetX: $this->targetX,
            targetY: $this->targetY,
            proFailed: true,
            resumeAction: $this->resumeAction,
            resumeParams: $this->resumeParams,
            replay: $this->replay,
        );
    }

    /**
     * Volba kouče se musí krýt s nabídkou (oprava 18.09.2026: dřív šlo 'pro' i bez skillu a 'team_reroll' i bez
     * nabídnutého přehozu; `rules_bb2016.txt` ř. 8381, 8385-8386, 926).
     */
    public function assertChoice(string $choice): void
    {
        if ($choice === 'pro' && !$this->proAvailable) {
            throw new \InvalidArgumentException('Pro not available');
        }
        if ($choice === 'team_reroll' && !$this->teamRerollAvailable) {
            throw new \InvalidArgumentException('Team reroll not available');
        }
        if (!in_array($choice, ['pro', 'team_reroll', 'decline'], true)) {
            throw new \InvalidArgumentException("Invalid reroll choice: {$choice}");
        }
    }

    /**
     * @return array<string, mixed>
     */
    public function toArray(): array
    {
        return [
            'rollType' => $this->rollType,
            'playerId' => $this->playerId,
            'target' => $this->target,
            'roll' => $this->roll,
            'proAvailable' => $this->proAvailable,
            'teamRerollAvailable' => $this->teamRerollAvailable,
            'targetX' => $this->targetX,
            'targetY' => $this->targetY,
            'proFailed' => $this->proFailed,
            'resumeAction' => $this->resumeAction?->value,
            'resumeParams' => $this->resumeParams,
            'replay' => $this->replay?->toArray(),
        ];
    }

    /**
     * @param array<string, mixed> $data
     */
    public static function fromArray(array $data): self
    {
        return new self(
            rollType: (string) $data['rollType'],
            playerId: (int) $data['playerId'],
            target: (int) $data['target'],
            roll: (int) $data['roll'],
            proAvailable: (bool) ($data['proAvailable'] ?? false),
            teamRerollAvailable: (bool) ($data['teamRerollAvailable'] ?? false),
            targetX: (int) ($data['targetX'] ?? 0),
            targetY: (int) ($data['targetY'] ?? 0),
            proFailed: (bool) ($data['proFailed'] ?? false),
            resumeAction: isset($data['resumeAction']) ? ActionType::from((string) $data['resumeAction']) : null,
            resumeParams: (array) ($data['resumeParams'] ?? []),
            replay: isset($data['replay']) ? ReplayContext::fromArray((array) $data['replay']) : null,
        );
    }
}
