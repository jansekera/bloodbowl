<?php

declare(strict_types=1);

namespace App\Tests\Engine;

use App\Engine\BallResolver;
use App\Engine\KickoffResolver;
use App\Engine\RandomDiceRoller;
use App\Engine\ScatterCalculator;
use App\Engine\TacklezoneCalculator;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use PHPUnit\Framework\TestCase;

/**
 * ⭐⭐ PHP31: VYBER POLE, KAM SE KOPE.
 *
 * ⛔ Do 14.09.2026 se kopalo na JEDNO natvrdo zadane pole. Tenhle test je
 *   zaroven POZITIVNI KONTROLA k tvrzeni "vyber neco dela": kdyby
 *   `chooseKickTarget()` vracelo porad totez, spadne.
 */
final class KickTargetChoiceTest extends TestCase
{
    private function resolver(): KickoffResolver
    {
        $tz = new TacklezoneCalculator();
        $dice = new RandomDiceRoller();

        return new KickoffResolver($dice, new ScatterCalculator(), new BallResolver($dice, $tz, new ScatterCalculator()));
    }

    /**
     * @param list<int> $ma  pohyb hracu prijimajiciho tymu
     */
    private function stav(bool $kopajiciMaKick, array $ma, TeamSide $prijima)
    {
        $b = (new GameStateBuilder())->withActiveTeam($prijima);
        $kopajici = $prijima->opponent();

        // Kopajici tym: jeden hrac, volitelne s Kick. Musi stat MIMO siroky
        // pas (y 4..10) a MIMO lajnu (x 12 pro HOME, 13 pro AWAY).
        $b->addPlayer($kopajici, $kopajici === TeamSide::HOME ? 10 : 15, 7,
            skills: $kopajiciMaKick ? [SkillName::Kick] : [], id: 1);

        $i = 100;
        foreach ($ma as $m) {
            $b->addPlayer($prijima, $prijima === TeamSide::HOME ? 4 : 21, 7, movement: $m, id: $i++);
        }

        return $b->build()->withKickingTeam($kopajici);
    }

    public function testBezKickJeJedinaBezpecnaVolbaAProtoSeNemeni(): void
    {
        // Rozptyl az 6 poli => v pulce sirokе 13 poli zbyva JEDINE pole,
        // ze ktereho mic nemuze odskakat ani z hriste, ani za pulici caru.
        $r = $this->resolver();

        $pomaly = $r->chooseKickTarget($this->stav(false, [4, 4, 4], TeamSide::HOME), TeamSide::HOME);
        $rychly = $r->chooseKickTarget($this->stav(false, [8, 8, 8], TeamSide::HOME), TeamSide::HOME);

        $this->assertSame(6, $pomaly->getX(), 'bez Kick existuje jen x=6');
        $this->assertSame(6, $rychly->getX(), 'bez Kick se rychlosti nic nemeni -- neni z ceho vybirat');
        $this->assertSame(7, $pomaly->getY());
    }

    public function testSKickemSeProtiPomalemuKopeHloubejiNezProtiRychlemu(): void
    {
        $r = $this->resolver();

        // HOME prijima => jeho pulka je x 0..12, jeho koncova zona je x=0.
        // "Hluboko" je tedy MALE x, "kratko" (k lajne) velke x.
        $protiPomalemu = $r->chooseKickTarget($this->stav(true, [4, 4, 4], TeamSide::HOME), TeamSide::HOME);
        $protiRychlemu = $r->chooseKickTarget($this->stav(true, [8, 8, 8], TeamSide::HOME), TeamSide::HOME);

        $this->assertSame(3, $protiPomalemu->getX(), 'proti pomalemu se kope hluboko');
        $this->assertSame(9, $protiRychlemu->getX(), 'proti rychlemu kratko k lajne');
        $this->assertNotSame(
            $protiPomalemu->getX(),
            $protiRychlemu->getX(),
            'POZITIVNI KONTROLA: kdyby vyber nic nedelal, byla by tahle dve cisla stejna',
        );
    }

    public function testTotezZeStranyAWAYJenZrcadlove(): void
    {
        $r = $this->resolver();

        // AWAY prijima => pulka x 13..25, koncova zona x=25 => hluboko je VELKE x.
        $protiPomalemu = $r->chooseKickTarget($this->stav(true, [4, 4, 4], TeamSide::AWAY), TeamSide::AWAY);
        $protiRychlemu = $r->chooseKickTarget($this->stav(true, [8, 8, 8], TeamSide::AWAY), TeamSide::AWAY);

        $this->assertSame(22, $protiPomalemu->getX(), 'proti pomalemu hluboko = k jejich koncove zone');
        $this->assertSame(16, $protiRychlemu->getX(), 'proti rychlemu kratko k lajne');
    }

    public function testZadnyCilNedovoliTouchbackAniPriNejhorsimRozptylu(): void
    {
        // ⛔ Tvrda podminka z pravidel (r. 280-282): mic nesmi odskakat
        //   z hriste ani do pulky kopajiciho tymu.
        $r = $this->resolver();

        foreach ([[false, 6], [true, 3]] as [$maKick, $rozptyl]) {
            foreach ([TeamSide::HOME, TeamSide::AWAY] as $prijima) {
                foreach ([[4, 4, 4], [8, 8, 8]] as $ma) {
                    $cil = $r->chooseKickTarget($this->stav($maKick, $ma, $prijima), $prijima);
                    [$lo, $hi] = $prijima === TeamSide::HOME ? [0, 12] : [13, 25];

                    $this->assertGreaterThanOrEqual($lo + $rozptyl, $cil->getX());
                    $this->assertLessThanOrEqual($hi - $rozptyl, $cil->getX());
                    $this->assertGreaterThanOrEqual($rozptyl, $cil->getY());
                    $this->assertLessThanOrEqual(14 - $rozptyl, $cil->getY());
                }
            }
        }
    }
}
