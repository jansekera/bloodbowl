<?php

declare(strict_types=1);

namespace App\Tests\Engine;

use App\DTO\ActionResult;
use App\DTO\GameState;
use App\Engine\ActionResolver;
use App\Engine\FixedDiceRoller;
use App\Enum\ActionType;
use App\Enum\PlayerState;
use App\Enum\TeamSide;
use PHPUnit\Framework\TestCase;

/**
 * PÁD PŘI ÚHYBU A GFI: KDE HRÁČ LEŽÍ A ŽE SE HÁZÍ NA ZRANĚNÍ (audit parity 08.10.2026, nálezy 4a a 4b).
 *
 * `rules_bb2016.txt` ř. 496-499 (úhyb): "If the D6 roll is less than the required total, then
 * the player is Knocked Down **in the square he was dodging to** and **a roll must be made to
 * see if he was injured**."
 * ř. 1701-1703 (GFI): "On a roll of 1 the player trips up and is Knocked Down **in the square
 * that they moved to**. **Roll to see if he was injured.**"
 * ř. 678-681: nosič "will drop the ball **in the square where they fall**. The dropped ball
 * will bounce one square ... after the player's armour and injury rolls (if any) are fully
 * resolved."
 *
 * Stará mechanika:
 *   4a -- po neúspěšném úhybu hráč ležel na VÝCHOZÍM poli (`MoveHandler` i `RerollHandler`);
 *   4b -- cesta přes dialog přehozu (`RerollHandler`, interaktivní hra člověka) po pádu
 *         nehodila na brnění ani na zranění, u úhybu ani u GFI.
 *
 * Fixtura úhybu: HOME id 1 na (5,5), AWAY na (5,4) ⇒ krok na (5,6) je úhyb na 3+ (cílové pole
 * není v zóně). Výchozí a cílové pole se liší, takže obě mechaniky dávají jinou polohu.
 */
final class FallSquareAndInjuryOnFailedMoveTest extends TestCase
{
    private function stavUhyb(int $rerolls, bool $nosic = false): GameState
    {
        $b = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, id: 1)
            ->addPlayer(TeamSide::AWAY, 5, 4, id: 2);
        if ($nosic) {
            $b->withBallCarried(1);
        }
        $s = $b->build();

