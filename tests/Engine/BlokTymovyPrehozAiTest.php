<?php

declare(strict_types=1);

namespace App\Tests\Engine;

use App\DTO\GameState;
use App\DTO\TeamStateDTO;
use App\Engine\ActionResolver;
use App\Engine\FixedDiceRoller;
use App\Enum\ActionType;
use App\Enum\PlayerState;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use PHPUnit\Framework\TestCase;

/**
 * P328 (uživatel 25.11.2026: „6 ano“) — AI na webu použije týmový přehoz na blok jako C++ `resolveBlock`: špatná kostka
 * (`shouldRerollBlock`) ⇒ Pro, po neúspěšném Pro týmový přehoz hodu Pro (`rules_bb2016.txt` ř. 8385-8387), bez Pro
 * týmový přehoz kostek; na soupeřova nosiče i výsledek, který ho nesrazí ani mu nevezme míč (C++ P249c). Dřív PHP
 * AI týmový přehoz na blok nebralo nikdy. Kostky: 1 lebka · 2 Both Down · 3-4 Pushed · 5 Stumbles · 6 Defender Down.
 * Útočník HOME (5,5) ST4 proti ST3 ⇒ dvě kostky, volí útočník.
 */
final class BlokTymovyPrehozAiTest extends TestCase
{
    /** @param list<SkillName> $utocnik */
    private function stav(array $utocnik = [], bool $obranceNese = false): GameState
    {
        $b = (new GameStateBuilder())
            ->withHomeTeam(TeamStateDTO::create(1, 'Home', 'Human', TeamSide::HOME, 3))
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 4, skills: $utocnik, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, id: 2);

        return ($obranceNese ? $b->withBallCarried(2) : $b->withBallOffPitch())->build();
    }

    /** @param list<int> $kostky */
    private function blok(GameState $s, array $kostky): GameState
    {
        return (new ActionResolver(new FixedDiceRoller($kostky)))->resolve($s, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2])->getNewState();
    }

    public function testDvojiLebkaPrehodiTymovymPrehozem(): void
    {
        $s = $this->blok($this->stav(), [1, 1, 6, 6, 1, 1]);   // lebky; přehoz 6,6 = Defender Down; brnění 2

        $this->assertSame(2, $s->getTeamState(TeamSide::HOME)->getRerolls(), 'týmový přehoz nevzat (dřív nikdy)');
        $this->assertSame(PlayerState::STANDING, $s->requirePlayer(1)->getState());
        $this->assertSame(PlayerState::PRONE, $s->requirePlayer(2)->getState());
    }

    /** Pozitivní kontrola: dobrá kostka se nepřehazuje. */
    public function testDobraKostkaBezPrehozu(): void
    {
        $s = $this->blok($this->stav(), [6, 1, 1, 1]);

        $this->assertSame(3, $s->getTeamState(TeamSide::HOME)->getRerolls());
        $this->assertSame(PlayerState::PRONE, $s->requirePlayer(2)->getState());
    }

    /** Pro 2 neuspěje ⇒ týmový přehoz přehodí HOD PRO (5), ten pustí nové kostky 6,6. */
    public function testPoNeuspesnemProPrehodiHodPro(): void
    {
        $s = $this->blok($this->stav([SkillName::Pro]), [1, 1, 2, 5, 6, 6, 1, 1]);

        $this->assertSame(2, $s->getTeamState(TeamSide::HOME)->getRerolls());
        $this->assertTrue($s->requirePlayer(1)->isProUsedThisTurn());
        $this->assertSame(PlayerState::STANDING, $s->requirePlayer(1)->getState());
        $this->assertSame(PlayerState::PRONE, $s->requirePlayer(2)->getState());
    }

    /** Pushed na soupeřova nosiče je špatný výsledek (P249c), mimo nosiče ne. */
    public function testPushedNaNosicePrehodi(): void
    {
        $naNosice = $this->blok($this->stav(obranceNese: true), [3, 3, 6, 6, 1, 1, 1, 1, 1, 1]);
        $this->assertSame(2, $naNosice->getTeamState(TeamSide::HOME)->getRerolls());
        $this->assertSame(PlayerState::PRONE, $naNosice->requirePlayer(2)->getState());

        $mimo = $this->blok($this->stav(), [3, 3, 1, 1, 1, 1]);
        $this->assertSame(3, $mimo->getTeamState(TeamSide::HOME)->getRerolls(), 'Pushed mimo nosiče se nepřehazuje');
    }
}
