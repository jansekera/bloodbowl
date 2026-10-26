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
 *  - kind 'bnc': blok Ball & Chain (ř. 7829-7831) v kroku `step` pohybu; `face` null = B&C odtlačuje
 *    ležícího hráče bez bloku (ř. 7843-7845) a ptá se hráče v řetězu (review P226, LOW 2);
 *  - kind 'mb': Multiple Block (`targets` oba cíle, `index` právě vyhodnocovaný blok).
 * `face` = zvolená strana kostky (Pushed vs Knock-down, ř. 8511-8513), `pushTo` = pole, kam by hráč
 * bez Stand Firm šel (přehrání nanečisto; null = do davu). `grabPossible`: útočník-člověk se pak smí
 * Grabem rozhodnout jinak (pole je bez Grabu). `pushToDependsOn`: lidé v řetězu, kteří ještě
 * neodpověděli -- přehrání je bere jako držící, pole závisí na jejich odpovědi.
 */
final class PendingStandFirmDTO
{
    /**
     * @param array<int, bool> $answers id hráče => použije Stand Firm
     * @param list<int> $targets
     * @param list<int> $pushToDependsOn
     */
    public function __construct(
        private readonly int $playerId,
        private readonly int $attackerId,
        private readonly int $defenderId,
        private readonly ?BlockDiceFace $face,
        private readonly ?Position $pushTo,
        private readonly string $kind = 'block',
        private readonly int $faceIndex = 0,
        private readonly array $answers = [],
        private readonly ?bool $followUp = null,
        private readonly Position|false|null $grab = null,
        private readonly int $step = 0,
        private readonly array $targets = [],
        private readonly int $index = 0,
        private readonly bool $grabPossible = false,
        private readonly array $pushToDependsOn = [],
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

    public function getFace(): ?BlockDiceFace
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

    public function isGrabPossible(): bool
    {
        return $this->grabPossible;
    }

    /** @return list<int> */
    public function getPushToDependsOn(): array
    {
        return $this->pushToDependsOn;
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
            'face' => $this->face?->value,
            'pushTo' => $this->pushTo === null ? null : ['x' => $this->pushTo->getX(), 'y' => $this->pushTo->getY()],
            'kind' => $this->kind,
            'faceIndex' => $this->faceIndex,
            'answers' => $answers,
            'followUp' => $this->followUp,
            'grab' => $this->grab instanceof Position ? ['x' => $this->grab->getX(), 'y' => $this->grab->getY()] : $this->grab,
            'step' => $this->step,
            'targets' => $this->targets,
            'index' => $this->index,
            'grabPossible' => $this->grabPossible,
            'pushToDependsOn' => $this->pushToDependsOn,
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
            isset($data['face']) ? BlockDiceFace::from((string) $data['face']) : null,
            $pole($data['pushTo'] ?? null),
            (string) ($data['kind'] ?? 'block'),
            (int) ($data['faceIndex'] ?? 0),
            $answers,
            isset($data['followUp']) ? (bool) $data['followUp'] : null,
            $grab,
            (int) ($data['step'] ?? 0),
            array_values(array_map('intval', (array) ($data['targets'] ?? []))),
            (int) ($data['index'] ?? 0),
            (bool) ($data['grabPossible'] ?? false),
            array_values(array_map('intval', (array) ($data['pushToDependsOn'] ?? []))),
        );
    }
}
