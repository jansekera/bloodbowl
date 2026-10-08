<?php

declare(strict_types=1);

namespace App\Tests\Engine;

use App\DTO\GameState;
use App\Engine\ActionResolver;
use App\Engine\FixedDiceRoller;
use App\Engine\RulesEngine;
use App\Enum\ActionType;
use App\Enum\PlayerState;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use PHPUnit\Framework\TestCase;

/**
 * KDO SE POHNUL, NEBLOKUJE (audit parity 08.10.2026, nález 2).
 *
 * `rules_bb2016.txt` ř. 674-676: "Note that a player who stands up may not take a Block
 * Action, because **you may not move when you take a Block Action**. The player may take
 * any Action other than a Block Action."
 * ř. 347-352: "Blitz: The player may move ... He may make one block during the move. ...
 * IMPORTANT: This Action may not be declared by more than one player per turn."
 *
 * Stará mechanika: validace BLOCK hlídala jen `canAct()` (stojí a `!hasActed`), MOVE nastaví
 * jen `hasMoved` ⇒ tentýž hráč udělal MOVE a potom BLOCK, týmový blitz zůstal nepoužitý,
 * tedy neomezený počet „blitzů" za kolo.
 */
final class NoBlockAfterMoveTest extends TestCase
{
    /** HOME id 1 na (5,5), AWAY id 2 na (8,5): po kroku na (7,5) spolu sousedí. Bez zón cestou ⇒ žádné kostky. */
    private function stavPoPohybu(): GameState
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, id: 1)
            ->addPlayer(TeamSide::AWAY, 8, 5, id: 2)
            ->addPlayer(TeamSide::HOME, 8, 6, id: 3)
            ->build();

        $r = (new ActionResolver(new FixedDiceRoller([])))
            ->resolve($state, ActionType::MOVE, ['playerId' => 1, 'x' => 7, 'y' => 5]);
        $this->assertTrue($r->isSuccess(), 'fixtura: pohyb bez hodu');
        $this->assertTrue($r->getNewState()->requirePlayer(1)->hasMoved(), 'fixtura: hráč se pohnul');

        return $r->getNewState();
    }

    /** @return list<int> id hráčů, kterým se nabízí akce daného typu */
    private function nabidka(GameState $state, ActionType $type): array
    {
        $ids = [];
        foreach ((new RulesEngine())->getAvailableActions($state) as $a) {
            if ($a['type'] === $type->value && isset($a['playerId'])) {
                $ids[] = $a['playerId'];
            }
        }

        return $ids;
    }

    public function testBlokPoPohybuValidaceOdmitne(): void
    {
        $errors = (new RulesEngine())->validate(
            $this->stavPoPohybu(),
            ActionType::BLOCK,
            ['playerId' => 1, 'targetId' => 2],
        );

        $this->assertNotSame([], $errors, 'ř. 675: "you may not move when you take a Block Action"');
    }

    public function testBlokSePoPohybuNenabizi(): void
    {
        $state = $this->stavPoPohybu();

        $this->assertNotContains(1, $this->nabidka($state, ActionType::BLOCK), 'ř. 675: po pohybu se Block nenabízí');
        // Pozitivní kontrola: hráč, který se nehnul a sousedí s týmž soupeřem, blokovat smí.
        $this->assertContains(3, $this->nabidka($state, ActionType::BLOCK));
        $this->assertSame([], (new RulesEngine())->validate($state, ActionType::BLOCK, ['playerId' => 3, 'targetId' => 2]));
    }

    public function testPohybARanaJeBlitzATenJdeIPoPohybu(): void
    {
        // "Smí jen X" není "musí X": zakázaný je Block, ne rána jako taková. Pohyb + rána je
        // Blitz (ř. 347-350) -- ten po pohybu zůstává k dispozici a spotřebuje týmový blitz.
        $state = $this->stavPoPohybu();

        $this->assertContains(1, $this->nabidka($state, ActionType::BLITZ));
        $this->assertSame([], (new RulesEngine())->validate($state, ActionType::BLITZ, ['playerId' => 1, 'targetId' => 2]));
    }

    public function testHracKteryVstalNeblokuje(): void
    {
        // ř. 674-675: "a player who stands up may not take a Block Action". HOME id 1 leží
        // vedle soupeře; vstane a udělá krok na (5,6) (pořád vedle soupeře na (6,5)).
        // Úhyb ze zóny soupeře: 6 = úspěch.
        $state = (new GameStateBuilder())
            ->addPronePlayer(TeamSide::HOME, 5, 5, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, id: 2)
            ->build();
        $r = (new ActionResolver(new FixedDiceRoller([6])))
            ->resolve($state, ActionType::MOVE, ['playerId' => 1, 'x' => 5, 'y' => 6]);
        $this->assertSame(PlayerState::STANDING, $r->getNewState()->requirePlayer(1)->getState(), 'fixtura: hráč vstal');

        $this->assertNotSame(
            [],
            (new RulesEngine())->validate($r->getNewState(), ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]),
        );
        $this->assertNotContains(1, $this->nabidka($r->getNewState(), ActionType::BLOCK));
    }

    public function testMultipleBlockPoPohybuValidaceOdmitne(): void
    {
        // Multiple Block je Block Action (ř. 675 platí stejně).
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, skills: [SkillName::MultipleBlock], id: 1)
            ->addPlayer(TeamSide::AWAY, 8, 4, id: 2)
            ->addPlayer(TeamSide::AWAY, 8, 6, id: 3)
            ->build();
        // Krok na (7,5) vstupuje do zón, ale žádnou neopouští ⇒ bez úhybu.
        $r = (new ActionResolver(new FixedDiceRoller([])))
            ->resolve($state, ActionType::MOVE, ['playerId' => 1, 'x' => 7, 'y' => 5]);
        $this->assertTrue($r->isSuccess());

        $this->assertNotSame(
            [],
            (new RulesEngine())->validate(
                $r->getNewState(),
                ActionType::MULTIPLE_BLOCK,
                ['playerId' => 1, 'targetId' => 2, 'targetId2' => 3],
            ),
        );
        $this->assertNotContains(1, $this->nabidka($r->getNewState(), ActionType::MULTIPLE_BLOCK));
    }
}
