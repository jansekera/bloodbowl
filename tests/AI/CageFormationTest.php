<?php
declare(strict_types=1);

namespace App\Tests\AI;

use App\AI\LearningAICoach;
use App\Engine\RulesEngine;
use App\Enum\ActionType;
use App\Enum\TeamSide;
use App\Tests\Engine\GameStateBuilder;
use PHPUnit\Framework\TestCase;

/**
 * ⭐⭐ KLEC — PHP33, zadání uživatele 12.09.2026:
 *    *„začni správným postavením klece a pak pohybem celé klece dopředu."*
 *
 * Definice, kterou uživatel potvrdil: **klec = nosič + ČTYŘI DIAGONÁLNÍ ROHY.**
 * Ortogonální soused je k ničemu — soupeř na nosiče dosáhne stejně.
 *
 * ⛔ Do 12.09. dával kouč `+1.0` za **jakékoli** sousední pole ⇒ vznikala
 *    hvězda kolem nosiče, ne klec.
 */
final class CageFormationTest extends TestCase
{
    /** Nosič HOME uprostřed, jeden volný spoluhráč, soupeř daleko. */
    private function stateWithCarrierAndOneHelper(int $helperX, int $helperY): \App\DTO\GameState
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 10, 7, movement: 6, id: 1)          // nosič
            ->addPlayer(TeamSide::HOME, $helperX, $helperY, movement: 6, id: 2)
            ->addPlayer(TeamSide::AWAY, 24, 1, id: 3)
            ->withBallCarried(1)
            ->build();

        // Nosič už jednal ⇒ rozhoduje se jen o pomocníkovi.
        return $state->withPlayer(
            $state->getPlayer(1)->withHasActed(true)->withHasMoved(true),
        );
    }

    public function testPlayerAlreadyOnACornerHoldsPosition(): void
    {
        // Pomocník stojí na rohu (9,6) — diagonála od nosiče (10,7).
        $state = $this->stateWithCarrierAndOneHelper(9, 6);
        $rules = new RulesEngine();

        // SEBEKONTROLA FIXTURY: hráč se opravdu MŮŽE hnout, takže „zůstal stát"
        // je rozhodnutí, ne nedostatek možností.
        $this->assertNotSame([], $rules->getValidMoveTargets($state, 2),
            'fixtura je vadná: hráč nemá kam, držení pozice by nic neznamenalo');

        $decision = (new LearningAICoach())->decideAction($state, $rules);

        $this->assertSame(ActionType::STAND_PAT, $decision['action'],
            'hráč stál v rohu klece a přesto se hnul — klec se tím rozsype');
        $this->assertSame(2, $decision['params']['playerId']);
    }

    public function testPlayerOffTheCageMovesToACornerNotToAnEdge(): void
    {
        // Pomocník stojí dál (12,7) a má se zařadit. Roh je diagonála,
        // hrana (např. 11,7 nebo 10,6) je k ničemu.
        $state = $this->stateWithCarrierAndOneHelper(13, 7);
        $rules = new RulesEngine();

        $decision = (new LearningAICoach())->decideAction($state, $rules);

        $this->assertSame(ActionType::MOVE, $decision['action']);
        $dx = abs($decision['params']['x'] - 10);
        $dy = abs($decision['params']['y'] - 7);
        $this->assertTrue($dx === 1 && $dy === 1,
            sprintf('kouč šel na (%d,%d) — to je hrana nebo mimo klec, ne roh',
                $decision['params']['x'], $decision['params']['y']));
    }

    public function testHoldingBeatsShufflingToAnotherCorner(): void
    {
        // ⭐ Držení musí přebít přesun na JINÝ roh, jinak by kouč rohy jen
        //    přehazoval a klec by se pořád rozpadala a skládala.
        $state = $this->stateWithCarrierAndOneHelper(11, 8);   // taky roh
        $rules = new RulesEngine();

        $decision = (new LearningAICoach())->decideAction($state, $rules);

        $this->assertSame(ActionType::STAND_PAT, $decision['action']);
    }
}
