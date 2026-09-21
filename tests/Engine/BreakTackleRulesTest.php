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
 * ⭐⭐ BREAK TACKLE PODLE PRAVIDEL (21.09.2026).
 *
 * `rules_bb2016.txt` r. 7988-7991: "The player **may** use his Strength instead
 * of his Agility when making a Dodge roll. For example, a player with Strength
 * 4 and Agility 2 would count as having an Agility of 4 when making a Dodge
 * roll. This skill may only be used **once per turn**."
 *
 * Do 21.09. engine bral silu VZDY (i kdyz byla nizsi nez obratnost) a bez
 * jakehokoli limitu na kolo.
 *
 * Fixtura: hrac na (5,5) mezi soupeři na (5,4) a (5,6), cesta na (7,5) ma dva
 * uhyby. Pro ST4/AG2 vychazeji cile 3+ a pak 4+ (druhy uz bez sily);
 * pred opravou to bylo 3+ a 2+.
 */
final class BreakTackleRulesTest extends TestCase
{
    private function stav(int $strength, int $agility, array $skills = [SkillName::BreakTackle]): GameState
    {
        $s = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, movement: 6, strength: $strength, agility: $agility, skills: $skills, id: 1)
            ->addPlayer(TeamSide::AWAY, 5, 4, id: 2)
            ->addPlayer(TeamSide::AWAY, 5, 6, id: 3)
            ->build();

        return $s->withTeamState(TeamSide::HOME, $s->getTeamState(TeamSide::HOME)->withRerolls(0));
    }

    private function hrac(GameState $s, int $id): MatchPlayerDTO
    {
        $p = $s->getPlayer($id);
        $this->assertNotNull($p, "hrac {$id} musi byt ve stavu");

        return $p;
    }

    /** @param list<\App\DTO\GameEvent> $events @return list<int> */
    private function cileUhybu(array $events): array
    {
        $out = [];
        foreach ($events as $e) {
            if ($e->getType() === 'dodge') { $out[] = (int) $e->getData()['target']; }
        }

        return $out;
    }

    public function testDruhyUhybVKoleUzSiluNebere(): void
    {
        // ST4/AG2: prvni uhyb cil 3+ (sila), druhy 4+ (obratnost).
        // Kostky 3 a 3: prvni projde, druhy ne => turnover.
        // S vadou byl druhy cil 2+ a trojka by prosla.
        $r = (new ActionResolver(new FixedDiceRoller([3, 3, 1, 1, 1, 1])))
            ->resolve($this->stav(strength: 4, agility: 2), ActionType::MOVE, ['playerId' => 1, 'x' => 7, 'y' => 5]);

        $this->assertSame([3, 4], $this->cileUhybu($r->getEvents()), 'r. 7991: sila jen na prvni uhyb v kole');
        $this->assertTrue($r->isTurnover());
    }

    public function testSilaSeNebere_kdyzJeNIZSInezObratnost(): void
    {
        // ST2/AG4. "May" + volba nejlepsiho vysledku ⇒ hrac si vezme obratnost.
        // Cil s AG4 je 3+, se silou 2 by byl 5+. Kostka 4 tedy vyjde jen spravne.
        $r = (new ActionResolver(new FixedDiceRoller([4, 6])))
            ->resolve($this->stav(strength: 2, agility: 4), ActionType::MOVE, ['playerId' => 1, 'x' => 6, 'y' => 4]);

        $this->assertSame([3], $this->cileUhybu($r->getEvents()), 'nizsi sila se brat nesmi');
        $this->assertTrue($r->isSuccess());
        $this->assertFalse($this->hrac($r->getNewState(), 1)->isBreakTackleUsedThisTurn(),
            'kdyz se sila nepouzila, skill se nespotreboval');
    }

    public function testPouzitiSeZapiseAObnoviSeNaZacatkuKola(): void
    {
        $r = (new ActionResolver(new FixedDiceRoller([3, 6])))
            ->resolve($this->stav(strength: 4, agility: 2), ActionType::MOVE, ['playerId' => 1, 'x' => 6, 'y' => 4]);

        $this->assertTrue($this->hrac($r->getNewState(), 1)->isBreakTackleUsedThisTurn());

        $noveKolo = $r->getNewState()->resetPlayersForNewTurn(TeamSide::HOME);
        $this->assertFalse($this->hrac($noveKolo, 1)->isBreakTackleUsedThisTurn());
    }

    public function testBezSkilluSePriznakNikdyNenastavi(): void
    {
        // Pozitivni kontrola: tataz fixtura bez Break Tackle -- cil se pocita
        // z obratnosti a priznak zustava prazdny.
        $r = (new ActionResolver(new FixedDiceRoller([6, 6])))
            ->resolve($this->stav(strength: 4, agility: 2, skills: []), ActionType::MOVE, ['playerId' => 1, 'x' => 6, 'y' => 4]);

        $this->assertSame([5], $this->cileUhybu($r->getEvents()), 'bez skillu se pocita z AG2');
        $this->assertFalse($this->hrac($r->getNewState(), 1)->isBreakTackleUsedThisTurn());
    }
}
