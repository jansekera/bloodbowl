<?php

declare(strict_types=1);

namespace App\Tests\Engine;

use App\DTO\GameState;
use App\Engine\ActionResolver;
use App\Engine\FixedDiceRoller;
use App\Enum\ActionType;
use App\Enum\PlayerState;
use App\Enum\TeamSide;
use PHPUnit\Framework\TestCase;

/**
 * OMRÁČENÝ SE OTÁČÍ NA KONCI PŘÍŠTÍHO KOLA SVÉHO TÝMU (audit parity 08.10.2026, nález 1).
 *
 * `rules_bb2016.txt` ř. 703-709 (tabulka zranění, 2-7 Stunned): "Leave the player on the
 * pitch, but turn them face-down. All face-down players are turned face up **at the end of
 * their team's next turn**, even if a turnover takes place. Note that a player **may not
 * turn face up on the turn they are Stunned**. Once face-up they may stand up on any
 * subsequent turn using the normal rules."
 *
 * Stará mechanika otáčela na ZAČÁTKU kola vlastního týmu, takže omráčený v nejbližším
 * vlastním kole vstal a hrál — omráčení se nelišilo od sražení.
 */
final class StunnedTurnsFaceUpTest extends TestCase
{
    private function konecKola(GameState $state): GameState
    {
        $r = (new ActionResolver(new FixedDiceRoller([])))->resolve($state, ActionType::END_TURN, []);
        $this->assertTrue($r->isSuccess());

        return $r->getNewState();
    }

