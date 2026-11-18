<?php

declare(strict_types=1);

namespace App\Tests\DTO;

use App\DTO\GameState;
use App\DTO\PendingRerollDTO;
use App\DTO\ReplayContext;
use App\Enum\ActionType;
use App\Tests\Engine\GameStateBuilder;
use PHPUnit\Framework\TestCase;

/**
 * P306 E1 (18.11.2026): klient nedostane přehrání čekající otázky (stav před akcí, padlé hody, odpovědi) — potřebuje ho
 * jen server; uložený stav ho drží dál, jinak by odpověď nešla přehrát.
 */
final class StavProKlientaTest extends TestCase
{
    private static function sOtazkou(): GameState
    {
        $s = (new GameStateBuilder())->build();

        return $s->withPendingReroll(new PendingRerollDTO('dodge', 1, 4, 2, false, true, 6, 6, new ReplayContext(ActionType::MOVE, ['playerId' => 1], [2], base: $s)));
    }

    public function testKlientPrehraniNedostane(): void
    {
        $klient = self::sOtazkou()->toClientArray();

        $this->assertIsArray($klient['pendingReroll']);
        $this->assertArrayNotHasKey('replay', $klient['pendingReroll']);
        $this->assertSame('dodge', $klient['pendingReroll']['rollType'], 'otázku samotnou klient dostane');
    }

    public function testUlozenyStavPrehraniDrziAOtazkaJdePrehrat(): void
    {
        $ulozeny = GameState::fromArray(json_decode((string) json_encode(self::sOtazkou()->toArray()), true));

        $this->assertSame([2], $ulozeny->getPendingReroll()?->getReplay()->dice);
    }
}
