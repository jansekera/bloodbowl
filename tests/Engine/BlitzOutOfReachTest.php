<?php
declare(strict_types=1);

namespace App\Tests\Engine;

use App\Engine\ActionResolver;
use App\Engine\FixedDiceRoller;
use App\Enum\ActionType;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use PHPUnit\Framework\TestCase;

/**
 * ⛔ VADA (11.09.2026): `BlitzHandler::resolve` házel
 *    `InvalidArgumentException('Cannot reach target for blitz')`, když se na
 *    sousední pole cíle nedosáhlo. Naměřeno 19 z 531 rozhodnutí (3,58 %)
 *    u kouče, proti kterému hraje člověk -- a živá cesta
 *    (`AITurnService::playTurn`) ten throw NECHYTÁ ani jednou.
 *
 * ⭐ PROČ JE TO VADA A NE OBRANA (uživatel 11.09.): vyhlásit blitz mimo dosah
 *    je LEGITIMNÍ TAH. Wild Animal (`rules_bb2016.txt` r. 8666-8669) hází
 *    D6 **+2 za Block nebo Blitz**, padá na 1-3 -- deklarace blitzu je tedy
 *    jediný způsob, jak Rat Ogra rozhýbat na přirozenou **2+** místo 4+.
 *    Na cíl přitom vůbec nemusí dosáhnout.
 */
final class BlitzOutOfReachTest extends TestCase
{
    public function testBlitzOutOfReachApproachesInsteadOfThrowing(): void
    {
        // Útočník MA1 na (5,7), cíl až na (20,7): dosah 1 << vzdálenost 15.
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, movement: 1, id: 1)
            ->addPlayer(TeamSide::AWAY, 20, 7, id: 2)
            ->withBallOffPitch()
            ->build();

        // SEBEKONTROLA FIXTURY: na cíl se OPRAVDU nedá dosáhnout. Bez tohohle
        // tvrzení by test prošel i nad stavem, kde blitz normálně vyjde --
        // a neměřil by nic.
        $att = $state->getPlayer(1);
        $def = $state->getPlayer(2);
        $this->assertNotNull($att->getPosition());
        $this->assertNotNull($def->getPosition());
        $this->assertGreaterThan(
            $att->getStats()->getMovement() + 2,          // +2 = maximum GFI
            $att->getPosition()->distanceTo($def->getPosition()) - 1,
            'fixtura je vadná: cíl je v dosahu, do opravované větve se nedojde',
        );

        $dice = new FixedDiceRoller([6, 6, 6, 6, 6, 6]);
        $resolver = new ActionResolver($dice);

        // Dřív tady letěla výjimka. Teď se má vrátit výsledek.
        $result = $resolver->resolve($state, ActionType::BLITZ, [
            'playerId' => 1, 'targetId' => 2,
        ]);

        $this->assertTrue($result->isSuccess(), 'blitz mimo dosah nesmí házet výjimku');
        $this->assertFalse($result->isTurnover());

        // A hráč se má PŘIBLÍŽIT, ne stát -- v tom je celý smysl deklarace.
        $after = $result->getNewState()->getPlayer(1);
        $this->assertNotNull($after->getPosition());
        $this->assertLessThan(
            $att->getPosition()->distanceTo($def->getPosition()),
            $after->getPosition()->distanceTo($def->getPosition()),
            'hráč se k cíli nepřiblížil',
        );

