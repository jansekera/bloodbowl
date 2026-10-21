<?php

declare(strict_types=1);

namespace App\DTO;

use App\Enum\BlockDiceFace;
use App\ValueObject\Position;

/**
 * P226 (21.10.2026), parita s C++ P130 (`USE_STAND_FIRM`). Čekající otázka kouče-člověka, jestli
 * jeho hráč `playerId` použije Stand Firm (`rules_bb2016.txt` ř. 8510-8516: "may choose to not be
 * pushed back … If a player is pushed back into a player with using Stand Firm then neither player
 * moves"). Ptá se přímo blokovaného obránce i hráče v řetězovém odtlačení -- vždy jen toho, na
 * kterém odtlačení právě závisí.
 *
 * Otázka přeruší akci dřív, než se při odtlačení cokoli pohne nebo hodí; po odpovědi se odtlačení
 * vyhodnotí znovu se všemi dosavadními odpověďmi (`answers`):
 *  - kind 'block': blok z akce Block/Blitz; `PendingBlockDTO` dál čeká, `faceIndex` je zvolená
 *    kostka, `followUp` volba útočníka poslaná s kostkou, `grab` odpověď útočníka na Grab (P224:
 *    pole, false = ne, null = zatím se neptal);
 *  - kind 'bnc': blok Ball & Chain (ř. 7829-7831) v kroku `step` pohybu;
 *  - kind 'mb': Multiple Block (`targets` oba cíle, `index` právě vyhodnocovaný blok).
 * `face` = zvolená strana kostky (Pushed vs Knock-down, ř. 8511-8513), `pushTo` = pole, kam by hráč
 * bez Stand Firm šel (přehrání nanečisto, bez Grabu; null = do davu).
 */
final class PendingStandFirmDTO
{
    /**
     * @param array<int, bool> $answers id hráče => použije Stand Firm
     * @param list<int> $targets
     */
    public function __construct(
        private readonly int $playerId,
        private readonly int $attackerId,
        private readonly int $defenderId,
        private readonly BlockDiceFace $face,
        private readonly ?Position $pushTo,
        private readonly string $kind = 'block',
        private readonly int $faceIndex = 0,
        private readonly array $answers = [],
        private readonly ?bool $followUp = null,
        private readonly Position|false|null $grab = null,
        private readonly int $step = 0,
        private readonly array $targets = [],
        private readonly int $index = 0,
    ) {}

    public function getPlayerId(): int
    {
        return $this->playerId;
    }

    public function getAttackerId(): int
    {
        return $this->attackerId;
    }

    public function getDefenderId(): int
    {
        return $this->defenderId;
    }

    public function getFace(): BlockDiceFace
    {
        return $this->face;
    }

    public function getPushTo(): ?Position
    {
        return $this->pushTo;
    }

    public function getKind(): string
    {
        return $this->kind;
    }

    public function getFaceIndex(): int
    {
        return $this->faceIndex;
    }

    /** @return array<int, bool> */
    public function getAnswers(): array
    {
        return $this->answers;
    }

    public function getFollowUp(): ?bool
    {
        return $this->followUp;
    }

    public function getGrab(): Position|false|null
    {
        return $this->grab;
    }

    public function getStep(): int
    {
        return $this->step;
    }

    /** @return list<int> */
    public function getTargets(): array
    {
        return $this->targets;
    }

    public function getIndex(): int
    {
        return $this->index;
    }

    /** @return array<string, mixed> */
    public function toArray(): array
    {
        $answers = [];
        foreach ($this->answers as $id => $use) {
            $answers[] = ['playerId' => $id, 'use' => $use];
        }

        return [
            'playerId' => $this->playerId,
            'attackerId' => $this->attackerId,
            'defenderId' => $this->defenderId,
            'face' => $this->face->value,
            'pushTo' => $this->pushTo === null ? null : ['x' => $this->pushTo->getX(), 'y' => $this->pushTo->getY()],
            'kind' => $this->kind,
            'faceIndex' => $this->faceIndex,
            'answers' => $answers,
            'followUp' => $this->followUp,
            'grab' => $this->grab instanceof Position ? ['x' => $this->grab->getX(), 'y' => $this->grab->getY()] : $this->grab,
            'step' => $this->step,
            'targets' => $this->targets,
            'index' => $this->index,
        ];
    }

    /** @param array<string, mixed> $data */
    public static function fromArray(array $data): self
    {
        $answers = [];
        foreach ((array) ($data['answers'] ?? []) as $a) {
            $a = (array) $a;
            $answers[(int) $a['playerId']] = (bool) $a['use'];
        }
        $pole = static function (mixed $p): ?Position {
            if (!is_array($p) && !is_object($p)) {
                return null;
            }
            $p = (array) $p;

            return new Position((int) $p['x'], (int) $p['y']);
        };
        $grab = $data['grab'] ?? null;
        $grab = $grab === null ? null : ($pole($grab) ?? false);

        return new self(
            (int) $data['playerId'],
            (int) $data['attackerId'],
            (int) $data['defenderId'],
            BlockDiceFace::from((string) $data['face']),
            $pole($data['pushTo'] ?? null),
            (string) ($data['kind'] ?? 'block'),
            (int) ($data['faceIndex'] ?? 0),
            $answers,
            isset($data['followUp']) ? (bool) $data['followUp'] : null,
            $grab,
            (int) ($data['step'] ?? 0),
            array_values(array_map('intval', (array) ($data['targets'] ?? []))),
            (int) ($data['index'] ?? 0),
        );
    }
}
