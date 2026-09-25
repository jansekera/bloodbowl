<?php

declare(strict_types=1);

namespace App\Tests\Engine;

use App\DTO\GameEvent;
use App\Engine\ActionResolver;
use App\Engine\BallResolver;
use App\Engine\FixedDiceRoller;
use App\Engine\ScatterCalculator;
use App\Engine\TacklezoneCalculator;
use App\Enum\ActionType;
use App\Enum\PlayerState;
use App\Enum\TeamSide;
use App\ValueObject\Position;
use PHPUnit\Framework\TestCase;

/**
 * Throw-in podle rules_bb2016 r. 868-878:
 *  - sablona Throw-in od posledniho pole na hristi, vzdalenost 2D6;
 *  - stojici hrac na cilovem poli MUSI chytat; prazdne pole nebo lezici/omraceny = odskok;
 *  - kdyz mic vyleti znovu, vhazuje se znovu od posledniho pole, kde byl.
 * Sablona (LRB6, shodne s C++ `ball_handler.cpp:129`): u strany D6 -- 1-2 diagonala,
 * 3-4 kolmo zpet do hriste, 5-6 druha diagonala; v rohu D3 -- podel jedne hrany,
 * diagonala, podel druhe.
 * A r. 659-663: nosic vytlaceny do davu -- mic vhazuje dav od posledniho pole nosice.
 */
final class ThrowInTest extends TestCase
{
    private function resolver(FixedDiceRoller $dice): BallResolver
    {
        return new BallResolver($dice, new TacklezoneCalculator(), new ScatterCalculator());
    }

    /** @param list<GameEvent> $events @return list<GameEvent>
     *
     * @param list<\App\DTO\GameEvent> $events
     * @return list<\App\DTO\GameEvent>
     */
    private function throwIns(array $events): array
    {
        return array_values(array_filter($events, fn($e) => $e->getType() === 'throw_in'));
    }

    public function testFromLeftEdgeStraightIn2D6ThenBounce(): void
    {
        $state = (new GameStateBuilder())->withBallOnGround(0, 7)->build();

        // sablona 3 = kolmo (V), 2D6 = 2+3 = 5 -> (5,7) prazdne -> odskok 5 (J) -> (5,8)
        $dice = new FixedDiceRoller([3, 2, 3, 5]);
        $result = $this->resolver($dice)->resolveThrowIn($state, new Position(0, 7), new Position(-1, 7));

        $this->assertEquals(new Position(5, 8), $result['state']->getBall()->requirePosition());
        $this->assertSame('(5,7)', $this->throwIns($result['events'])[0]->getData()['to']);
        $this->assertFalse($dice->hasRemainingRolls());
    }

    public function testFromBottomEdgeDiagonalOntoStandingPlayerWhoMustCatch(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 12, 12, agility: 3, id: 1)
            ->withBallOnGround(10, 14)
            ->build();

        // sablona 6 = diagonala SV, 2D6 = 1+1 = 2 -> (12,12), hrac chyta 6
        $dice = new FixedDiceRoller([6, 1, 1, 6]);
        $result = $this->resolver($dice)->resolveThrowIn($state, new Position(10, 14), new Position(10, 15));

        $this->assertSame(1, $result['state']->getBall()->getCarrierId());
        $this->assertFalse($dice->hasRemainingRolls());
    }

    public function testCornerUsesD3AlongEdgeDiagonalOrOtherEdge(): void
    {
        $state = (new GameStateBuilder())->withBallOnGround(0, 0)->build();

        // roh vlevo nahore: D6 3 -> D3 2 = diagonala JV; 2D6 = 1+2 = 3 -> (3,3) -> odskok 3 (V) -> (4,3)
        $dice = new FixedDiceRoller([3, 1, 2, 3]);
        $result = $this->resolver($dice)->resolveThrowIn($state, new Position(0, 0), new Position(-1, -1));

        $this->assertEquals(new Position(4, 3), $result['state']->getBall()->requirePosition());
        $this->assertFalse($dice->hasRemainingRolls());
    }

    public function testLandingOnPronePlayerBounces(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 4, 7, id: 1)
            ->withBallOnGround(0, 7)
            ->build();
        $state = $state->withPlayer($state->requirePlayer(1)->withState(PlayerState::PRONE));

        // sablona 4 = kolmo, 2D6 = 2+2 -> (4,7) lezici hrac -> odskok 3 -> (5,7)
        $dice = new FixedDiceRoller([4, 2, 2, 3]);
        $result = $this->resolver($dice)->resolveThrowIn($state, new Position(0, 7), new Position(-1, 7));

        $this->assertEquals(new Position(5, 7), $result['state']->getBall()->requirePosition());
        $this->assertNull($result['state']->getBall()->getCarrierId());
        $this->assertFalse($dice->hasRemainingRolls());
    }

    public function testOffPitchAgainIsRethrownFromLastSquareItWasIn(): void
    {
        $state = (new GameStateBuilder())->withBallOnGround(10, 0)->build();

        // horni hrana: 1 = diagonala JZ, 2D6 = 12 -> (9,1)...(0,10), dalsi krok (-1,11) mimo;
        // znovu od (0,10), leva hrana: 3 = V, 2D6 = 2 -> (2,10) -> odskok 3 -> (3,10)
        $dice = new FixedDiceRoller([1, 6, 6, 3, 1, 1, 3]);
        $result = $this->resolver($dice)->resolveThrowIn($state, new Position(10, 0), new Position(10, -1));

        $throwIns = $this->throwIns($result['events']);
        $this->assertCount(2, $throwIns);
        $this->assertSame('(0,10)', $throwIns[1]->getData()['from']);
        $this->assertEquals(new Position(3, 10), $result['state']->getBall()->requirePosition());
        $this->assertFalse($dice->hasRemainingRolls());
    }

    public function testCarrierPushedIntoCrowdBallIsThrownInFromHisSquare(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 24, 5, strength: 3, id: 1)
            ->addPlayer(TeamSide::AWAY, 25, 5, strength: 3, id: 2) // nosic u prave hrany
            ->withBallCarried(2)
            ->build();

        // blok 3 = odstrceni do davu; throw-in: sablona 3 = Z, 2D6 = 1+2 -> (22,5) -> odskok 5 -> (22,6);
        // zraneni od davu 2D6 = 3+3
        $dice = new FixedDiceRoller([3, 3, 1, 2, 5, 3, 3]);
        $result = (new ActionResolver($dice))->resolve($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);

        $throwIns = $this->throwIns($result->getEvents());
        $this->assertCount(1, $throwIns, 'r. 659-663: dav mic vhazuje zpet');
        $this->assertSame('(25,5)', $throwIns[0]->getData()['from']);
        $this->assertEquals(new Position(22, 6), $result->getNewState()->getBall()->requirePosition());
        $this->assertFalse($dice->hasRemainingRolls());
    }

    public function testBounceOntoPronePlayerBouncesAgain(): void
    {
        // r. 893-898: „the ball bounces to a square with a Prone or Stunned player ... then it will bounce"
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 11, 7, id: 1)
            ->withBallOnGround(10, 7)
            ->build();
        $state = $state->withPlayer($state->requirePlayer(1)->withState(PlayerState::STUNNED));

        // odskok 3 (V) na omraceneho (11,7) -> odskok 5 (J) -> (11,8)
        $dice = new FixedDiceRoller([3, 5]);
        $result = $this->resolver($dice)->resolveBounce($state, new Position(10, 7));

        $this->assertEquals(new Position(11, 8), $result['state']->getBall()->requirePosition());
        $this->assertFalse($dice->hasRemainingRolls());
    }
}
