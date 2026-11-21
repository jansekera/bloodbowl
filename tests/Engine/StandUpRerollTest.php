<?php

declare(strict_types=1);

namespace App\Tests\Engine;

use App\DTO\GameState;
use App\Engine\ActionResolver;
use App\Engine\FixedDiceRoller;
use App\Engine\RulesEngine;
use App\Enum\ActionType;
use App\Enum\PlayerState;
use App\Enum\RollCategory;
use App\Enum\RollType;
use App\Enum\TeamSide;
use PHPUnit\Framework\TestCase;

/**
 * P86 pokračování (19.11.2026). `rules_bb2016.txt` ř. 690-695: pod 3 pole pohybu vstává hráč na 4+, neúspěch není
 * turnover. Ř. 929-933: týmový přehoz smí přehodit „any dice roll (other than Armour, Injury or Casualty rolls) made
 * by a player in their own team … during their own turn“ ⇒ i hod na vstání. Dřív PHP hodilo holou kostkou bez přehozu
 * (C++ `move_handler.cpp` přes `attemptRoll`), a nabídka pohybu ukazovala vstání s jistotou 100 %.
 */
final class StandUpRerollTest extends TestCase
{
    private function state(int $rerolls = 1): GameState
    {
        $s = (new GameStateBuilder())
            ->addPronePlayer(TeamSide::HOME, 5, 7, movement: 2, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 7, id: 2)
            ->withBallOffPitch()
            ->build();

        return $s->withTeamState(TeamSide::HOME, $s->getTeamState(TeamSide::HOME)->withRerolls($rerolls));
    }

    public function testTheAiRerollsAFailedStandUpWithATeamReroll(): void
    {
        // vstání 3 = neúspěch · týmový přehoz 4 = vstane
        $r = (new ActionResolver(new FixedDiceRoller([3, 4])))->resolve($this->state(), ActionType::MOVE, ['playerId' => 1, 'x' => 5, 'y' => 7]);

        $this->assertFalse($r->isTurnover());
        $this->assertSame(PlayerState::STANDING, $r->getNewState()->requirePlayer(1)->getState(), 'ř. 929-933: hod na vstání se přehazuje');
        $this->assertSame(0, $r->getNewState()->getTeamState(TeamSide::HOME)->getRerolls());
    }

    public function testWithoutARerollAFailedStandUpLeavesThePlayerProneWithoutATurnover(): void
    {
        $dice = new FixedDiceRoller([3, 6]);
        $r = (new ActionResolver($dice))->resolve($this->state(0), ActionType::MOVE, ['playerId' => 1, 'x' => 5, 'y' => 7]);

        $this->assertFalse($r->isTurnover(), 'ř. 694-695');
        $this->assertSame(1, $dice->getRollCount());
        $this->assertSame(PlayerState::PRONE, $r->getNewState()->requirePlayer(1)->getState());
    }

    public function testAHumanCoachIsAskedAndTheRerollStandsThePlayerUpInABlitz(): void
    {
        $s = $this->state();
        $r1 = new ActionResolver(new FixedDiceRoller([3]));
        $r1->setHumanSides([TeamSide::HOME]);
        $s = $r1->resolve($s, ActionType::BLITZ, ['playerId' => 1, 'targetId' => 2])->getNewState();

        $q = $s->getPendingReroll();
        $this->assertNotNull($q, 'kouče-člověka se nezeptal');
        $this->assertSame(RollType::STAND_UP, $q->getRollType());
        $this->assertSame(4, $q->getTarget());
        $this->assertSame(RollCategory::MOVE, RollType::STAND_UP->category());
        $this->assertNotNull(RollType::STAND_UP->consequence());

        // přehoz 4 = vstane · GFI na ránu 6 · rána 3 = PUSHED
        $r2 = new ActionResolver(new FixedDiceRoller([4, 6, 3]));
        $r2->setHumanSides([TeamSide::HOME]);
        $res = $r2->resolve($s, ActionType::RESOLVE_REROLL, ['choice' => 'team_reroll']);

        $this->assertNull($res->getNewState()->getPendingReroll());
        $this->assertSame(PlayerState::STANDING, $res->getNewState()->requirePlayer(1)->getState());
        $this->assertSame(7, $res->getNewState()->requirePlayer(2)->requirePosition()->getX(), 'rána se nehodila');
    }

    public function testTheMoveOfferCountsTheFourPlusStandUpRoll(): void
    {
        $s = $this->state();
        $s = $s->withPlayer($s->requirePlayer(2)->withPosition(new \App\ValueObject\Position(20, 7)));
        $cile = [];
        foreach ((new RulesEngine())->getValidMoveTargets($s, 1) as $t) {
            $cile[$t['x'] . ',' . $t['y']] = $t['successChance'];
        }

        $this->assertSame(50, $cile['5,7'], 'vstání na místě: 4+ = 50 %');
        $this->assertSame(42, $cile['6,7'], 'vstání 4+ a GFI 2+: 3/6 · 5/6');
    }
    public function testJumpUpStandsUpWithCertaintyInTheMoveOffer(): void
    {
        $s = (new GameStateBuilder())
            ->addPronePlayer(TeamSide::HOME, 5, 7, movement: 2, skills: [\App\Enum\SkillName::JumpUp], id: 1)
            ->addPlayer(TeamSide::AWAY, 20, 7, id: 2)
            ->build();
        $t = (new RulesEngine())->getValidMoveTargets($s, 1)[0];

        $this->assertSame([5, 7, 100], [$t['x'], $t['y'], $t['successChance']], 'ř. 8197-8199: Jump Up vstává zdarma, bez hodu');
    }

