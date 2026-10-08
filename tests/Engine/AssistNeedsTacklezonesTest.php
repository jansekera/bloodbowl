<?php

declare(strict_types=1);

namespace App\Tests\Engine;

use App\DTO\GameState;
use App\Engine\ActionResolver;
use App\Engine\FixedDiceRoller;
use App\Engine\StrengthCalculator;
use App\Enum\ActionType;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use App\ValueObject\Position;
use PHPUnit\Framework\TestCase;

/**
 * HRÁČ BEZ TACKLE ZÓN NEASISTUJE A CIZÍ ASISTENCI NERUŠÍ (audit parity 08.10.2026, nález 9).
 *
 * `rules_bb2016.txt` ř. 1662-1669 (Assisting a Block): "In order to make an assist, the
 * player: 1. Must be adjacent to the enemy player involved in the block, and... 2. Must not
 * be in the tackle zone of any other player from the opposing team, and ... 3. Must be
 * standing, and … 4. **Must have his tackle zones.**"
 * Zóny bere Bone-head (ř. 7983-7985: "loses his tackle zones and may not ... assist another
 * player on a block or foul"), Really Stupid (ř. 8401-8403) a Hypnotic Gaze (ř. 8185-8187).
 * Faul: ř. 1849-1851 "No player from either side may assist a foul if they are in the tackle
 * zone of an opposing player, do not have their tackle zones, or are not standing."
 *
 * Stará mechanika: asistent se u bloku zkoušel jen na "stojí"; a hráč soupeře bez zón dál
 * "stál v cestě" (počítal se jako tackle zóna, která asistenci ruší) -- u bloku i u faulu.
 */
final class AssistNeedsTacklezonesTest extends TestCase
{
    private function bezZon(GameState $state, int $id): GameState
    {
        return $state->withPlayer($state->requirePlayer($id)->withLostTacklezones(true));
    }

    /** Útočník 1 (5,7) blokuje obránce 2 (6,7); spoluhráč 3 (7,7) sousedí s obráncem. */
    private function sAsistentem(): GameStateBuilder
    {
        return (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 7, id: 2)
            ->addPlayer(TeamSide::HOME, 7, 7, id: 3);
    }

    private function asistence(GameState $state, int $blokujici, int $cil): int
    {
        return (new StrengthCalculator())->countAssists(
            $state,
            $state->requirePlayer($blokujici),
            $state->requirePlayer($cil)->requirePosition(),
        );
    }

    public function testKontrolaAsistentSeZonamiAsistuje(): void
    {
        $this->assertSame(1, $this->asistence($this->sAsistentem()->build(), 1, 2));
    }

    public function testUtocnyAsistentBezZonNeasistuje(): void
    {
        $state = $this->bezZon($this->sAsistentem()->build(), 3);

        $this->assertSame(0, $this->asistence($state, 1, 2), 'ř. 1669: "Must have his tackle zones"');
    }

    public function testObrannyAsistentBezZonNeasistuje(): void
    {
        // Obránci 2 pomáhá spoluhráč 4 na (5,8), sousedí s útočníkem.
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 7, id: 2)
            ->addPlayer(TeamSide::AWAY, 5, 8, id: 4)
            ->build();
        $this->assertSame(1, $this->asistence($state, 2, 1), 'fixtura: se zónami asistuje');

        $this->assertSame(0, $this->asistence($this->bezZon($state, 4), 2, 1), 'ř. 1669');
    }

    public function testGuardBezZonNeasistuje(): void
    {
        // Guard promíjí jen podmínku 2 (cizí zóna, ř. 8159-8160), ne podmínku 4.
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 7, id: 2)
            ->addPlayer(TeamSide::HOME, 7, 7, skills: [SkillName::Guard], id: 3)
            ->build();

        $this->assertSame(0, $this->asistence($this->bezZon($state, 3), 1, 2), 'ř. 1669');
    }

    public function testSouperBezZonAsistenciNerusi(): void
    {
        // Asistenta 3 (7,7) "hlídá" soupeř 5 na (8,7). Se zónami asistenci ruší (ř. 1666-1667),
        //   bez zón v ničí zóně asistent nestojí.
        $state = $this->sAsistentem()->addPlayer(TeamSide::AWAY, 8, 7, id: 5)->build();
        $this->assertSame(0, $this->asistence($state, 1, 2), 'fixtura: soupeř se zónami asistenci ruší');

        $this->assertSame(1, $this->asistence($this->bezZon($state, 5), 1, 2), 'ř. 1666-1667: zóna, která není, neruší');
    }

    public function testBlokPocitaKostkyBezAsistentaBezZon(): void
    {
        // Totéž přes celý blok: 3 proti 3 a asistent bez zón ⇒ 1 kostka (stará mechanika: 2).
        $state = $this->bezZon($this->sAsistentem()->build(), 3);

        $r = (new ActionResolver(new FixedDiceRoller([3, 3])))
            ->resolve($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);
        $rany = array_values(array_filter($r->getEvents(), fn($e) => $e->getType() === 'block'));

        $this->assertSame(1, $rany[0]->getData()['diceCount']);
    }

    public function testUFauluSouperBezZonAsistenciNerusi(): void
    {
        // Faulující 1 (5,7), oběť 2 leží na (6,7), pomocník 3 (7,7), u něj soupeř 5 (8,7).
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1)
            ->addPronePlayer(TeamSide::AWAY, 6, 7, id: 2)
            ->addPlayer(TeamSide::HOME, 7, 7, id: 3)
            ->addPlayer(TeamSide::AWAY, 8, 7, id: 5)
            ->build();
        $calc = new StrengthCalculator();
        $this->assertSame(0, $calc->countFoulAssists($state, $state->requirePlayer(1), $state->requirePlayer(2)), 'fixtura');

        $state = $this->bezZon($state, 5);

        $this->assertSame(
            1,
            $calc->countFoulAssists($state, $state->requirePlayer(1), $state->requirePlayer(2)),
            'ř. 1849-1851: pomocník nestojí v zóně soupeře, který zóny nemá',
        );
    }
}
