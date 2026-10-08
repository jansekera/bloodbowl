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

    public function testOmracenyPriVykopuSeOtociNaKonciPrvnihoKolaSvehoTymu(): void
    {
        // Throw a Rock / Pitch Invasion omráčí MIMO kolo kteréhokoli týmu; "their team's next
        // turn" (ř. 705) je pak hned první kolo po výkopu.
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, id: 1)
            ->addPlayer(TeamSide::AWAY, 15, 5, id: 2)
            ->build();
        $state = $state
            ->withPlayer($state->requirePlayer(1)->withState(PlayerState::STUNNED))
            ->withPlayer($state->requirePlayer(2)->withState(PlayerState::STUNNED));

        $poKoleHome = $this->konecKola($state);
        $this->assertSame(PlayerState::PRONE, $poKoleHome->requirePlayer(1)->getState(), 'přijímající: konec jeho prvního kola');
        $this->assertSame(PlayerState::STUNNED, $poKoleHome->requirePlayer(2)->getState(), 'kopající: jeho kolo teprve začíná');
        $this->assertSame(PlayerState::PRONE, $this->konecKola($poKoleHome)->requirePlayer(2)->getState());
    }
}
