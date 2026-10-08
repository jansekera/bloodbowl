<?php

declare(strict_types=1);

namespace App\Tests\Engine;

use App\DTO\ActionResult;
use App\DTO\GameState;
use App\Engine\ActionResolver;
use App\Engine\FixedDiceRoller;
use App\Enum\ActionType;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use App\ValueObject\Position;
use PHPUnit\Framework\TestCase;

/**
 * SIDE STEP: KTERÉKOLI SOUSEDNÍ VOLNÉ POLE (audit parity 08.10.2026, nález 11).
 *
 * `rules_bb2016.txt` ř. 8472-8480: "his coach may choose which square the player is moved to
 * when he is pushed back, rather than the opposing coach. Furthermore, the coach **may
 * choose to move the player to any adjacent square, not just the three squares shown on the
 * Push Back diagram**. Note that the player **may not use this skill if there are no open
 * squares on the pitch adjacent to this player**."
 * Bez skillu platí běžné pořadí: ř. 639 volné pole, ř. 641-644 řetěz jen když jsou všechna
 * obsazená, ř. 650-651 do davu, když na hřišti není volné pole odtlačení.
 * Grab ř. 8151-8153: "Grab and Side Step will cancel each other out and the standard
 * pushback rules apply."
 *
 * Stará mechanika: Side Step vybíral jen ze tří polí odtlačení; a když byla všechna tři
 * obsazená (nebo mimo hřiště), šel rovnou do řetězu -- i tam, kde podle běžného pořadí
 * patří hráč do davu.
 */
final class SideStepAnyAdjacentSquareTest extends TestCase
{
    private const POLE_ODTLACENI = [[7, 6], [7, 7], [7, 8]];

    /** @param list<int> $kostky */
    private function blok(GameState $state, array $kostky): ActionResult
    {
        return (new ActionResolver(new FixedDiceRoller($kostky)))
            ->resolve($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);
    }

    /** @return list<string> */
    private function typy(ActionResult $r): array
    {
        return array_map(fn($e) => $e->getType(), $r->getEvents());
    }

    /** @return array{0: int, 1: int} */
    private function pole(ActionResult $r, int $id): array
    {
        $p = $r->getNewState()->requirePlayer($id)->requirePosition();

        return [$p->getX(), $p->getY()];
    }

    public function testKdyzJsouTriPoleOdtlaceniObsazenaUhneNaJineSousedniVolnePole(): void
    {
        // Útočník (5,7) → obránce se Side Step (6,7). Všechna tři pole odtlačení drží hráči
        //   útočníka; volná jsou pole vedle a za útočníkem. Stará mechanika: řetěz.
        // Kostky: 3 asistence ⇒ 6 proti 3 = 2 kostky, obě 3 = Pushed.
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 7, skills: [SkillName::SideStep], id: 2)
            ->addPlayer(TeamSide::HOME, 7, 6, id: 3)
            ->addPlayer(TeamSide::HOME, 7, 7, id: 4)
            ->addPlayer(TeamSide::HOME, 7, 8, id: 5)
            ->build();

        $r = $this->blok($state, [3, 3]);

        $this->assertNotContains('chain_push', $this->typy($r), 'ř. 8474-8476: volné sousední pole existuje');
        $kam = $this->pole($r, 2);
        $this->assertNotContains($kam, self::POLE_ODTLACENI);
        $this->assertSame(1, (new Position(6, 7))->distanceTo(new Position(...$kam)), 'sousední pole');
        $this->assertSame([7, 6], $this->pole($r, 3), 'nikdo další se nehnul');
        $this->assertSame([7, 7], $this->pole($r, 4));
        $this->assertSame([7, 8], $this->pole($r, 5));
    }

    public function testVyberePoleMimoTriPoleOdtlaceniKdyzJeBezpecnejsi(): void
    {
        // Tři pole odtlačení jsou volná, ale v zónách tří hráčů útočníka (2, 3, 2 zóny);
        //   pole vedle obránce má jedinou zónu (útočníkovu). Stará mechanika: x = 7.
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 7, skills: [SkillName::SideStep], id: 2)
            ->addPlayer(TeamSide::HOME, 8, 6, id: 3)
            ->addPlayer(TeamSide::HOME, 8, 7, id: 4)
            ->addPlayer(TeamSide::HOME, 8, 8, id: 5)
            ->build();

        $r = $this->blok($state, [3]);

        $this->assertContains($this->pole($r, 2), [[6, 6], [6, 8], [5, 6], [5, 8]], 'ř. 8474-8476: "any adjacent square"');
    }

    public function testBezVolnehoSousednihoPoleSkillNeplatiAHracJdeDoDavu(): void
    {
        // Útočník (4,1) → obránce se Side Step (5,0), šikmo k lajně: pole odtlačení (6,-1)
        //   a (5,-1) jsou mimo hřiště, (6,0) obsazené. Obsazená jsou i všechna ostatní
        //   sousední pole ⇒ ř. 8476-8478 skill neplatí ⇒ běžné pořadí: na hřišti není volné
        //   pole odtlačení ⇒ dav (ř. 650-651). Stará mechanika: řetěz přes (6,0).
        // Kostky: obránci asistují (4,0) a (5,1) ⇒ 3 proti 5 = 2 kostky (3, 3 = Pushed),
        //   pak zranění od davu 3+3.
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 4, 1, id: 1)
            ->addPlayer(TeamSide::AWAY, 5, 0, skills: [SkillName::SideStep], id: 2)
            ->addPlayer(TeamSide::AWAY, 6, 0, id: 3)
            ->addPlayer(TeamSide::AWAY, 4, 0, id: 4)
            ->addPlayer(TeamSide::AWAY, 5, 1, id: 5)
            ->addPlayer(TeamSide::AWAY, 6, 1, id: 6)
            ->build();

        $r = $this->blok($state, [3, 3, 3, 3]);

        $this->assertContains('crowd_surf', $this->typy($r), 'ř. 650-651');
        $this->assertNotContains('chain_push', $this->typy($r));
        $this->assertNull($r->getNewState()->requirePlayer(2)->getPosition());
        $this->assertSame([6, 0], $this->pole($r, 3), 'řetěz se nekonal');
    }

    public function testGrabASideStepSeRusiAPlatiBeznePoradi(): void
    {
        // Hlídka (prošla i před opravou -- Grab měl v PHP přednost a vybírá stejně jako běžné
        //   odtlačení): širší výběr Side Step nesmí platit proti Grabu. Rozestavení jako
        //   výše; běžné odtlačení volí tým na tahu = pole s nejvíc zónami (7,7).
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, skills: [SkillName::Grab], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 7, skills: [SkillName::SideStep], id: 2)
            ->addPlayer(TeamSide::HOME, 8, 6, id: 3)
            ->addPlayer(TeamSide::HOME, 8, 7, id: 4)
            ->addPlayer(TeamSide::HOME, 8, 8, id: 5)
            ->build();

        $r = $this->blok($state, [3]);

        $this->assertSame([7, 7], $this->pole($r, 2), 'ř. 8151-8153');
    }
}
