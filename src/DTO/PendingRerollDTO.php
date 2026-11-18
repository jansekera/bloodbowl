<?php

declare(strict_types=1);

namespace App\DTO;

use App\Enum\RollType;

/**
 * Otázka kouči-člověku na přehoz nepovedeného hodu: pohyb (`dodge`, `gfi`, `leap`, `leap_gfi`), míč (`pickup`,
 * `catch`), velcí hráči (`bone_head`, `really_stupid`, `wild_animal`), hody (`pass`, `bomb`, `ttm`, `ttm_landing`,
 * `always_hungry`, `jump_up`).
 *
 * P294 krok 5f (17.11.2026): jediný způsob je přehrání akce (`replay`: akce, zapsané hody, odpovědi + nová; jako
 * Diving Catch) — dialog uprostřed akce s ručním pokračováním (`resumeAction`, `gfiRemaining`, `RerollHandler`) smazán.
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
        private readonly ReplayContext $replay,
        // ⭐ 18.09.2026: hod Pro padl 1-3 => puvodni vysledek plati a tymovy
        //   prehoz smi prehodit uz jen HOD PRO (`rules_bb2016.txt` r. 8385-8387).
        private readonly bool $proFailed = false,
    ) {
        RollType::from($rollType);   // P306 A7: neznámý druh hodu spadne tady, ne až ve webu
    }

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

    /** P294 krok 4: přehrání akce (akce, hody, odpovědi) */
    public function getReplay(): ReplayContext
    {
        return $this->replay;
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
        $druh = RollType::from($this->rollType);

        return [
            'rollType' => $this->rollType,
            // P306 A7: popisek, kategorie a následek odmítnutí z jednoho místa (`RollType`), web je jen vykreslí
            'label' => $druh->label(),
            'category' => $druh->category(),
            'consequence' => $druh->consequence(),
            'playerId' => $this->playerId,
            'target' => $this->target,
            'roll' => $this->roll,
            'proAvailable' => $this->proAvailable,
            'teamRerollAvailable' => $this->teamRerollAvailable,
            'targetX' => $this->targetX,
            'targetY' => $this->targetY,
            'proFailed' => $this->proFailed,
            'replay' => $this->replay->toArray(),
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
            replay: ReplayContext::fromArray((array) $data['replay']),
            proFailed: (bool) ($data['proFailed'] ?? false),
        );
    }
}