    /**
     * Ř. 690-692: „If the player has less than three squares of movement“ -- rozhoduje zbývající pohyb (jako C++
     * `movementRemaining < 3` a `Pathfinder::pohybPoVstani`), ne MA ze statistik. Dřív `resolveStandUp` bral MA
     * ze statistik: hráč s MA 6 a 2 zbývajícími poli vstal bez hodu a šel do −1.
     */
    public function testUnderThreeRemainingSquaresTheStandUpIsARollEvenWithHigherMa(): void
    {
        $s = (new GameStateBuilder())
            ->addPronePlayer(TeamSide::HOME, 5, 7, movement: 6, id: 1)
            ->addPlayer(TeamSide::AWAY, 20, 7, id: 2)
            ->build();
        $s = $s->withPlayer($s->requirePlayer(1)->withMovementRemaining(2));
        $s = $s->withTeamState(TeamSide::HOME, $s->getTeamState(TeamSide::HOME)->withRerolls(0));

        $r = (new ActionResolver(new FixedDiceRoller([3])))->resolve($s, ActionType::MOVE, ['playerId' => 1, 'x' => 5, 'y' => 7]);

        $this->assertSame(PlayerState::PRONE, $r->getNewState()->requirePlayer(1)->getState(), 'vstal bez hodu');
    }

    public function testTheGfiCapIsTwoThreeWithSprintAndZeroWhenRooted(): void
    {
        $s = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1)
            ->addPlayer(TeamSide::HOME, 5, 9, skills: [\App\Enum\SkillName::Sprint], id: 3)
            ->addPlayer(TeamSide::AWAY, 20, 7, id: 2)
            ->build();

        $this->assertSame(2, \App\Engine\Pathfinder::maxGfi($s->requirePlayer(1)), 'ř. 1695-1697');
        $this->assertSame(3, \App\Engine\Pathfinder::maxGfi($s->requirePlayer(3)), 'Sprint ř. 8487-8490');
        $this->assertSame(0, \App\Engine\Pathfinder::maxGfi($s->requirePlayer(1)->withRooted(true)), 'Take Root ř. 8577-8578');
    }
    /**
     * Rozhodnutí uživatele 20.11. (P86, otázka 9): AI v BLITZU nepřehazuje neúspěšné vstání, když na cíl ani po vstání
     * nedosáhne. Pod 3 MA zbude po vstání 0 pohybu, dál jen 2 GFI, a rána stojí pole (ř. 549-550, 690-695) ⇒ dojde
     * nejvýš na 1 pole. Soupeř na (9,7): sousední pole (8,7) jsou 3 kroky ⇒ nedosáhne, přehoz by byl zbytečný.
     */
    public function testTheAiDoesNotRerollTheStandUpInABlitzAtATargetItCannotReach(): void
    {
        $s = $this->state();
        $s = $s->withPlayer($s->requirePlayer(2)->withPosition(new \App\ValueObject\Position(9, 7)));

        // vstání 3 = neúspěch; víc kostek není -- přehoz by spadl na „no more rolls“
        $r = (new ActionResolver(new FixedDiceRoller([3])))->resolve($s, ActionType::BLITZ, ['playerId' => 1, 'targetId' => 2]);

        $this->assertFalse($r->isTurnover());
        $this->assertSame(PlayerState::PRONE, $r->getNewState()->requirePlayer(1)->getState());
        $this->assertSame(1, $r->getNewState()->getTeamState(TeamSide::HOME)->getRerolls(), 'týmový přehoz utracen na nedosažitelný cíl');
    }

    /** Pozitivní kontrola k předchozímu: cíl, na který po vstání dosáhne přes GFI (1 krok + rána), AI přehodí. */
    public function testTheAiRerollsTheStandUpInABlitzAtATargetReachableWithGfi(): void
    {
        $s = $this->state();
        $s = $s->withPlayer($s->requirePlayer(2)->withPosition(new \App\ValueObject\Position(7, 7)));

        // vstání 3 · přehoz 4 · GFI kroku 6 · GFI rány 6 · rána 3 = PUSHED
        $r = (new ActionResolver(new FixedDiceRoller([3, 4, 6, 6, 3])))->resolve($s, ActionType::BLITZ, ['playerId' => 1, 'targetId' => 2]);

        $this->assertSame(PlayerState::STANDING, $r->getNewState()->requirePlayer(1)->getState());
        $this->assertSame(0, $r->getNewState()->getTeamState(TeamSide::HOME)->getRerolls());
        $this->assertSame(8, $r->getNewState()->requirePlayer(2)->requirePosition()->getX(), 'rána se nehodila');
    }

    /** Uživatel 20.11.: „člověka se zeptat“ -- omezení je jen pro AI. */
    public function testAHumanCoachIsAskedEvenWhenTheBlitzTargetIsOutOfReach(): void
    {
        $s = $this->state();
        $s = $s->withPlayer($s->requirePlayer(2)->withPosition(new \App\ValueObject\Position(9, 7)));
        $r = new ActionResolver(new FixedDiceRoller([3]));
        $r->setHumanSides([TeamSide::HOME]);

        $q = $r->resolve($s, ActionType::BLITZ, ['playerId' => 1, 'targetId' => 2])->getNewState()->getPendingReroll();
        $this->assertNotNull($q);
        $this->assertSame(RollType::STAND_UP, $q->getRollType());
    }
}
