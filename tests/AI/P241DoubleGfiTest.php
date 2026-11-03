<?php

declare(strict_types=1);

namespace App\Tests\AI;

use App\AI\LearningAICoach;
use App\Engine\RulesEngine;
use App\Enum\{PlayerState, TeamSide};
use App\Tests\Engine\GameStateBuilder;
use PHPUnit\Framework\TestCase;

/**
 * ⭐ P241 (uživatel 2.–3.11.2026): „2 GFI jen pokud na něj nikdo nedosáhne“; „2x GFI má být výjimka,
 * ne obecný pohyb“; levný pád = soupeř nedosáhne na míč. Nosič MA 5 na (22,7) bez pohybu: (23,7) je
 * 1× GFI, (24,7) 2× GFI (pád s míčem 11/36, pravidla ř. 1695-1697, 1701-1706). Soupeř MA 6 doběhne
 * MA + 2 = 8 polí.
 */
final class P241DoubleGfiTest extends TestCase
{
    /** @return array{x: int, y: int}|null */
    private function tahNosice(int $ox, int $oy, PlayerState $stav = PlayerState::STANDING): ?array
    {
        $state = (new GameStateBuilder())->withActiveTeam(TeamSide::HOME)
            ->addPlayer(TeamSide::HOME, 22, 7, movement: 5, id: 1)
            ->addPlayer(TeamSide::AWAY, $ox, $oy, movement: 6, id: 12)
            ->withBallCarried(1)->build();
        $state = $state->withPlayer($state->requirePlayer(1)->withMovementRemaining(0));
        $state = $state->withPlayer($state->requirePlayer(12)->withState($stav));
        $build = new \ReflectionMethod(LearningAICoach::class, 'buildMoveAction');
        $r = $build->invoke(new LearningAICoach(null, 0.0), $state, new RulesEngine(), 1, TeamSide::HOME);

        return $r === null ? null : ['x' => $r['params']['x'], 'y' => $r['params']['y']];
    }

    /** Soupeř 4 pole od (24,7) — dosáhne vedle nosiče i na míč ⇒ jen 1× GFI. Dřív (24,7). */
    public function testNosicNejdePresDveGfiNaDosahSoupere(): void
    {
        $this->assertSame(['x' => 23, 'y' => 7], $this->tahNosice(24, 3));
    }

    /** Soupeř 10 polí od (24,7) — nedoběhne ⇒ výjimka (1), 2× GFI smí. */
    public function testNosicSmiPresDveGfiZDosahu(): void
    {
        $this->assertSame(['x' => 24, 'y' => 7], $this->tahNosice(14, 7));
    }

    /** Ležící soupeř bez Jump Up: MA 6 − 3 + 2 = 5 polí; 7 polí od (24,7) ⇒ nedosáhne, 2× GFI smí. */
    public function testLeziciSouperDosahujeMene(): void
    {
        $this->assertSame(['x' => 24, 'y' => 7], $this->tahNosice(17, 7, PlayerState::PRONE));
    }
}
