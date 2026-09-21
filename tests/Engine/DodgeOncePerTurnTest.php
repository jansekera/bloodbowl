<?php

declare(strict_types=1);

namespace App\Tests\Engine;

use App\DTO\GameState;
use App\DTO\MatchPlayerDTO;
use App\Engine\ActionResolver;
use App\Engine\FixedDiceRoller;
use App\Enum\ActionType;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use PHPUnit\Framework\TestCase;

/**
 * ⭐⭐ DODGE JE JEN 1x ZA KOLO (21.09.2026).
 *
 * `rules_bb2016.txt` r. 960-962: "However, the player may only re-roll one
 * failed Dodge roll per turn. So, if the player kept on moving and failed a
 * second Dodge roll, he could not use the skill again."
 * Totez r. 8089-8090 v popisu skillu.
 *
 * DVE FIXTURY, obe s hracem na (5,5), ktery ma Dodge, a bez tymovych prehozu:
 *   - `stav()`         -- soupere na (5,4) a (5,6); cesta na (7,5) ma PRAVE DVA uhyby (cile 4+, 3+)
 *   - `stavTriUhyby()` -- soupere na (4,4), (6,4), (4,6) a (6,6); cesta na (8,5) ma PRAVE TRI uhyby
 *                         (cile 5+, 4+, 3+), takze jde overit i to, co se deje u tretiho
 * Kostky jsou volene NA HRANE: kazdy test dopadne jinak s vadou a bez ni.
 */
final class DodgeOncePerTurnTest extends TestCase
{
    private function stav(int $rerolls = 0): GameState
    {
        $s = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, movement: 6, skills: [SkillName::Dodge], id: 1)
            ->addPlayer(TeamSide::AWAY, 5, 4, id: 2)
            ->addPlayer(TeamSide::AWAY, 5, 6, id: 3)
            ->build();

