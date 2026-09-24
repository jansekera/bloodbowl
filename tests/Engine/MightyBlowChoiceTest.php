<?php

declare(strict_types=1);

namespace App\Tests\Engine;

use App\Engine\FixedDiceRoller;
use App\Engine\InjuryResolver;
use App\Enum\PlayerState;
use App\Enum\TeamSide;
use App\ValueObject\PlayerStats;
use App\DTO\MatchPlayerDTO;
use App\ValueObject\Position;
use PHPUnit\Framework\TestCase;

/**
 * ⭐⭐ MIGHTY BLOW JE VOLBA, NE PEVNY BONUS NA BRNENI.
 *
 * `rules_bb2016.txt` r. 8291-8297: "Add 1 to ANY Armour OR Injury roll…
 * you only modify ONE of the dice rolls."
 *
 * ⛔ Do 14.09.2026 se predaval vzdy jako modifikator BRNENI, takze kdyz
 *   se brneni prolomilo i bez nej, bonus PROPADL.
 *
 * Tenhle test je POZITIVNI KONTROLA: kdyby volba nefungovala, prvni
 * pripad by skoncil KO misto zraneni.
 */
final class MightyBlowChoiceTest extends TestCase
{
    private function obranceAV9(): MatchPlayerDTO
    {
        return MatchPlayerDTO::create(
            id: 1,
            playerId: 1,
            name: 'Obrance',
            number: 1,
            positionalName: 'Lineman',
            stats: new PlayerStats(6, 3, 3, 9),   // AV 9
            skills: [],
            teamSide: TeamSide::HOME,
            position: new Position(5, 7),
        );
    }

    public function testKdyzBrneniPadneSamoJdeMightyBlowNaZraneni(): void
    {
        $res = new InjuryResolver();

        // Brneni: 5+5 = 10 > AV 9 => padne i BEZ Mighty Blow.
        // Zraneni: 4+5 = 9 => bez bonusu KO (8-9), s bonusem 10 => ZRANENI.
        // ⭐ +[1, 1] = hod na tabulce nasledku (D68) => 11 = Badly Hurt.
        //   ⛔ Puvodni posloupnost koncila nepouzitou sestkou; od 24.09.2026 ji cte
        //   tabulka nasledku jako DESITKY a 6x je DEAD. Proto je nahrazena, ne doplnena.
        $bezMB = $res->resolve($this->obranceAV9(), new FixedDiceRoller([5, 5, 4, 5, 1, 1]), 0, 0, false, false, false, false);
        $sMB   = $res->resolve($this->obranceAV9(), new FixedDiceRoller([5, 5, 4, 5, 1, 1]), 0, 0, false, false, false, true);

        $this->assertSame(PlayerState::KO, $bezMB['player']->getState(), 'bez MB ma vyjit KO');
        $this->assertSame(
            PlayerState::INJURED,
            $sMB['player']->getState(),
            'POZITIVNI KONTROLA: MB nesmi propadnout, kdyz brneni padlo samo -- ma prejit na zraneni',
        );
    }

    public function testKdyzBrneniSamoNepadneUtratiSeMightyBlowNaNej(): void
    {
        $res = new InjuryResolver();

        // Brneni: 4+5 = 9, AV 9 => samo NEPADNE (potreba > 9).
        // S Mighty Blow 9+1 = 10 > 9 => padne, a nasleduje hod na zraneni.
        $bezMB = $res->resolve($this->obranceAV9(), new FixedDiceRoller([4, 5, 1, 1, 1]), 0, 0, false, false, false, false);
        $sMB   = $res->resolve($this->obranceAV9(), new FixedDiceRoller([4, 5, 1, 1, 1]), 0, 0, false, false, false, true);

        $this->assertSame(PlayerState::STANDING, $bezMB['player']->getState(), 'bez MB brneni drzi');
        $this->assertNotSame(
            PlayerState::STANDING,
            $sMB['player']->getState(),
            'MB se ma utratit na brneni, kdyz je to jedina cesta, jak ho prolomit',
        );
    }

    public function testMightyBlowSeNikdyNeuplatniDVAKRAT(): void
    {
        $res = new InjuryResolver();

        // Brneni padne samo (10 > 9) => MB jde na zraneni.
        // Zraneni 1+1 = 2, s bonusem 3 -- porad hluboko ve STUNNED pasmu.
        // Kdyby se MB pricetlo i k brneni i ke zraneni, nic se nezmeni,
        // ale test hlida, ze se aspon nezdvoji do jineho pasma.
        $v = $res->resolve($this->obranceAV9(), new FixedDiceRoller([5, 5, 1, 1, 1]), 0, 0, false, false, false, true);

        $this->assertSame(PlayerState::STUNNED, $v['player']->getState());
    }
}
