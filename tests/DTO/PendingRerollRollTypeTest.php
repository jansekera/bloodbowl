<?php

declare(strict_types=1);

namespace App\Tests\DTO;

use App\DTO\PendingRerollDTO;
use App\DTO\ReplayContext;
use App\Enum\ActionType;
use App\Enum\RollType;
use PHPUnit\Framework\TestCase;

/**
 * P306 A7 (18.11.2026): popisek, kategorii a následek odmítnutí přehozu posílá server (jedno místo, `RollType`);
 * web je jen vykreslí. Následky podle pravidel: pád a turnover u pohybu (ř. 497-499, 1702-1703), zvednutí turnover
 * (ř. 371-375), chytání ne (ř. 376-378), velký hráč ztratí akci (ř. 7980-7986, 8398-8401, 8668-8669).
 */
final class PendingRerollRollTypeTest extends TestCase
{
    private static function otazka(RollType $druh): PendingRerollDTO
    {
        return new PendingRerollDTO($druh, 1, 4, 2, false, true, 6, 6, new ReplayContext(ActionType::MOVE, []));
    }

    public function testKlientDostanePopisekKategoriiANasledek(): void
    {
        $a = self::otazka(RollType::DODGE)->toArray();

        $this->assertSame(['Dodge', 'move'], [$a['label'], $a['category']]);
        $this->assertStringContainsString('falls', (string) $a['consequence']);
    }

    public function testNasledkyPodlePravidel(): void
    {
        $this->assertStringContainsString('turn ends', (string) RollType::PICKUP->consequence());
        $this->assertStringNotContainsString('turn ends', (string) RollType::CATCH->consequence());
        $this->assertStringContainsString('loses the declared action', (string) RollType::WILD_ANIMAL->consequence());
        $this->assertNull(RollType::PASS->consequence());
        $this->assertSame('big_guy', RollType::REALLY_STUPID->category());
        $this->assertSame('ball', RollType::CATCH->category());
        $this->assertSame('move', RollType::LEAP_GFI->category());
    }

    public function testNeznamyDruhHoduVUlozenemStavuSpadneHned(): void
    {
        $data = self::otazka(RollType::DODGE)->toArray();
        $data['rollType'] = 'neexistuje';

        $this->expectException(\ValueError::class);
        PendingRerollDTO::fromArray($data);
    }
}