        return $s->withTeamState(TeamSide::HOME, $s->getTeamState(TeamSide::HOME)->withRerolls($rerolls));
    }

    private function stavTriUhyby(int $rerolls = 0): GameState
    {
        $s = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, movement: 6, skills: [SkillName::Dodge], id: 1)
            ->addPlayer(TeamSide::AWAY, 4, 4, id: 2)
            ->addPlayer(TeamSide::AWAY, 6, 4, id: 3)
            ->addPlayer(TeamSide::AWAY, 4, 6, id: 4)
            ->addPlayer(TeamSide::AWAY, 6, 6, id: 5)
            ->build();

        return $s->withTeamState(TeamSide::HOME, $s->getTeamState(TeamSide::HOME)->withRerolls($rerolls));
    }

    /** @param list<\App\DTO\GameEvent> $events */
    private function pocetDodgePrehozu(array $events): int
    {
        $n = 0;
        foreach ($events as $e) {
            if ($e->getType() === 'reroll' && ($e->getData()['source'] ?? '') === 'Dodge') { $n++; }
        }

        return $n;
    }

    /** @param list<\App\DTO\GameEvent> $events @return list<bool> */
    private function vysledkyUhybu(array $events): array
    {
        $out = [];
        foreach ($events as $e) {
            if ($e->getType() === 'dodge') { $out[] = (bool) ($e->getData()['success'] ?? false); }
        }

        return $out;
    }

    private function hrac(GameState $s, int $id): MatchPlayerDTO
    {
        $p = $s->getPlayer($id);
        $this->assertNotNull($p, "hrac {$id} musi byt ve stavu");

        return $p;
    }

    public function testDruhyUhybUzDodgeNeprehodi(): void
    {
        // uhyb 1 (cil 4+): 3 = neuspech -> Dodge prehoz 5 = uspech.
        // uhyb 2 (cil 3+): 2 = neuspech -> Dodge uz byl 1x pouzity => pad a turnover.
        // S vadou: druhy prehoz 6 = uspech a zadny turnover.
        $r = (new ActionResolver(new FixedDiceRoller([3, 5, 2, 6, 1, 1, 1, 1])))
            ->resolve($this->stav(), ActionType::MOVE, ['playerId' => 1, 'x' => 7, 'y' => 5]);

        $this->assertTrue($r->isTurnover(), 'r. 960-962: Dodge prehoz jen na JEDEN neuspesny uhyb za kolo');
    }

    public function testPouzitiDodgePrehozuSeZapiseAObnoviSeNaZacatkuKola(): void
    {
        // Jediny krok na (6,4): uhyb 3 (cil 4+) neuspech -> Dodge prehoz 5 uspech.
        $r = (new ActionResolver(new FixedDiceRoller([3, 5])))
            ->resolve($this->stav(), ActionType::MOVE, ['playerId' => 1, 'x' => 6, 'y' => 4]);

        $this->assertTrue($r->isSuccess(), 'fixtura: Dodge prehoz zachranil prvni uhyb');
        $this->assertTrue($this->hrac($r->getNewState(), 1)->isDodgeUsedThisTurn());

        $noveKolo = $r->getNewState()->resetPlayersForNewTurn(TeamSide::HOME);
        $this->assertFalse($this->hrac($noveKolo, 1)->isDodgeUsedThisTurn(), 'na zacatku kola se Dodge obnovi');
    }

    public function testNeuspesnyUhybBezDodgePrehozuNicNezapisuje(): void
    {
        // Pozitivni kontrola k testu vyse: uhyb 1 vyjde napoprve (5 >= 4),
        // takze se Dodge NEPOUZIJE a priznak musi zustat prazdny.
        $r = (new ActionResolver(new FixedDiceRoller([5])))
            ->resolve($this->stav(), ActionType::MOVE, ['playerId' => 1, 'x' => 6, 'y' => 4]);

        $this->assertTrue($r->isSuccess(), 'fixtura: uhyb vysel napoprve');
        $this->assertFalse($this->hrac($r->getNewState(), 1)->isDodgeUsedThisTurn(), 'nepouzity Dodge se nezapisuje');
    }

    public function testPoVycerpanemDodgiSeDruhyUhybSmiPrehoditTymovymPrehozem(): void
    {
        // Pozitivni kontrola: omezeni plati na skill, ne na ostatni prehozy.
        // uhyb 1: 3 -> Dodge prehoz 5. uhyb 2: 2 -> tymovy prehoz 6 = uspech.
        $r = (new ActionResolver(new FixedDiceRoller([3, 5, 2, 6])))
            ->resolve($this->stav(rerolls: 1), ActionType::MOVE, ['playerId' => 1, 'x' => 7, 'y' => 5]);

        $this->assertFalse($r->isTurnover(), 'r. 960-962 omezuje jen skill, tymovy prehoz zustava');
        $this->assertSame(0, $r->getNewState()->getTeamState(TeamSide::HOME)->getRerolls());
    }

    // ================= TRI UHYBY V JEDNE AKCI (cile 5+, 4+, 3+) =================

    public function testPrehozPadneNaPRVNIUhybAZbytekProjdeBezNej(): void
    {
        // uhyb1 4 (cil 5+) neuspech -> Dodge prehoz 6 uspech; uhyb2 5 (cil 4+) OK; uhyb3 4 (cil 3+) OK.
        $r = (new ActionResolver(new FixedDiceRoller([4, 6, 5, 4])))
            ->resolve($this->stavTriUhyby(), ActionType::MOVE, ['playerId' => 1, 'x' => 8, 'y' => 5]);

        $this->assertTrue($r->isSuccess());
        $this->assertFalse($r->isTurnover());
        $this->assertSame(1, $this->pocetDodgePrehozu($r->getEvents()), 'Dodge se pouzil prave jednou');
        $this->assertSame([false, true, true, true], $this->vysledkyUhybu($r->getEvents()));
        $this->assertTrue($this->hrac($r->getNewState(), 1)->isDodgeUsedThisTurn());
    }

    public function testPrehozPadneAZNaDRUHYUhyb(): void
    {
        // uhyb1 5 (cil 5+) OK -- Dodge se nespotrebuje; uhyb2 3 (cil 4+) neuspech -> prehoz 6 OK;
        // uhyb3 3 (cil 3+) OK. Limit se tedy neváže na PRVNI uhyb, ale na prvni NEUSPESNY.
        $r = (new ActionResolver(new FixedDiceRoller([5, 3, 6, 3])))
            ->resolve($this->stavTriUhyby(), ActionType::MOVE, ['playerId' => 1, 'x' => 8, 'y' => 5]);

        $this->assertTrue($r->isSuccess());
        $this->assertFalse($r->isTurnover());
        $this->assertSame(1, $this->pocetDodgePrehozu($r->getEvents()));
        $this->assertSame([true, false, true, true], $this->vysledkyUhybu($r->getEvents()));
        $this->assertTrue($this->hrac($r->getNewState(), 1)->isDodgeUsedThisTurn());
    }

    public function testTriUhybyBezJedinehoPrehozu(): void
    {
        // 5 (cil 5+), 4 (cil 4+), 3 (cil 3+) -- vsechny vyjdou napoprve.
        $r = (new ActionResolver(new FixedDiceRoller([5, 4, 3])))
            ->resolve($this->stavTriUhyby(), ActionType::MOVE, ['playerId' => 1, 'x' => 8, 'y' => 5]);

        $this->assertTrue($r->isSuccess());
        $this->assertSame(0, $this->pocetDodgePrehozu($r->getEvents()), 'nebylo co prehazovat');
        $this->assertSame([true, true, true], $this->vysledkyUhybu($r->getEvents()));
        $this->assertFalse($this->hrac($r->getNewState(), 1)->isDodgeUsedThisTurn(), 'nepouzity Dodge se nezapisuje');
    }

    public function testNeuspesnyPrehozPrvnihoUhybuKonciTurnoverem(): void
    {
        // uhyb1 4 (cil 5+) neuspech -> Dodge prehoz 2 taky neuspech => pad, turnover.
        // Zbyle kostky jsou hod na brneni po padu.
        $r = (new ActionResolver(new FixedDiceRoller([4, 2, 1, 1, 1, 1])))
            ->resolve($this->stavTriUhyby(), ActionType::MOVE, ['playerId' => 1, 'x' => 8, 'y' => 5]);

        $this->assertTrue($r->isTurnover(), 'neuspesny uhyb i po prehozu = pad a turnover');
        $this->assertSame(1, $this->pocetDodgePrehozu($r->getEvents()));
        $this->assertSame([false, false], $this->vysledkyUhybu($r->getEvents()), 'dal se neuhybalo');
        $this->assertTrue($this->hrac($r->getNewState(), 1)->isDodgeUsedThisTurn(), 'spotrebovany i kdyz nepomohl');
    }

    public function testTRETIUhybUzDodgeNeprehodi(): void
    {
        // uhyb1 5 (cil 5+) OK; uhyb2 3 (cil 4+) neuspech -> Dodge prehoz 6 OK (Dodge spotrebovan);
        // uhyb3 2 (cil 3+) neuspech -> uz zadny prehoz => turnover.
        // S vadou: dalsi kostka 6 by uhyb3 zachranila a akce by dosla do cile.
        $r = (new ActionResolver(new FixedDiceRoller([5, 3, 6, 2, 6, 1, 1, 1])))
            ->resolve($this->stavTriUhyby(), ActionType::MOVE, ['playerId' => 1, 'x' => 8, 'y' => 5]);

        $this->assertTrue($r->isTurnover(), 'r. 960-962: na treti uhyb uz Dodge neni');
        $this->assertSame(1, $this->pocetDodgePrehozu($r->getEvents()), 'prehoz byl jen jeden');
        $this->assertSame([true, false, true, false], $this->vysledkyUhybu($r->getEvents()));
    }
}
