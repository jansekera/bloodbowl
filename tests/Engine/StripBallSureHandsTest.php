<?php

declare(strict_types=1);

namespace App\Tests\Engine;

use App\DTO\ActionResult;
use App\Engine\ActionResolver;
use App\Engine\FixedDiceRoller;
use App\Enum\ActionType;
use App\Enum\PlayerState;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use PHPUnit\Framework\TestCase;

/**
 * SURE HANDS RUŠÍ STRIP BALL (audit parity 08.10.2026, nález 10).
 *
 * `rules_bb2016.txt` ř. 8544-8546 (Sure Hands): "In addition, the Strip Ball skill **will
 * not work against a player with this skill**." Totéž ř. 973-976.
 * Strip Ball ř. 8518-8521: "applying a 'Pushed' or 'Defender Stumbles' result will cause
 * the opposing player to drop the ball in the square that they are pushed to, even if the
 * opposing player is not Knocked Down."
 *
 * Stará mechanika: Sure Hands se v bloku nekontrolovalo -- nosič o míč přišel pouhým
 * odtlačením.
 *
 * Kostky: 3 = Pushed (1 kostka). Další kostka (D8 = 1) je odskok vyraženého míče; hodí ji
 * jen větev, kde Strip Ball platí.
 */
final class StripBallSureHandsTest extends TestCase
{
    /** @param list<SkillName> $nosic */
    private function blok(array $nosic): ActionResult
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, skills: [SkillName::StripBall], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 7, skills: $nosic, id: 2)
            ->withBallCarried(2)
            ->build();

        return (new ActionResolver(new FixedDiceRoller([3, 1])))
            ->resolve($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);
    }

    /** @return list<string> */
    private function typy(ActionResult $r): array
    {
        return array_map(fn($e) => $e->getType(), $r->getEvents());
    }

    public function testOdtlacenyNosicSeSureHandsMicDrzi(): void
    {
        $r = $this->blok([SkillName::SureHands]);
        $po = $r->getNewState();

        $this->assertContains('push', $this->typy($r), 'fixtura: nosič byl odtlačen');
        $this->assertNotContains('strip_ball', $this->typy($r), 'ř. 8545-8546');
        $this->assertSame(2, $po->getBall()->getCarrierId(), 'ř. 8545-8546: Strip Ball proti Sure Hands neplatí');
        $this->assertTrue($po->getBall()->getPosition()?->equals($po->requirePlayer(2)->requirePosition()), 'míč jde s nosičem');
        $this->assertSame(PlayerState::STANDING, $po->requirePlayer(2)->getState());
    }

    public function testKontrolaBezSureHandsStripBallMicVyrazi(): void
    {
        $r = $this->blok([]);

        $this->assertContains('strip_ball', $this->typy($r), 'ř. 8518-8521');
        $this->assertNotSame(2, $r->getNewState()->getBall()->getCarrierId());
    }
}