        return $s->withTeamState(TeamSide::HOME, $s->getTeamState(TeamSide::HOME)->withRerolls($rerolls));
    }

    /** HOME id 1 s MA 1 jde o dvě pole ⇒ druhý krok na (5,7) je GFI. Žádný soupeř nablízku. */
    private function stavGfi(int $rerolls): GameState
    {
        $s = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, movement: 1, id: 1)
            ->addPlayer(TeamSide::AWAY, 20, 12, id: 2)
            ->build();

        return $s->withTeamState(TeamSide::HOME, $s->getTeamState(TeamSide::HOME)->withRerolls($rerolls));
    }

    /** @return list<string> */
    private function typy(ActionResult $r): array
    {
        return array_map(fn($e) => $e->getType(), $r->getEvents());
    }

    /** @return array{int, int} */
    private function pole(GameState $s, int $id): array
    {
        $p = $s->requirePlayer($id)->requirePosition();

        return [$p->getX(), $p->getY()];
    }

    // === 4a: pole pádu ===

    public function testPoNeuspesnemUhybuLeziHracNaPoliKamUhybal(): void
    {
        // Úhyb 2 (cíl 3+) = neúspěch, bez přehozu. Brnění 1+1 neprorazí.
        $r = (new ActionResolver(new FixedDiceRoller([2, 1, 1])))
            ->resolve($this->stavUhyb(0), ActionType::MOVE, ['playerId' => 1, 'x' => 5, 'y' => 6]);

        $this->assertTrue($r->isTurnover());
        $this->assertSame(PlayerState::PRONE, $r->getNewState()->requirePlayer(1)->getState());
        $this->assertSame([5, 6], $this->pole($r->getNewState(), 1), 'ř. 497-498: "Knocked Down in the square he was dodging to"');
    }

    public function testNosicPoNeuspesnemUhybuPoustiMicNaPoliKamUhybal(): void
    {
        // Úhyb 2 = pád · brnění 1+1 · odskok D8 = 5 (o pole "dolů", +y).
        // Z cílového pole (5,6) dopadne míč na (5,7); z výchozího (5,5) by dopadl na (5,6).
        $r = (new ActionResolver(new FixedDiceRoller([2, 1, 1, 5])))
            ->resolve($this->stavUhyb(0, nosic: true), ActionType::MOVE, ['playerId' => 1, 'x' => 5, 'y' => 6]);

        $mic = $r->getNewState()->getBall()->getPosition();
        $this->assertNotNull($mic);
        $this->assertSame([5, 7], [$mic->getX(), $mic->getY()], 'ř. 678-679: míč padá na poli, kde hráč spadl');
    }

    public function testNosicVyrazenyZranenimPoustiMicNaPoliKamUhybal(): void
    {
        // Úhyb 2 = pád · brnění 6+6 prorazí · zranění 4+4 = KO (hráč mizí ze hřiště) · odskok D8 = 5.
        $r = (new ActionResolver(new FixedDiceRoller([2, 6, 6, 4, 4, 5])))
            ->resolve($this->stavUhyb(0, nosic: true), ActionType::MOVE, ['playerId' => 1, 'x' => 5, 'y' => 6]);

        $this->assertSame(PlayerState::KO, $r->getNewState()->requirePlayer(1)->getState(), 'fixtura: nosič je KO');
        $mic = $r->getNewState()->getBall()->getPosition();
        $this->assertNotNull($mic);
        $this->assertSame([5, 7], [$mic->getX(), $mic->getY()], 'ř. 678-679: míč padá na poli, kde hráč spadl');
    }

    // === 4a + 4b: cesta přes dialog přehozu ===

    /** První hod ($dice[0]) selže a vznikne dialog přehozu; vrací resolver a stav s dialogem. */
    private function sDialogem(GameState $state, ActionResolver $resolver, int $x, int $y): GameState
    {
        $resolver->setInteractiveRerolls(true);
        $r = $resolver->resolve($state, ActionType::MOVE, ['playerId' => 1, 'x' => $x, 'y' => $y]);
        $this->assertNotNull($r->getNewState()->getPendingReroll(), 'fixtura: vznikl dialog přehozu');

        return $r->getNewState();
    }

    public function testOdmitnutyPrehozUhybuPadNaCilovemPoliAHodNaBrneni(): void
    {
        // Úhyb 2 = neúspěch → dialog → odmítnuto · brnění 1+1 neprorazí.
        $dice = new FixedDiceRoller([2, 1, 1]);
        $resolver = new ActionResolver($dice);
        $state = $this->sDialogem($this->stavUhyb(1), $resolver, 5, 6);

        $r = $resolver->resolve($state, ActionType::RESOLVE_REROLL, ['choice' => 'decline']);

        $this->assertTrue($r->isTurnover());
        $this->assertSame([5, 6], $this->pole($r->getNewState(), 1), 'ř. 497-498: pád na poli, kam uhýbal');
        $this->assertContains('armour_roll', $this->typy($r), 'ř. 498-499: "a roll must be made to see if he was injured"');
        $this->assertSame(3, $dice->getRollCount(), 'úhyb + 2 kostky brnění');
    }

    public function testNeuspesnyTymovyPrehozUhybuHodNaBrneniIZraneni(): void
    {
        // Úhyb 2 → dialog → týmový přehoz 2 = zase neúspěch · brnění 6+6 prorazí · zranění 1+1 = omráčen.
        $resolver = new ActionResolver(new FixedDiceRoller([2, 2, 6, 6, 1, 1]));
        $state = $this->sDialogem($this->stavUhyb(1), $resolver, 5, 6);

        $r = $resolver->resolve($state, ActionType::RESOLVE_REROLL, ['choice' => 'team_reroll']);

        $this->assertTrue($r->isTurnover());
        $this->assertContains('injury_roll', $this->typy($r), 'ř. 498-499: po proraženém brnění hod na zranění');
        $this->assertSame(PlayerState::STUNNED, $r->getNewState()->requirePlayer(1)->getState());
        $this->assertSame([5, 6], $this->pole($r->getNewState(), 1));
    }

    public function testOdmitnutyPrehozGfiPadNaCilovemPoliAHodNaBrneni(): void
    {
        // GFI 1 = neúspěch → dialog → odmítnuto · brnění 6+6 prorazí · zranění 1+1 = omráčen.
        $resolver = new ActionResolver(new FixedDiceRoller([1, 6, 6, 1, 1]));
        $state = $this->sDialogem($this->stavGfi(1), $resolver, 5, 7);

        $r = $resolver->resolve($state, ActionType::RESOLVE_REROLL, ['choice' => 'decline']);

        $this->assertTrue($r->isTurnover());
        $this->assertSame([5, 7], $this->pole($r->getNewState(), 1), 'ř. 1702-1703: "Knocked Down in the square that they moved to"');
        $this->assertContains('armour_roll', $this->typy($r), 'ř. 1703: "Roll to see if he was injured"');
        $this->assertSame(PlayerState::STUNNED, $r->getNewState()->requirePlayer(1)->getState());
    }

    public function testGfiBezDialoguPadNaCilovemPoliAHodNaBrneni(): void
    {
        // Kontrola druhé cesty (bez dialogu, AI): tady to platilo už před opravou.
        $r = (new ActionResolver(new FixedDiceRoller([1, 1, 1])))
            ->resolve($this->stavGfi(0), ActionType::MOVE, ['playerId' => 1, 'x' => 5, 'y' => 7]);

        $this->assertTrue($r->isTurnover());
        $this->assertSame([5, 7], $this->pole($r->getNewState(), 1));
        $this->assertContains('armour_roll', $this->typy($r));
    }

    public function testNosicPoOdmitnutemPrehozuPoustiMicAzPoZraneniZCilovehoPole(): void
    {
        // Úhyb 2 → dialog → odmítnuto · brnění 1+1 · odskok D8 = 5 z (5,6) na (5,7).
        // Stará mechanika brnění neházela: jednička by šla do odskoku (D8 = 1) z (5,5) na (5,4).
        $resolver = new ActionResolver(new FixedDiceRoller([2, 1, 1, 5]));
        $state = $this->sDialogem($this->stavUhyb(1, nosic: true), $resolver, 5, 6);

        $r = $resolver->resolve($state, ActionType::RESOLVE_REROLL, ['choice' => 'decline']);

        $mic = $r->getNewState()->getBall()->getPosition();
        $this->assertNotNull($mic);
        $this->assertSame([5, 7], [$mic->getX(), $mic->getY()], 'ř. 678-681: míč odskakuje z pole pádu až po brnění');
    }
}
