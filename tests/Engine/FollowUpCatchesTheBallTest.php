<?php

declare(strict_types=1);

namespace App\Tests\Engine;

use App\DTO\BallState;
use App\Engine\ActionResolver;
use App\Engine\FixedDiceRoller;
use App\Enum\ActionType;
use App\Enum\TeamSide;
use App\ValueObject\Position;
use PHPUnit\Framework\TestCase;

/**
 * Audit parity 08.10. bod (c), ROZHODL UŽIVATEL 16.10.2026: „follow up chytá“ (C++ od 13.10., P194/6).
 * `rules_bb2016.txt` ř. 609-610: follow-up se rozhoduje „before any other dice rolls“ ⇒ míč, který se při
 * odtlačení odrazí na uvolněné pole, dopadá na útočníka, který tam už stojí, a ten ho CHYTÁ.
 * Dřív v PHP útočník na ležící míč vstoupil a nic se nestalo (dva v jednom poli: hráč a volný míč).
 */
final class FollowUpCatchesTheBallTest extends TestCase
{
    public function testTheFollowingAttackerCatchesTheBallThatBouncedIntoTheVacatedSquare(): void
    {
        // útočník (5,7), obránce (6,7), volný míč na poli odtlačení (7,7)
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, agility: 3, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 7, id: 2)
            ->addPlayer(TeamSide::AWAY, 7, 6, id: 3)          // obsazená pole odtlačení ⇒ jen (7,7)
            ->addPlayer(TeamSide::AWAY, 7, 8, id: 4)
            ->build()
            ->withBall(BallState::onGround(new Position(7, 7)));
        // 3 = Pushed; odtlačený na (7,7) shodí míč, odraz směr 7 (x−1) ⇒ (6,7) = uvolněné pole;
        // útočník následuje a chytá: 6
        $r = (new ActionResolver(new FixedDiceRoller([3, 7, 6, 6, 6, 6])))
            ->resolve($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);
        $s = $r->getNewState();
        $p = $s->requirePlayer(1)->requirePosition();
        $this->assertSame([6, 7], [$p->getX(), $p->getY()], 'fixtura: útočník následoval');
        $ball = $s->getBall();
        $this->assertFalse(!$ball->isHeld() && $ball->getPosition()?->equals($p), 'útočník nestojí na ležícím míči');
        $this->assertTrue($ball->isHeld() && $ball->getCarrierId() === 1, 'follow-up chytá (ř. 609-610)');
    }
}
