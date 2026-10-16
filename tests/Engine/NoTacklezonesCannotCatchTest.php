<?php

declare(strict_types=1);

namespace App\Tests\Engine;

use App\DTO\BallState;
use App\Engine\BallResolver;
use App\Engine\FixedDiceRoller;
use App\Engine\ScatterCalculator;
use App\Engine\TacklezoneCalculator;
use App\Enum\TeamSide;
use App\ValueObject\Position;
use PHPUnit\Framework\TestCase;

/**
 * Audit parity 16.10.2026 (nález 19 z 08.10., v C++ `f64cb396`): hráč, který přišel o tackle zóny
 * (Bone-head, Really Stupid, Hypnotic Gaze), nechytá. `rules_bb2016.txt` ř. 7983-7986 (Bone-head):
 * „loses his tackle zones and may not catch, intercept or pass“; totéž ř. 8401-8405, 8185-8188.
 * PHP to hlídalo jen u zachycení (`PassResolver`), ne u chytání — ani přímo, ani při odrazu.
 */
final class NoTacklezonesCannotCatchTest extends TestCase
{
    /** @param list<int> $rolls */
    private function ballResolver(array $rolls): BallResolver
    {
        return new BallResolver(new FixedDiceRoller($rolls), new TacklezoneCalculator(), new ScatterCalculator());
    }

    public function testResolveCatchRefusesAPlayerWithoutTacklezones(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, agility: 6, id: 1)
            ->addPlayer(TeamSide::AWAY, 20, 7, id: 2)
            ->build();
        $state = $state->withPlayer($state->requirePlayer(1)->withLostTacklezones(true));
        $state = $state->withBall(BallState::onGround(new Position(5, 7)));

        // Jediná kostka = směr odrazu; hod na chycení by spotřeboval další a test by spadl.
        $result = $this->ballResolver([3])->resolveCatch($state, $state->requirePlayer(1));

        $this->assertFalse($result['success'], 'bez tackle zón chytit nesmí (ř. 7983-7986)');
        $this->assertFalse($result['state']->getBall()->isHeld());
    }

    public function testBounceOntoAPlayerWithoutTacklezonesBouncesOn(): void
    {
        // Míč odskočí z (5,7) směrem 3 (x+1) na (6,7), kde stojí hráč bez zón ⇒ neodchytí, odskočí dál.
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 6, 7, agility: 6, id: 1)
            ->addPlayer(TeamSide::AWAY, 20, 7, id: 2)
            ->build();
        $state = $state->withPlayer($state->requirePlayer(1)->withLostTacklezones(true));
        $state = $state->withBall(BallState::onGround(new Position(5, 7)));

        $result = $this->ballResolver([3, 6, 6, 3, 3, 3])->resolveBounce($state, new Position(5, 7));

        $ball = $result['state']->getBall();
        // SEBEKONTROLA: týž odraz na hráče SE zónami skončí v jeho rukou (kostky 3 = směr, 6 = chycení).
        $kontrola = $state->withPlayer($state->requirePlayer(1)->withLostTacklezones(false));
        $k = $this->ballResolver([3, 6, 6, 3, 3, 3])->resolveBounce($kontrola, new Position(5, 7))['state']->getBall();
        $this->assertTrue($k->isHeld() && $k->getCarrierId() === 1, 'pozitivní kontrola: se zónami chytá');
        $this->assertFalse($ball->isHeld() && $ball->getCarrierId() === 1, 'hráč bez zón odražený míč nechytí');
    }
}
