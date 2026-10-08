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

    // === Review P186, H1: pohyb, který skončil úspěšným PŘEHOZEM v dialogu ===
    //
    // Dialog přehozu hráče po úspěchu přesune a pohyb NEUKONČÍ (člověk smí táhnout dál),
    // takže "pohyb ukončen" (`hasMoved`) zůstává false. Zákaz bloku ale platí od prvního
    // pole: ř. 675 "you may not move when you take a Block Action".

    /**
     * Pohyb v režimu s dialogem: první kostka padne 1 (neúspěch ⇒ dialog), týmový přehoz 6.
     *
     * @param array{x: int, y: int} $cil
     */
    private function pohybPrehozenyVDialogu(GameState $state, array $cil, string $typHodu): GameState
    {
        $resolver = new ActionResolver(new FixedDiceRoller([1, 6]));
        $resolver->setInteractiveRerolls(true);
        $r = $resolver->resolve($state, ActionType::MOVE, ['playerId' => 1] + $cil);
        $this->assertSame($typHodu, $r->getNewState()->getPendingReroll()?->getRollType(), 'fixtura: čeká dialog přehozu');

        $r = $resolver->resolve($r->getNewState(), ActionType::RESOLVE_REROLL, ['choice' => 'team_reroll']);
        $this->assertTrue($r->isSuccess(), 'fixtura: přehoz uspěl');
        $hrac = $r->getNewState()->requirePlayer(1);
        $this->assertSame([$cil['x'], $cil['y']], [$hrac->requirePosition()->getX(), $hrac->requirePosition()->getY()], 'fixtura: hráč se přesunul');

        return $r->getNewState();
    }

    public function testBlokPoUhybuPrehozenemVDialoguValidaceOdmitne(): void
    {
        // HOME 1 na (5,5) v zóně AWAY 4 na (4,5); úhyb na (6,5), kde sousedí s AWAY 2 na (7,5).
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, id: 1)
            ->addPlayer(TeamSide::AWAY, 7, 5, id: 2)
            ->addPlayer(TeamSide::AWAY, 4, 5, id: 4)
            ->build();
        $state = $this->pohybPrehozenyVDialogu($state, ['x' => 6, 'y' => 5], 'dodge');

        $this->assertNotSame(
            [],
            (new RulesEngine())->validate($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]),
            'ř. 675: "you may not move when you take a Block Action" -- i když pohyb prošel přehozem',
        );
        $this->assertNotContains(1, $this->nabidka($state, ActionType::BLOCK));
        // Rána po pohybu je Blitz (ř. 347-350) a ten k dispozici zůstává.
        $this->assertSame([], (new RulesEngine())->validate($state, ActionType::BLITZ, ['playerId' => 1, 'targetId' => 2]));
    }

    public function testBlokPoGfiPrehozenemVDialoguValidaceOdmitne(): void
    {
        // HOME 1 s MA 1 na (5,5): druhé pole (7,5) je Going For It; tam sousedí s AWAY 2 na (8,5).
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, movement: 1, id: 1)
            ->addPlayer(TeamSide::AWAY, 8, 5, id: 2)
            ->build();
        $state = $this->pohybPrehozenyVDialogu($state, ['x' => 7, 'y' => 5], 'gfi');

        $this->assertNotSame(
            [],
            (new RulesEngine())->validate($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]),
            'ř. 675: po pohybu (i s přehozeným GFI) Block nejde',
        );
        $this->assertNotContains(1, $this->nabidka($state, ActionType::BLOCK));
        $this->assertSame([], (new RulesEngine())->validate($state, ActionType::BLITZ, ['playerId' => 1, 'targetId' => 2]));
    }

    public function testVPristimKoleSmiHracKteryseHnulZaseBlokovat(): void
    {
        // Zákaz platí pro kolo, ve kterém se hráč pohnul -- příští kolo je nová akce.
        $state = $this->stavPoPohybu()->resetPlayersForNewTurn(TeamSide::HOME);

        $this->assertSame([], (new RulesEngine())->validate($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]));
    }

    public function testStavUlozenyPredZavedenimPriznakuZakazBlokuDrzi(): void
    {
        // Starší uložený stav klíč `movedThisTurn` nemá. Tehdy zákaz stál na `hasMoved`,
        // takže hráč s ukončeným pohybem nesmí po načtení blokovat (ř. 675).
        $data = $this->stavPoPohybu()->requirePlayer(1)->toArray();
        unset($data['movedThisTurn']);
        $this->assertFalse(\App\DTO\MatchPlayerDTO::fromArray($data)->canTakeBlockAction());

        // Pozitivní kontrola: hráč, který se nehnul, po načtení blokovat smí.
        $data = $this->stavPoPohybu()->requirePlayer(3)->toArray();
        unset($data['movedThisTurn']);
        $this->assertTrue(\App\DTO\MatchPlayerDTO::fromArray($data)->canTakeBlockAction());
    }

    // === Review P186: vstání na místě a Jump Up ===

    public function testVstaniNaMisteAPakBlokNejde(): void
    {
        // ř. 674-675: "a player who stands up may not take a Block Action" -- i když po
        // vstání neudělal ani krok. MOVE na vlastní pole = jen vstát (bez kostek).
        $state = (new GameStateBuilder())
            ->addPronePlayer(TeamSide::HOME, 5, 5, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, id: 2)
            ->build();
        $r = (new ActionResolver(new FixedDiceRoller([])))
            ->resolve($state, ActionType::MOVE, ['playerId' => 1, 'x' => 5, 'y' => 5]);
        $this->assertSame(PlayerState::STANDING, $r->getNewState()->requirePlayer(1)->getState(), 'fixtura: hráč vstal');

        $this->assertNotSame(
            [],
            (new RulesEngine())->validate($r->getNewState(), ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]),
        );
        $this->assertNotContains(1, $this->nabidka($r->getNewState(), ActionType::BLOCK));
    }

    public function testJumpUpSmiBlokovatZLehuAlePoPohybuNe(): void
    {
        // ř. 8200-8201: "The player may also declare a Block Action while Prone" -- blok
        // z lehu Jump Up smí. ř. 8197-8199 + 674-675: vstane-li jinou akcí (Move), už ne.
        $state = (new GameStateBuilder())
            ->addPronePlayer(TeamSide::HOME, 5, 5, skills: [SkillName::JumpUp], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, id: 2)
            ->build();

        $this->assertSame([], (new RulesEngine())->validate($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]));
        $this->assertContains(1, $this->nabidka($state, ActionType::BLOCK));

        $r = (new ActionResolver(new FixedDiceRoller([])))
            ->resolve($state, ActionType::MOVE, ['playerId' => 1, 'x' => 5, 'y' => 5]);
        $this->assertSame(PlayerState::STANDING, $r->getNewState()->requirePlayer(1)->getState(), 'fixtura: Jump Up vstal akcí Move');

        $this->assertNotSame(
            [],
            (new RulesEngine())->validate($r->getNewState(), ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]),
        );
        $this->assertNotContains(1, $this->nabidka($r->getNewState(), ActionType::BLOCK));
    }
}
