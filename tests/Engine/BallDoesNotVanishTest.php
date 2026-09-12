<?php
declare(strict_types=1);

namespace App\Tests\Engine;

use App\Engine\{BallResolver, RandomDiceRoller, ScatterCalculator, TacklezoneCalculator};
use App\Enum\{PlayerState, TeamSide};
use PHPUnit\Framework\TestCase;

/**
 * ⛔⛔⛔ VADA PHP34 (12.09.2026): MÍČ ZMIZEL ZE HRY A HRA BĚŽELA DÁL.
 *
 * `handleBallOnPlayerDown` nastavovala `BallState::offPitch()`, když sražený
 * nosič **neměl pozici** — tedy když ho právě odstranilo zranění, KO nebo
 * crowd surf. Nic ho pak nevrátilo: zbytek půle se hrál **bez míče**,
 * nikdo ho nemohl zvednout a skórovat už nešlo.
 *
 * ⭐ ZMĚŘENO PŘED OPRAVOU: **16 kol z 317 (5 %)** začalo v HRATELNÉ fázi
 *    s míčem `offPitch`.
 *
 * ⭐ Míč přitom ví, kde byl — `BallState::carried()` nese pozici.
 */
final class BallDoesNotVanishTest extends TestCase
{
    public function testBallStaysInPlayWhenCarrierIsRemovedFromThePitch(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 10, 7, id: 1)
            ->addPlayer(TeamSide::AWAY, 20, 3, id: 2)
            ->withBallCarried(1)
            ->build();

        // SEBEKONTROLA FIXTURY: míč opravdu drží hráč 1.
        $this->assertTrue($state->getBall()->isHeld());
        $this->assertSame(1, $state->getBall()->getCarrierId());

        // Nosiče odstraní zranění: přijde o pozici i o stav na hřišti.
        $zraneny = $state->getPlayer(1)
            ->withState(PlayerState::INJURED)
            ->withPosition(null);
        $state = $state->withPlayer($zraneny);

        $dice = new RandomDiceRoller();
        $scatter = new ScatterCalculator();
        $resolver = new BallResolver($dice, new TacklezoneCalculator(), $scatter);

        [$novy, $events] = $resolver->handleBallOnPlayerDown($state, $zraneny, []);

        $this->assertFalse($novy->getBall()->isHeld(), 'míč nemá koho držet');
        $this->assertTrue($novy->getBall()->isOnPitch(),
            'míč zmizel ze hry -- zbytek půle by se hrál bez něj');
    }

    public function testBallStillDropsNormallyWhenCarrierStaysOnThePitch(): void
    {
        // ⭐ POZITIVNÍ KONTROLA OBRÁCENĚ: běžný pád se nesmí opravou změnit.
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 10, 7, id: 1)
            ->addPlayer(TeamSide::AWAY, 20, 3, id: 2)
            ->withBallCarried(1)
            ->build();
        $fallen = $state->getPlayer(1)->withState(PlayerState::PRONE);
        $state = $state->withPlayer($fallen);

        $scatter = new ScatterCalculator();
        $resolver = new BallResolver(new RandomDiceRoller(), new TacklezoneCalculator(), $scatter);
        [$novy] = $resolver->handleBallOnPlayerDown($state, $fallen, []);

        $this->assertFalse($novy->getBall()->isHeld());
        $this->assertTrue($novy->getBall()->isOnPitch());
    }
}