        // Blok se konat NESMÍ -- na cíl se nedosáhlo.
        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertNotContains('block', $types, 'blok bez dosažení cíle je nesmysl');
    }

    public function testBlitzOutOfReachAndImmobileStillDoesNotThrow(): void
    {
        // ⛔ Druhá polovina páru: ani když se hráč nemůže hnout VŮBEC,
        //    deklarace nesmí skončit výjimkou.
        //
        // ⚠️ MA 0 NESTAČÍ -- GFI dává 2 pole navíc bez ohledu na MA, takže
        //    hráč s MA0 se pořád pohne. (Tuhle verzi fixtury shodila vlastní
        //    sebekontrola: čekal jsem x=5, dostal x=7.) Nehybnost se vyrábí
        //    ZAZDĚNÍM, ne nulou v MA.
        $b = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, movement: 6, id: 1)
            ->addPlayer(TeamSide::AWAY, 20, 7, id: 2);
        $id = 3;
        foreach ([[-1,-1],[-1,0],[-1,1],[0,-1],[0,1],[1,-1],[1,0],[1,1]] as [$dx, $dy]) {
            $b = $b->addPlayer(TeamSide::HOME, 5 + $dx, 7 + $dy, id: $id++);
        }
        $state = $b->withBallOffPitch()->build();

        $dice = new FixedDiceRoller([6, 6, 6, 6]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::BLITZ, [
            'playerId' => 1, 'targetId' => 2,
        ]);

        $this->assertTrue($result->isSuccess());
        $this->assertFalse($result->isTurnover());
        $this->assertSame(5, $result->getNewState()->getPlayer(1)->getPosition()->getX(),
            'nemá se kam hnout, tak se hnout nemá');
        $this->assertSame(7, $result->getNewState()->getPlayer(1)->getPosition()->getY());
    }

    /**
     * ⭐⭐⭐ SCÉNÁŘ UŽIVATELE: vyhlásit blitz pro Rat Ogra, ať se pohne na 2+.
     *    Tohle je ten důvod, proč se deklarace mimo dosah NESMÍ zakázat.
     */
    public function testWildAnimalMovesOnTwoWhenBlitzIsDeclaredOutOfReach(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, movement: 6, strength: 5, id: 1,
                        skills: [SkillName::WildAnimal])
            ->addPlayer(TeamSide::AWAY, 20, 7, id: 2)
            ->withBallOffPitch()
            ->build();

        // SEBEKONTROLA: cíl je mimo dosah (MA6 + 2 GFI = 8 < 15-1).
        $att = $state->getPlayer(1);
        $def = $state->getPlayer(2);
        $this->assertGreaterThan(
            $att->getStats()->getMovement() + 2,
            $att->getPosition()->distanceTo($def->getPosition()) - 1,
            'fixtura je vadná: cíl je v dosahu',
        );

        // PŘIROZENÁ DVOJKA. Bez bonusu by padla (prah 4+); s bonusem za blitz
        // je to 2+, takže PROJDE. Přesně tvrzení „pohne se na 2 plus".
        $dice = new FixedDiceRoller([2, 6, 6, 6, 6, 6, 6]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::BLITZ, [
            'playerId' => 1, 'targetId' => 2,
        ]);

        $this->assertTrue($result->isSuccess());
        $after = $result->getNewState()->getPlayer(1);
        $this->assertLessThan(
            $att->getPosition()->distanceTo($def->getPosition()),
            $after->getPosition()->distanceTo($def->getPosition()),
            'Rat Ogre se na dvojce pohnout MĚL -- kvůli tomu se blitz vyhlašuje',
        );
    }

    public function testWildAnimalStillLosesTheActionOnOneEvenWhenBlitzing(): void
    {
        // ⛔ Druhá polovina páru k tomu výš: jednička padá i s bonusem
        //    (1+2=3, a 1-3 je pád) -- jinak by to byl auto-pass, tedy ta
        //    vada, kterou C++ engine opravil 07.08.2026.
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, movement: 6, strength: 5, id: 1,
                        skills: [SkillName::WildAnimal])
            ->addPlayer(TeamSide::AWAY, 20, 7, id: 2)
            ->withBallOffPitch()
            ->build();

        $dice = new FixedDiceRoller([1, 6, 6, 6, 6, 6]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::BLITZ, [
            'playerId' => 1, 'targetId' => 2,
        ]);

        $this->assertTrue($result->isSuccess());
        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('wild_animal', $types);
        $this->assertSame(5, $result->getNewState()->getPlayer(1)->getPosition()->getX(),
            'na jedničce se hráč hnout NESMÍ');
    }
}
