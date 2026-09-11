<?php
declare(strict_types=1);

namespace App\Tests\Engine;

use App\DTO\BallState;
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
 * ⭐⭐⭐ TRVALÉ PRAVIDLO: `rules_bb2016.txt` r. 857-858 —
 *    „**Prone and Stunned players may never attempt to catch the ball.**"
 *
 * ⛔⛔ DO 11.09.2026 NA NĚJ NEBYL ANI JEDEN TEST *(1 012 testů v sadě)*.
 *    Drželo se jen tím, že si ho **deset volajících hlídalo samo**
 *    *(`resolveBounce:211`, throw-in `:258`, `PassResolver` 8×)* — a jedna
 *    cesta ho neměla: `resolveCatch` sama. Tou se dalo projít hand-offem
 *    a ležícímu hráči se házel hod na chycení, jako by stál.
 *    Nalezeno, protože uživatel na to pravidlo ukázal u Bloodlustu.
 *
 * ⇒ Tenhle soubor pravidlo zakotvuje na VÍCE CESTÁCH, ne jen na té opravené,
 *    aby se nemohlo tiše rozejít znovu.
 */
final class ProneCannotCatchTest extends TestCase
{
    private function ballResolver(array $rolls): BallResolver
    {
        $tz = new TacklezoneCalculator();

        return new BallResolver(new FixedDiceRoller($rolls), $tz, new ScatterCalculator());
    }

    public function testResolveCatchRefusesProneAndBouncesInstead(): void
    {
        // Cesta 1: přímé volání `resolveCatch` -- tudy chodí hand-off.
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, agility: 6, id: 1)
            ->addPlayer(TeamSide::AWAY, 20, 7, id: 2)
            ->build();
        $state = $state->withPlayer(
            $state->getPlayer(1)->withState(PlayerState::PRONE),
        );
        $state = $state->withBall(BallState::onGround(new Position(5, 7)));

        // SEBEKONTROLA: hráč leží, a s AG6 by jinak chytal skoro jistě --
        // takže když nechytí, je to tím pravidlem, ne hodem.
        $this->assertSame(PlayerState::PRONE, $state->getPlayer(1)->getState());
        $this->assertSame(6, $state->getPlayer(1)->getStats()->getAgility());

        // Jediná kostka = směr odrazu. Kdyby se házelo na chycení, spotřebuje
        // se na něj a test spadne na „no more rolls" -- to je tu taky tvrzení.
        $resolver = $this->ballResolver([3]);
        $result = $resolver->resolveCatch($state, $state->getPlayer(1));

        $this->assertFalse($result['success'], 'ležící chytit NESMÍ (r. 857-858)');
        $this->assertFalse($result['state']->getBall()->isHeld(),
            'míč se má odrazit, ne zůstat v rukou');
    }

    public function testStunnedIsRefusedToo(): void
    {
        // ⭐ Pravidlo jmenuje OBA stavy. Bez tohohle by stačilo hlídat PRONE.
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, agility: 6, id: 1)
            ->addPlayer(TeamSide::AWAY, 20, 7, id: 2)
            ->build();
        $state = $state->withPlayer(
            $state->getPlayer(1)->withState(PlayerState::STUNNED),
        );
        $state = $state->withBall(BallState::onGround(new Position(5, 7)));

        $resolver = $this->ballResolver([3]);
        $result = $resolver->resolveCatch($state, $state->getPlayer(1));

        $this->assertFalse($result['success'], 'omráčený chytit NESMÍ (r. 857-858)');
    }

    public function testStandingPlayerStillCatches(): void
    {
        // ⛔ Druhá polovina páru: stráž nesmí zakázat chytání i stojícím.
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, agility: 6, id: 1)
            ->addPlayer(TeamSide::AWAY, 20, 7, id: 2)
            ->build();
        $state = $state->withBall(BallState::onGround(new Position(5, 7)));

        $this->assertTrue($state->getPlayer(1)->getState()->canAct(),
            'fixtura je vadná: hráč nestojí');

        $resolver = $this->ballResolver([6]);
        $result = $resolver->resolveCatch($state, $state->getPlayer(1));

        $this->assertTrue($result['success'], 'stojící hráč chytat MÁ');
        $this->assertSame(1, $result['state']->getBall()->getCarrierId());
    }

    public function testBounceOntoProneKeepsBouncing(): void
    {
        // Cesta 2: odraz. `resolveBounce` si stráž hlídal sám už předtím --
        // tenhle test to zakotvuje, aby se to nedalo omylem zrušit.
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, agility: 6, id: 1)
            ->addPlayer(TeamSide::AWAY, 20, 7, id: 2)
            ->build();
        $state = $state->withPlayer(
            $state->getPlayer(1)->withState(PlayerState::PRONE),
        );

        // Odraz z (4,7) směrem 3 (dx +1) => na (5,7), kde LEŽÍ hráč 1.
        $resolver = $this->ballResolver([3, 3]);
        $result = $resolver->resolveBounce($state, new Position(4, 7));

        $ball = $result['state']->getBall();
        $this->assertFalse($ball->isHeld(),
            'odraz na ležícího ho do rukou dát nesmí (r. 857-858)');
    }

    public function testPassToProneReceiverIsNotCaught(): void
    {
        // Cesta 3: přihrávka. `PassResolver` si stráž hlídá sám (8 míst) --
        // zakotveno, ať se to nerozejde.
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, agility: 6, id: 1)
            ->addPlayer(TeamSide::HOME, 8, 7, agility: 6, id: 2)
            ->addPlayer(TeamSide::AWAY, 20, 7, id: 3)
            ->withBallCarried(1)
            ->build();
        $state = $state->withPlayer(
            $state->getPlayer(2)->withState(PlayerState::PRONE),
        );
        $state = $state->withTeamState(TeamSide::HOME,
            $state->getTeamState(TeamSide::HOME)->withRerollUsed());

        $resolver = new ActionResolver(new FixedDiceRoller([6, 6, 3, 3, 3, 3]));
        $result = $resolver->resolve($state, ActionType::PASS, [
            'playerId' => 1, 'targetX' => 8, 'targetY' => 7,
        ]);

        $this->assertNotSame(2, $result->getNewState()->getBall()->getCarrierId(),
            'ležící příjemce míč chytit nesmí (r. 857-858)');
    }
}