    /** HOME (id 1) srazí blokem AWAY (id 2) a ten je omráčen: kostka 6 = Defender Down, brnění 5+4 > AV7, zranění 3+3 = Stunned. */
    private function awayOmracenVKoleHome(): GameState
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, armour: 7, id: 2)
            ->build();

        $r = (new ActionResolver(new FixedDiceRoller([6, 5, 4, 3, 3])))
            ->resolve($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);
        $this->assertSame(PlayerState::STUNNED, $r->getNewState()->requirePlayer(2)->getState(), 'fixtura: AWAY je omráčen');

        return $r->getNewState();
    }

    /** HOME (id 1) neuhne (1), brnění 6+6 prorazí AV8, zranění 1+1 = Stunned — omráčen ve VLASTNÍM kole. */
    private function homeOmracenVeVlastnimKole(): GameState
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 7, id: 2)
            ->addPlayer(TeamSide::AWAY, 6, 8, id: 3)
            ->build();
        $state = $state->withTeamState(TeamSide::HOME, $state->getTeamState(TeamSide::HOME)->withRerolls(0));

        $r = (new ActionResolver(new FixedDiceRoller([1, 6, 6, 1, 1])))
            ->resolve($state, ActionType::MOVE, ['playerId' => 1, 'x' => 4, 'y' => 6]);
        $this->assertTrue($r->isTurnover(), 'fixtura: pád při úhybu');
        $this->assertSame(PlayerState::STUNNED, $r->getNewState()->requirePlayer(1)->getState(), 'fixtura: HOME je omráčen');

        return $r->getNewState();
    }

    public function testOmracenyVSouperoveKoleLeziLicemDoluCeleSvePristiKolo(): void
    {
        $state = $this->konecKola($this->awayOmracenVKoleHome());

        $this->assertSame(TeamSide::AWAY, $state->getActiveTeam());
        $this->assertSame(
            PlayerState::STUNNED,
            $state->requirePlayer(2)->getState(),
            'ř. 704-705: lícem nahoru až na KONCI příštího kola svého týmu — během něj je pořád omráčený',
        );
    }

    public function testOmracenyVSouperoveKoleSeOtociNaKonciSvehoPristihoKola(): void
    {
        $state = $this->konecKola($this->konecKola($this->awayOmracenVKoleHome()));

        $this->assertSame(TeamSide::HOME, $state->getActiveTeam());
        $this->assertSame(
            PlayerState::PRONE,
            $state->requirePlayer(2)->getState(),
            'ř. 704-705: na konci příštího kola svého týmu se otočí lícem nahoru',
        );
    }

    public function testOmracenyVeVlastnimKoleSeNaJehoKonciNeotoci(): void
    {
        // Konec kola HOME (ve kterém byl omráčen) a celé kolo AWAY: pořád lícem dolů.
        $poKoleHome = $this->konecKola($this->homeOmracenVeVlastnimKole());
        $this->assertSame(
            PlayerState::STUNNED,
            $poKoleHome->requirePlayer(1)->getState(),
            'ř. 706-707: v kole, kdy byl omráčen, se neotáčí',
        );

        // Začalo příští kolo HOME — otáčí se až na jeho konci, ne na začátku.
        $pristiKoloHome = $this->konecKola($poKoleHome);
        $this->assertSame(TeamSide::HOME, $pristiKoloHome->getActiveTeam());
        $this->assertSame(
            PlayerState::STUNNED,
            $pristiKoloHome->requirePlayer(1)->getState(),
            'ř. 704-705: během příštího kola svého týmu je pořád omráčený',
        );

        $this->assertSame(
            PlayerState::PRONE,
            $this->konecKola($pristiKoloHome)->requirePlayer(1)->getState(),
            'ř. 704-705: na konci příštího kola svého týmu se otočí',
        );
    }

    public function testOtoceniPlatiIPriTurnoveru(): void
    {
        // ř. 705-706: "even if a turnover takes place". AWAY (id 2) je omráčen z kola HOME;
        // kolo AWAY skončí turnoverem (id 3 neuhne) a omráčený se přesto otočí.
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, armour: 7, id: 2)
            ->addPlayer(TeamSide::AWAY, 10, 10, id: 3)
            ->addPlayer(TeamSide::HOME, 11, 10, id: 4)
            ->build();
        $state = $state->withTeamState(TeamSide::AWAY, $state->getTeamState(TeamSide::AWAY)->withRerolls(0));
        $r = (new ActionResolver(new FixedDiceRoller([6, 5, 4, 3, 3])))
            ->resolve($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);
        $state = $this->konecKola($r->getNewState());

        // Úhyb 1 = pád, brnění 1+1 neprorazí.
        $r = (new ActionResolver(new FixedDiceRoller([1, 1, 1])))
            ->resolve($state, ActionType::MOVE, ['playerId' => 3, 'x' => 9, 'y' => 10]);
        $this->assertTrue($r->isTurnover(), 'fixtura: kolo AWAY končí turnoverem');

        $this->assertSame(PlayerState::PRONE, $this->konecKola($r->getNewState())->requirePlayer(2)->getState());
    }

    // === Omráčení při výkopu skutečnou cestou (review P186) ===
    //
    // Dřívější test si stav "omráčen při výkopu" nastavoval ručně. Tady jde hra celou
    // cestou END_SETUP → `KickoffResolver` → první kolo, protože právě na ní se rozhoduje,
    // jestli omráčený nese příznak "omráčen v tomto kole" (po výkopu se kolo přijímajícího
    // týmu nezahajuje přes `resetPlayersForNewTurn`, takže by mu příznak zůstal).
    // ř. 704-707: "All face-down players are turned face up at the end of their team's next
    // turn ... a player may not turn face up on the turn they are Stunned." Výkop není kolo
    // žádného týmu ⇒ "next turn" je hned první kolo týmu po výkopu.

    /**
     * Rozestavení (HOME přijímá, AWAY se rozestaví sám) a výkop s danými kostkami tabulky.
     *
     * @param list<int> $kostkyUdalosti kostky od hodu na tabulku výkopu dál
     */
    private function poVykopu(array $kostkyUdalosti): GameState
    {
        $builder = (new GameStateBuilder())->withPhase(\App\Enum\GamePhase::SETUP)->withActiveTeam(TeamSide::HOME);
        for ($i = 0; $i < 3; $i++) {
            $builder->addPlayer(TeamSide::HOME, 12, 5 + $i, id: $i + 1);
        }
        for ($i = 0; $i < 8; $i++) {
            $builder->addPlayer(TeamSide::HOME, 6, $i + 3, id: $i + 4);
        }
        for ($i = 0; $i < 11; $i++) {
            $builder->addOffPitchPlayer(TeamSide::AWAY, id: 100 + $i);
        }

        // Rozptyl kopu D8 = 1, D6 = 1; na konci rezerva na dopad míče (odskok / chycení).
        $dice = new FixedDiceRoller([1, 1, ...$kostkyUdalosti, 4, 4, 4, 4, 4, 4]);
        $r = (new ActionResolver($dice))->resolve($builder->build(), ActionType::END_SETUP, []);
        $state = $r->getNewState();
        $this->assertSame(\App\Enum\GamePhase::PLAY, $state->getPhase(), 'fixtura: výkop proběhl');
        $this->assertSame(TeamSide::HOME, $state->getActiveTeam(), 'fixtura: první kolo má přijímající HOME');

        return $state;
    }

    /** Id jediného omráčeného hráče strany (fixtura: přesně jeden). */
    private function omraceny(GameState $state, TeamSide $side): int
    {
        $ids = [];
        foreach ($state->getPlayersOnPitch($side) as $p) {
            if ($p->getState() === PlayerState::STUNNED) {
                $ids[] = $p->getId();
            }
        }
        $this->assertCount(1, $ids, "fixtura: právě jeden omráčený hráč {$side->value}");

        return $ids[0];
    }

    private function overOtoceniPoVykopu(GameState $poVykopu): void
    {
        $home = $this->omraceny($poVykopu, TeamSide::HOME);
        $away = $this->omraceny($poVykopu, TeamSide::AWAY);

        $poKoleHome = $this->konecKola($poVykopu);
        $this->assertSame(PlayerState::PRONE, $poKoleHome->requirePlayer($home)->getState(), 'přijímající: konec jeho prvního kola');
        $this->assertSame(PlayerState::STUNNED, $poKoleHome->requirePlayer($away)->getState(), 'kopající: jeho první kolo teprve začíná');
        $this->assertSame(PlayerState::PRONE, $this->konecKola($poKoleHome)->requirePlayer($away)->getState(), 'kopající: konec jeho prvního kola');
    }

    public function testOmracenyKamenemPriVykopuSeOtociNaKonciPrvnihoKolaSvehoTymu(): void
    {
        // Tabulka 5+6 = 11 Throw a Rock (ř. 1342-1350); trenéři 3:3 = kámen na oba týmy;
        // za každý tým: los hráče 1,1 a zranění 1+1 = omráčen.
        $this->overOtoceniPoVykopu($this->poVykopu([5, 6, 3, 3, 1, 1, 1, 1, 1, 1, 1, 1]));
    }

    public function testOmracenyPriVpaduFanouskuSeOtociNaKonciPrvnihoKolaSvehoTymu(): void
    {
        // Tabulka 6+6 = 12 Pitch Invasion (ř. 1351-1356): D6 za každého hráče na hřišti,
        // 6 = omráčen. První hráč každého týmu 6, ostatních deset 1.
        $jedenOmracen = [6, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1];
        $this->overOtoceniPoVykopu($this->poVykopu([6, 6, ...$jedenOmracen, ...$jedenOmracen]));
    }

    public function testNacteniStavuBezKliceStunnedThisTurn(): void
    {
        // Stav uložený před zavedením příznaku (08.10.2026) klíč `stunnedThisTurn` nemá.
        // Tehdy se omráčený otáčel už na začátku kola svého týmu, takže kdo je v uloženém
        // stavu omráčený, byl omráčen dřív než v právě běžícím kole svého týmu -- nebo
        // v kole soupeře. V obou případech se otáčí na konci nejbližšího konce kola svého
        // týmu (ř. 704-705); chybějící klíč proto znamená false.
        $data = $this->awayOmracenVKoleHome()->toArray();
        foreach ($data['players'] as $i => $hrac) {
            $this->assertArrayHasKey('stunnedThisTurn', $hrac, 'fixtura: nový stav klíč ukládá');
            unset($data['players'][$i]['stunnedThisTurn']);
        }
        $nacteny = GameState::fromArray($data);

        $this->assertSame(PlayerState::STUNNED, $nacteny->requirePlayer(2)->getState());
        $this->assertFalse($nacteny->requirePlayer(2)->isStunnedThisTurn());
        // Kolo HOME končí, AWAY odehraje své kolo a na jeho konci se hráč otočí.
        $poKoleAway = $this->konecKola($this->konecKola($nacteny));
        $this->assertSame(PlayerState::PRONE, $poKoleAway->requirePlayer(2)->getState());
    }
}
