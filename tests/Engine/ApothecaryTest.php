<?php

declare(strict_types=1);

namespace App\Tests\Engine;

use App\DTO\TeamStateDTO;
use App\Engine\ActionResolver;
use App\Engine\FixedDiceRoller;
use App\Enum\ActionType;
use App\Enum\PlayerState;
use App\Enum\TeamSide;
use PHPUnit\Framework\TestCase;

/**
 * Lékárník, `rules_bb2016.txt` ř. 1200-1210 (P206, převod C++ 14.10.2026). PŘEPSÁNO: dosavadní
 * testy kódovaly jinou mechaniku — lékárník přehazoval CELÝ hod na zranění (výsledek „omráčen“),
 * jen v bloku. Pravidla: po Casualty soupeř hodí ZNOVU NA TABULKU CASUALTY a platí výsledek, který
 * se vybere; je-li to Badly Hurt, hráč jde do REZERV (hraje dál). Jednou za zápas.
 * Kdy ho AI (strana bez kouče) použije — stejné pravidlo jako C++ `injury.cpp`: jen na Badly Hurt
 * nebo smrt a jen pro hráče, kterého nenahradí stejný hráč z lavičky.
 *
 * Kostky bloku: ST4 × ST3 = 2 kostky, 6,6 = Defender Down · brnění 5+4 = 9 > AV7 · zranění 5+5 = 10
 * · tabulka Casualty D6 (desítky) + D8 (jednotky).
 */
final class ApothecaryTest extends TestCase
{
    private function blok(FixedDiceRoller $dice, ?TeamStateDTO $away = null, bool $lavicka = false): \App\DTO\ActionResult
    {
        $b = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, strength: 4, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 7, strength: 3, armour: 7, id: 2);
        if ($away !== null) {
            $b = $b->withAwayTeam($away);
        }
        if ($lavicka) {
            $b = $b->addOffPitchPlayer(TeamSide::AWAY, armour: 7, id: 3);   // stejný hráč (Lineman) v rezervách
        }

        return (new ActionResolver($dice))->resolve($b->build(), ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);
    }

    public function testMrtvy_LekarnikPrehodiTabulku_BadlyHurtDoRezerv(): void
    {
        // Casualty 6+1 = mrtev · lékárník 1+1 = Badly Hurt ⇒ rezervy
        $r = $this->blok(new FixedDiceRoller([6, 6, 5, 4, 5, 5, 6, 1, 1, 1]));
        $obrance = $r->getNewState()->requirePlayer(2);
        $this->assertSame(PlayerState::OFF_PITCH, $obrance->getState(), 'Badly Hurt po lékárníkovi = rezervy');
        $this->assertTrue($r->getNewState()->getAwayTeam()->isApothecaryUsed());
        $this->assertNotEmpty(array_filter($r->getEvents(), fn($e) => $e->getType() === 'apothecary'));
    }

    public function testMrtvy_DruhyHodHorsi_PlatiPrvni(): void
    {
        // Casualty 6+1 = mrtev, lékárník 6+2 = mrtev ⇒ zůstává mrtev (lékárník spotřebován)
        $r = $this->blok(new FixedDiceRoller([6, 6, 5, 4, 5, 5, 6, 1, 6, 2]));
        $this->assertSame(PlayerState::DEAD, $r->getNewState()->requirePlayer(2)->getState());
        $this->assertTrue($r->getNewState()->getAwayTeam()->isApothecaryUsed());
    }

    public function testBadlyHurt_RovnouDoRezerv(): void
    {
        // ř. 1208-1210 "(even if it was the original Casualty roll)": Badly Hurt + lékárník ⇒ rezervy
        // bez dalšího hodu, který by stál za to; AI ho tu použije (Badly Hurt = jistý návrat).
        $r = $this->blok(new FixedDiceRoller([6, 6, 5, 4, 5, 5, 1, 1, 4, 1]));
        $this->assertSame(PlayerState::OFF_PITCH, $r->getNewState()->requirePlayer(2)->getState());
        $this->assertTrue($r->getNewState()->getAwayTeam()->isApothecaryUsed());
    }

    public function testStredniPasmo_AiLekarnikaSetri(): void
    {
        // Casualty 4+1 = Miss Next Game: AI lékárníka šetří (stejně jako C++)
        $r = $this->blok(new FixedDiceRoller([6, 6, 5, 4, 5, 5, 4, 1]));
        $this->assertSame(PlayerState::INJURED, $r->getNewState()->requirePlayer(2)->getState());
        $this->assertFalse($r->getNewState()->getAwayTeam()->isApothecaryUsed());
    }

    public function testNahraditelnyZLavicky_AiLekarnikaSetri(): void
    {
        $r = $this->blok(new FixedDiceRoller([6, 6, 5, 4, 5, 5, 6, 1]), lavicka: true);
        $this->assertSame(PlayerState::DEAD, $r->getNewState()->requirePlayer(2)->getState());
        $this->assertFalse($r->getNewState()->getAwayTeam()->isApothecaryUsed());
    }

    public function testKo_AiLekarnikaNepouzije(): void
    {
        $r = $this->blok(new FixedDiceRoller([6, 6, 5, 4, 4, 4]));
        $this->assertSame(PlayerState::KO, $r->getNewState()->requirePlayer(2)->getState());
        $this->assertFalse($r->getNewState()->getAwayTeam()->isApothecaryUsed());
    }

    public function testJenJednouZaZapas(): void
    {
        $away = TeamStateDTO::create(2, 'Away', 'Orc', TeamSide::AWAY, 2)->withApothecaryUsed();
        $r = $this->blok(new FixedDiceRoller([6, 6, 5, 4, 5, 5, 6, 1]), $away);
        $this->assertSame(PlayerState::DEAD, $r->getNewState()->requirePlayer(2)->getState());
    }

    public function testPlatiIUFaulu(): void
    {
        // Jedno místo pro všechna zranění: dřív lékárník jen v bloku. Faul: brnění 5+4 = 9 > AV7,
        // zranění 5+6 = 11 (bez dubletu), Casualty 6+1 mrtev · lékárník 1+2 Badly Hurt ⇒ rezervy
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1)
            ->addPronePlayer(TeamSide::AWAY, 6, 7, armour: 7, id: 2)
            ->build();
        $r = (new ActionResolver(new FixedDiceRoller([5, 4, 5, 6, 6, 1, 1, 2])))
            ->resolve($state, ActionType::FOUL, ['playerId' => 1, 'targetId' => 2]);
        $this->assertSame(PlayerState::OFF_PITCH, $r->getNewState()->requirePlayer(2)->getState());
        $this->assertTrue($r->getNewState()->getAwayTeam()->isApothecaryUsed());
    }
}
