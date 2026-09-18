<?php

declare(strict_types=1);

namespace App\Tests\Engine;

use App\Engine\ActionResolver;
use App\Engine\FixedDiceRoller;
use App\Enum\ActionType;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use PHPUnit\Framework\TestCase;

/**
 * Nepresna prihravka se rozptyluje TRIKRAT po jednom poli (rules_bb2016 r. 735-740):
 * „Roll for scatter three times, one after the other ... The ball can only be caught
 * in the final square." A r. 271-273: D6 na vzdalenost jen u vykopu, jinak
 * „the ball only moves one square per Scatter roll."
 *
 * Mic, ktery pri rozptylu opusti hriste, je „immediately thrown back in" (r. 868-871)
 * od „the last square the ball crossed before going off" -- dalsi rozptyly se uz nehazi.
 *
 * Kostky jsou volene tak, aby stara implementace (1 smer × min(3, D6), throw-in od cile)
 * dopadla jinak.
 */
final class MissedPassScatterTest extends TestCase
{
    public function testMissedPassScattersThreeTimesOneSquareEach(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, agility: 3, id: 1)
            ->addPlayer(TeamSide::HOME, 7, 5, agility: 3, id: 2)
            ->addPlayer(TeamSide::HOME, 7, 6, agility: 3, id: 3)
            ->withBallCarried(1)
            ->build();

        // presnost 2, prehoz 2 = nepresna; rozptyl V (8,5), J (8,6), Z (7,6); chytani 6
        // (stara verze: V × min(3,5) = (10,5))
        $dice = new FixedDiceRoller([2, 2, 3, 5, 7, 6]);
        $result = (new ActionResolver($dice))->resolve($state, ActionType::PASS, [
            'playerId' => 1, 'targetX' => 7, 'targetY' => 5,
        ]);

        $this->assertFalse($result->isTurnover());
        $this->assertSame(3, $result->getNewState()->getBall()->getCarrierId());
        $this->assertFalse($dice->hasRemainingRolls());
    }

    public function testBallScatteringThroughPlayerSquareIsNotCaughtThere(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, agility: 3, id: 1)
            ->addPlayer(TeamSide::HOME, 7, 5, agility: 3, id: 2)
            ->addPlayer(TeamSide::HOME, 8, 5, agility: 3, id: 3) // prvni pole rozptylu
            ->withBallCarried(1)
            ->build();

        // rozptyl pres (8,5) a (8,6) do prazdneho (7,6) -> odskok S na (7,5) -> hrac 2 chyta 6
        $dice = new FixedDiceRoller([2, 2, 3, 5, 7, 1, 6]);
        $result = (new ActionResolver($dice))->resolve($state, ActionType::PASS, [
            'playerId' => 1, 'targetX' => 7, 'targetY' => 5,
        ]);

        $this->assertSame(2, $result->getNewState()->getBall()->getCarrierId(),
            'hrac na poli, pres ktere mic jen proletel, chytat nesmi (r. 739-741)');
        $this->assertFalse($dice->hasRemainingRolls());
    }

    public function testMissedPassOffPitchStopsScatteringAndThrowsInFromLastSquare(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 3, agility: 3, id: 1)
            ->addPlayer(TeamSide::HOME, 7, 1, agility: 3, id: 2)
            ->addPlayer(TeamSide::HOME, 7, 3, agility: 3, id: 3)
            ->withBallCarried(1)
            ->build();

        // rozptyl S (7,0), S (7,-1) mimo -> treti rozptyl se NEHAZI;
        // throw-in od (7,0): smer 5, vzdalenost 3 -> (7,3), hrac 3 chyta 6
        // (stara verze: S × min(3,1) = (7,0), zadny throw-in)
        $dice = new FixedDiceRoller([2, 2, 1, 1, 5, 3, 6]);
        $result = (new ActionResolver($dice))->resolve($state, ActionType::PASS, [
            'playerId' => 1, 'targetX' => 7, 'targetY' => 1,
        ]);

        $throwIns = array_values(array_filter($result->getEvents(), fn($e) => $e->getType() === 'throw_in'));
        $this->assertCount(1, $throwIns);
        $this->assertSame('(7,0)', $throwIns[0]->getData()['from']);
        $this->assertFalse($dice->hasRemainingRolls());
    }

    public function testHailMaryOffPitchStopsScatteringAndThrowsInFromLastSquare(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 1, 5, agility: 3, skills: [SkillName::HailMaryPass], id: 1)
            ->addPlayer(TeamSide::HOME, 20, 1, agility: 3, id: 2)
            ->addPlayer(TeamSide::HOME, 20, 3, agility: 3, id: 3)
            ->withBallCarried(1)
            ->build();

        // hod 4 (ne fumble); rozptyl S (20,0), S (20,-1) mimo -> dal se nehazi;
        // throw-in od (20,0): 5, 3 -> (20,3), hrac 3 chyta 6
        // (stara verze: treti rozptyl 5 vrati mic na (20,0), zadny throw-in)
        $dice = new FixedDiceRoller([4, 1, 1, 5, 3, 6]);
        $result = (new ActionResolver($dice))->resolve($state, ActionType::PASS, [
            'playerId' => 1, 'targetX' => 20, 'targetY' => 1,
        ]);

        $throwIns = array_values(array_filter($result->getEvents(), fn($e) => $e->getType() === 'throw_in'));
        $this->assertCount(1, $throwIns);
        $this->assertSame('(20,0)', $throwIns[0]->getData()['from']);
        $this->assertFalse($dice->hasRemainingRolls());
    }

    public function testDumpOffMissedPassScattersThreeTimesOneSquareEach(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, id: 1) // utocnik
            ->addPlayer(TeamSide::AWAY, 6, 5, agility: 3, skills: [SkillName::DumpOff], id: 2)
            ->addPlayer(TeamSide::AWAY, 8, 5, agility: 3, id: 3) // cil dump-offu
            ->addPlayer(TeamSide::AWAY, 8, 6, agility: 3, id: 4)
            ->withBallCarried(2)
            ->build();

        // dump-off: presnost 2 = nepresna; rozptyl V (9,5), J (9,6), Z (8,6); hrac 4 chyta 6
        // blok: 3 = odstrceni
        $dice = new FixedDiceRoller([2, 3, 5, 7, 6, 3]);
        $result = (new ActionResolver($dice))->resolve($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);

        $this->assertSame(4, $result->getNewState()->getBall()->getCarrierId());
    }
}
