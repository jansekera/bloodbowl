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
 * P86 (19.11.2026), port C++ 38e4fd85. `rules_bb2016.txt` ř. 670-676: ležící „may do nothing before standing
 * up at a cost of three squares of his movement when he next takes an Action … a player who stands up may not
 * take a Block Action … The player may take any Action other than a Block Action“ ⇒ Blitz z lehu je legální.
 * Ř. 690-695: pod 3 pole pohybu vstává na 4+ a dál jen přes GFI; neúspěch není turnover. Jump Up ř. 8197-8199:
 * „declares any Action other than a Block Action he may stand up for free“. Rána v blitzu stojí pole (ř. 549-550).
 * Dřív PHP ležícímu blitz neohlásilo vůbec (`canAct()` = jen stojící).
 */
final class BlitzFromProneTest extends TestCase
{
    /** @param list<SkillName> $skills */
    private function state(int $enemyX, int $movement = 6, array $skills = []): GameState
    {
        $s = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, movement: $movement, skills: $skills, id: 1)
            ->addPlayer(TeamSide::AWAY, $enemyX, 7, id: 2)
            ->withBallOffPitch()
            ->build();
        $s = $s->withPlayer($s->requirePlayer(1)->withState(PlayerState::PRONE));

        return $s->withTeamState(TeamSide::HOME, $s->getTeamState(TeamSide::HOME)->withRerolls(0));
    }

    /** @return list<string> */
    private function offered(GameState $s, int $playerId): array
    {
        return array_values(array_map(fn($a) => $a['type'], array_filter(
            (new RulesEngine())->getAvailableActions($s),
            fn($a) => ($a['playerId'] ?? null) === $playerId,
        )));
    }

    public function testAProneBlitzerStandsUpForThreeSquaresAndThenBlocks(): void
    {
        $s = $this->state(6);
        $this->assertSame([], (new RulesEngine())->validate($s, ActionType::BLITZ, ['playerId' => 1, 'targetId' => 2]), 'ř. 676: blitz z lehu je legální');

        // rána 3 = PUSHED
        $r = (new ActionResolver(new FixedDiceRoller([3])))->resolve($s, ActionType::BLITZ, ['playerId' => 1, 'targetId' => 2]);

        $this->assertFalse($r->isTurnover());
        $hrac = $r->getNewState()->requirePlayer(1);
        $this->assertSame(PlayerState::STANDING, $hrac->getState());
        $this->assertSame(2, $hrac->getMovementRemaining(), 'ř. 670-672: vstání 3 pole + rána 1 pole (MA 6)');
        $this->assertSame(7, $r->getNewState()->requirePlayer(2)->requirePosition()->getX(), 'rána se nehodila');
    }

    public function testJumpUpStandsUpForFreeInABlitz(): void
    {
        $s = $this->state(6, skills: [SkillName::JumpUp]);
        $this->assertSame([], (new RulesEngine())->validate($s, ActionType::BLITZ, ['playerId' => 1, 'targetId' => 2]));

        // Jen kostka rány: hod AG +2 (Jump Up u Block Action, ř. 8199-8203) se v blitzu NEHÁZÍ.
        $dice = new FixedDiceRoller([3]);
        $r = (new ActionResolver($dice))->resolve($s, ActionType::BLITZ, ['playerId' => 1, 'targetId' => 2]);

        $this->assertFalse($r->isTurnover());
        $this->assertSame(1, $dice->getRollCount(), 'v blitzu se hodilo na vstání');
        $this->assertSame(5, $r->getNewState()->requirePlayer(1)->getMovementRemaining(), 'ř. 8197-8199: vstání zdarma, rána 1 pole');
        $this->assertSame(7, $r->getNewState()->requirePlayer(2)->requirePosition()->getX());
    }

    public function testUnderThreeMaTheBlitzerRollsFourPlusThenPaysTheBlockWithAGfi(): void
    {
        $s = $this->state(6, movement: 2);
        $this->assertSame([], (new RulesEngine())->validate($s, ActionType::BLITZ, ['playerId' => 1, 'targetId' => 2]));

        // vstání 4 · GFI na ránu 6 · rána 3 = PUSHED
        $r = (new ActionResolver(new FixedDiceRoller([4, 6, 3])))->resolve($s, ActionType::BLITZ, ['playerId' => 1, 'targetId' => 2]);

        $this->assertFalse($r->isTurnover());
        $types = array_map(fn($e) => $e->getType(), $r->getEvents());
        $this->assertContains('gfi', $types, 'ř. 692-694: po vstání na hod jen přes GFI');
        $this->assertSame(-1, $r->getNewState()->requirePlayer(1)->getMovementRemaining());
        $this->assertSame(7, $r->getNewState()->requirePlayer(2)->requirePosition()->getX());
    }

    public function testUnderThreeMaAFailedStandUpEndsTheBlitzWithoutATurnover(): void
    {
        $s = $this->state(6, movement: 2);

        // vstání 3 = neúspěch; další kostka by byla rána -- nesmí se hodit
        $dice = new FixedDiceRoller([3, 3]);
        $r = (new ActionResolver($dice))->resolve($s, ActionType::BLITZ, ['playerId' => 1, 'targetId' => 2]);

        $this->assertFalse($r->isTurnover(), 'ř. 694-695: neúspěšné vstání není turnover');
        $this->assertSame(1, $dice->getRollCount());
        $this->assertSame(PlayerState::PRONE, $r->getNewState()->requirePlayer(1)->getState());
        $this->assertSame(6, $r->getNewState()->requirePlayer(2)->requirePosition()->getX());
        $this->assertTrue($r->getNewState()->getTeamState(TeamSide::HOME)->isBlitzUsedThisTurn(), 'blitz byl ohlášen');
    }

    public function testAProneLinemanIsOfferedBlitzButNotBlock(): void
    {
        $s = $this->state(6);
        $nabidka = $this->offered($s, 1);

        $this->assertContains(ActionType::BLITZ->value, $nabidka, 'ř. 676: blitz z lehu');
        $this->assertNotContains(ActionType::BLOCK->value, $nabidka, 'ř. 674-676: Block z lehu ne');
        $this->assertNotSame([], (new RulesEngine())->validate($s, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]));
        $this->assertCount(1, (new RulesEngine())->getBlitzTargets($s, $s->requirePlayer(1)));
    }

    public function testAStunnedPlayerStillCannotBlitz(): void
    {
        $s = $this->state(6);
        $s = $s->withPlayer($s->requirePlayer(1)->withState(PlayerState::STUNNED));

        $this->assertNotContains(ActionType::BLITZ->value, $this->offered($s, 1));
        $this->assertNotSame([], (new RulesEngine())->validate($s, ActionType::BLITZ, ['playerId' => 1, 'targetId' => 2]));
    }

    public function testStandingUpShortensTheReachOfTheBlitz(): void
    {
        // Soupeř na (11,7): stojící MA 6 dojde 5 polí na (10,7) a ránu zaplatí 1. GFI. Ležící má po vstání
        //   3 pole + 2 GFI = 5, z toho 1 na ránu ⇒ dojde nejvýš 4 pole -- na sousední pole nedosáhne.
        $lezi = $this->state(11);
        $stoji = $lezi->withPlayer($lezi->requirePlayer(1)->withState(PlayerState::STANDING));
        $rules = new RulesEngine();

        $this->assertNotNull($rules->blitzLandingSquares($stoji, 1, [$stoji->requirePlayer(2)])[2], 'fixtura: stojící dosáhne');
        $this->assertNull($rules->blitzLandingSquares($lezi, 1, [$lezi->requirePlayer(2)])[2], 'vstání se do dosahu nepočítalo');

        // A totéž v provedení: rána se nehodí, ležící se přiblíží jen normálním pohybem (3 pole po vstání).
        $r = (new ActionResolver(new FixedDiceRoller([])))->resolve($lezi, ActionType::BLITZ, ['playerId' => 1, 'targetId' => 2]);
        $this->assertFalse($r->isTurnover());
        $this->assertSame(11, $r->getNewState()->requirePlayer(2)->requirePosition()->getX());
        $this->assertSame(8, $r->getNewState()->requirePlayer(1)->requirePosition()->getX());
    }

    public function testAProneBlitzerReachesTheTargetWithTheRestAfterStandingUp(): void
    {
        // Soupeř na (10,7): po vstání 3 pole + 1 GFI na (9,7), rána za 2. GFI.
        $s = $this->state(10);
        $this->assertSame([], (new RulesEngine())->validate($s, ActionType::BLITZ, ['playerId' => 1, 'targetId' => 2]));
        $this->assertNotNull((new RulesEngine())->blitzLandingSquares($s, 1, [$s->requirePlayer(2)])[2]);

        // GFI 6 · GFI na ránu 6 · rána 3 = PUSHED
        $r = (new ActionResolver(new FixedDiceRoller([6, 6, 3])))->resolve($s, ActionType::BLITZ, ['playerId' => 1, 'targetId' => 2]);
        $this->assertFalse($r->isTurnover());
        $this->assertSame(11, $r->getNewState()->requirePlayer(2)->requirePosition()->getX(), 'rána se nehodila');
        $this->assertSame(-2, $r->getNewState()->requirePlayer(1)->getMovementRemaining());
    }

    public function testAFailedDodgeAfterStandingUpKeepsTheStandUpInTheEvents(): void
    {
        // Soupeř v zóně: vstane (3 pole), úhyb padne ⇒ turnover. Vstání se stalo -- záznam o něm nesmí zmizet
        //   (`BlitzHandler` vracel při turnoveru pohybu jen události pohybu).
        $s = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 7, id: 2)
            ->addPlayer(TeamSide::AWAY, 10, 7, id: 3)
            ->withBallOffPitch()
            ->build();
        $s = $s->withPlayer($s->requirePlayer(1)->withState(PlayerState::PRONE));
        $s = $s->withTeamState(TeamSide::HOME, $s->getTeamState(TeamSide::HOME)->withRerolls(0));

        $this->assertSame([], (new RulesEngine())->validate($s, ActionType::BLITZ, ['playerId' => 1, 'targetId' => 3]));
        // úhyb 1 = pád · brnění 1+1
        $r = (new ActionResolver(new FixedDiceRoller([1, 1, 1, 1, 1])))->resolve($s, ActionType::BLITZ, ['playerId' => 1, 'targetId' => 3]);

        $this->assertTrue($r->isTurnover(), 'fixtura: úhyb padl');
        $this->assertContains('stand_up', array_map(fn($e) => $e->getType(), $r->getEvents()));
    }

    public function testAProneBlitzCanBeDeclaredWithoutATargetAndTheMoveStandsUp(): void
    {
        $s = $this->state(20);
        $rules = new RulesEngine();
        $this->assertSame([], $rules->validate($s, ActionType::BLITZ, ['playerId' => 1]));

        $s = (new ActionResolver(new FixedDiceRoller([])))->resolve($s, ActionType::BLITZ, ['playerId' => 1])->getNewState();
        $this->assertSame([], $rules->validate($s, ActionType::MOVE, ['playerId' => 1, 'x' => 8, 'y' => 7]));
        $s = (new ActionResolver(new FixedDiceRoller([])))->resolve($s, ActionType::MOVE, ['playerId' => 1, 'x' => 8, 'y' => 7])->getNewState();

        $hrac = $s->requirePlayer(1);
        $this->assertSame(PlayerState::STANDING, $hrac->getState());
        $this->assertSame(0, $hrac->getMovementRemaining(), 'vstání 3 + 3 kroky');
    }

    public function testUnderThreeMaAProneMoverMayGoForItAfterStandingUp(): void
    {
        // Ř. 692-694: „if he stands up successfully, he may not move further squares unless he Goes For It“.
        //   Týž rozpočet po vstání slouží MOVE i blitzu (`Pathfinder`); dřív ležící pod 3 MA směl jen vstát na místě.
        $s = $this->state(20, movement: 2);
        $this->assertSame([], (new RulesEngine())->validate($s, ActionType::MOVE, ['playerId' => 1, 'x' => 6, 'y' => 7]));

        // vstání 4 · GFI 6
        $r = (new ActionResolver(new FixedDiceRoller([4, 6])))->resolve($s, ActionType::MOVE, ['playerId' => 1, 'x' => 6, 'y' => 7]);
        $this->assertFalse($r->isTurnover());
        $this->assertSame(6, $r->getNewState()->requirePlayer(1)->requirePosition()->getX());
        $this->assertSame(-1, $r->getNewState()->requirePlayer(1)->getMovementRemaining());
    }
    /**
     * Ř. 8582-8584: „if a player fails his Take Root roll as part of a Blitz Action he may not block that turn (he can
     * still roll to stand up if he is Prone)“. Ležící Treeman (MA 2) při blitzu propadne Take Root: vstává (hod 4+),
     * ale nehne se a ránu nedá. Dřív akce skončila hned a hráč zůstal ležet.
     */
    public function testAProneBlitzerWhoFailsTakeRootStandsUpButDoesNotBlock(): void
    {
        $s = $this->state(6, movement: 2, skills: [SkillName::TakeRoot]);

        // Take Root 1 · vstání 4; další kostka by byla rána -- nesmí se hodit (prázdná kostka by spadla)
        $dice = new FixedDiceRoller([1, 4]);
        $r = (new ActionResolver($dice))->resolve($s, ActionType::BLITZ, ['playerId' => 1, 'targetId' => 2]);

        $this->assertFalse($r->isTurnover());
        $hrac = $r->getNewState()->requirePlayer(1);
        $this->assertSame(PlayerState::STANDING, $hrac->getState(), 'ř. 8584: vstát smí');
        $this->assertTrue($hrac->isRooted());
        $this->assertTrue($hrac->hasActed(), 'po vstání akce končí');
        $this->assertSame(6, $r->getNewState()->requirePlayer(2)->requirePosition()->getX(), 'ř. 8583: blokovat nesmí');
        $this->assertNotContains('block', array_map(fn($e) => $e->getType(), $r->getEvents()));
    }

    public function testAProneBlitzDeclaredWithoutATargetWithAFailedTakeRootGetsNoBlockLater(): void
    {
        $s = $this->state(6, movement: 2, skills: [SkillName::TakeRoot]);

        // Take Root 1 · vstání 4
        $s = (new ActionResolver(new FixedDiceRoller([1, 4])))->resolve($s, ActionType::BLITZ, ['playerId' => 1])->getNewState();

        $this->assertSame(PlayerState::STANDING, $s->requirePlayer(1)->getState(), 'ř. 8584: vstát smí');
        $this->assertNotSame([], (new RulesEngine())->validate($s, ActionType::BLITZ, ['playerId' => 1, 'targetId' => 2]), 'ř. 8583: rána ne');
    }

    public function testADeclaredProneBlitzerIsStillOfferedTheBlitzBlow(): void
    {
        $s = $this->state(6);
        $s = (new ActionResolver(new FixedDiceRoller([])))->resolve($s, ActionType::BLITZ, ['playerId' => 1])->getNewState();

        $this->assertSame(PlayerState::PRONE, $s->requirePlayer(1)->getState(), 'fixtura: ohlášení samo nevstává');
        $this->assertContains(ActionType::BLITZ->value, $this->offered($s, 1), 'ohlášený ležící dá ránu (vstane v ní)');
    }

    public function testAProneBallAndChainPlayerIsNotOfferedABlitz(): void
    {
        $s = $this->state(20, skills: [SkillName::BallAndChain]);

        $this->assertNotContains(ActionType::BLITZ->value, $this->offered($s, 1), 'ř. 7809-7810: Ball & Chain jen pohyb');
    }
}
