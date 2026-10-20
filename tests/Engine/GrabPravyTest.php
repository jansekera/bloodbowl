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
use PHPUnit\Framework\TestCase;

/**
 * P192 PARITA — PRAVÝ GRAB (převod C++ cd58d6b6, b697d22a, 15b6c592).
 *
 * `rules_bb2016.txt` ř. 8147-8154 (Grab): "only while making a Block Action, if his block results
 * in a push back he **may** choose **any empty square adjacent to his opponent** to push back his
 * opponent. When making a Block **or Blitz** Action, Grab and Side Step will cancel each other
 * out and the standard pushback rules apply. Grab will not work if there are no empty adjacent
 * squares."
 * Uživatel 10.10.: "většina skillů jsou nepovinné - může grab nevyužít pro vysurfování" — dav
 * hlídá `CombatSkillsTest::testGrabJeVolbaBezVolnehoPoleZeTriJdeObranceDoDavu`.
 *
 * Stará mechanika PHP: Grab vybíral jen ze tří polí diagramu (při obsazených řetězil) a platil
 * i v blitzu.
 */
final class GrabPravyTest extends TestCase
{
    /** @param list<int> $kostky */
    private function rana(GameState $state, ActionType $akce, array $kostky): ActionResult
    {
        return (new ActionResolver(new FixedDiceRoller($kostky)))
            ->resolve($state, $akce, ['playerId' => 1, 'targetId' => 2]);
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

    /**
     * Útočník (5,7) → obránce (6,7); tři pole odtlačení (7,6..8) drží hráči útočníka.
     * Volná sousední pole obránce: (5,6), (5,8), (6,6), (6,8).
     *
     * @param list<SkillName> $utocnik
     * @param list<SkillName> $obrance
     */
    private function triPoleObsazena(array $utocnik, array $obrance = []): GameState
    {
        return (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, skills: $utocnik, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 7, skills: $obrance, id: 2)
            ->addPlayer(TeamSide::HOME, 7, 6, id: 3)
            ->addPlayer(TeamSide::HOME, 7, 7, id: 4)
            ->addPlayer(TeamSide::HOME, 7, 8, id: 5)
            ->withBallOffPitch()
            ->build();
    }

    public function testGrabVBlitzuNeplati(): void
    {
        // ř. 8148 "only while making a Block Action". Prázdné okolí: Grab by mohl zvolit jiné
        //   pole než běžné odtlačení -- v blitzu musí obránce skončit tam, kam ho pošle útočník
        //   bez Grabu.
        $state = fn(bool $grab): GameState => (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, skills: $grab ? [SkillName::Grab] : [], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, id: 2)
            ->withBallOffPitch()
            ->build();

        $sGrabem = $this->rana($state(true), ActionType::BLITZ, [3, 3, 3]);
        $bezGrabu = $this->rana($state(false), ActionType::BLITZ, [3, 3, 3]);

        $this->assertSame($this->pole($bezGrabu, 2), $this->pole($sGrabem, 2));
    }

    public function testGrabVBlitzuRusiSideStep(): void
    {
        // ř. 8151-8153 "When making a Block or Blitz Action, Grab and Side Step will cancel each
        //   other out" -- v blitzu Grab sám neplatí, Side Step ale zruší ⇒ běžné odtlačení = řetěz.
        //   (C++ to mělo obráceně, cd58d6b6.) Pozitivní kontrola: bez Grabu Side Step uhne vedle.
        $sGrabem = $this->rana($this->triPoleObsazena([SkillName::Grab], [SkillName::SideStep]), ActionType::BLITZ, [3, 3, 3, 3]);
        $bezGrabu = $this->rana($this->triPoleObsazena([], [SkillName::SideStep]), ActionType::BLITZ, [3, 3, 3, 3]);

        $this->assertContains('chain_push', $this->typy($sGrabem));
        $this->assertSame(7, $this->pole($sGrabem, 2)[0]);
        $this->assertNotContains('chain_push', $this->typy($bezGrabu));
        $this->assertNotSame(7, $this->pole($bezGrabu, 2)[0]);
    }
}
