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
 * ⛔ VADA (11.09.2026, PHP15 b+d): PHP mělo Take Root špatně TŘEMI způsoby —
 *    přesně těmi, které C++ opravil 24.08. jako `TA2`
 *    (`engine/src/big_guy_handler.cpp:112-158`):
 *    (1) hod se házel **jen na MOVE**, ale r. 8573 říká „Immediately after
 *        declaring **an Action**" ⇒ Treeman blokoval bez rizika;
 *    (2) zakořenění **nepřetrvávalo** (r. 8575-8576: „his MA is considered 0
 *        **until a drive ends, or he is Knocked Down or Placed Prone**")
 *        ⇒ příště zase normálně chodil;
 *    (3) na 1 se blokovala **každá** akce, ale r. 8581-8582 blok výslovně
 *        **dovolují**.
 */
final class TakeRootPersistenceTest extends TestCase
{
    private function treeman(): \App\DTO\GameState
    {
        return (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, movement: 2, id: 1,
                        skills: [SkillName::TakeRoot])
            ->addPlayer(TeamSide::AWAY, 20, 7, id: 2)
            ->withBallOffPitch()
            ->build();
    }

    public function testRootingPersistsAndStopsFurtherMovement(): void
    {
        $state = $this->treeman();

        // SEBEKONTROLA: zatím zakořeněný není.
        $this->assertFalse($state->getPlayer(1)->isRooted(),
            'fixtura je vadná: hráč už je zakořeněný');

        $resolver = new ActionResolver(new FixedDiceRoller([1]));
        $result = $resolver->resolve($state, ActionType::MOVE, [
            'playerId' => 1, 'x' => 6, 'y' => 7,
        ]);

        $after = $result->getNewState()->getPlayer(1);

        $this->assertTrue($after->isRooted(),
            'zakořenění má přetrvat (r. 8575-8576)');
        $this->assertSame(0, $after->getMovementRemaining(),
            'MA je od té chvíle 0');
        $this->assertSame(5, $after->getPosition()->getX(),
            'zakořeněný se hnout nesmí');
    }

    public function testAlreadyRootedPlayerDoesNotRollAgain(): void
    {
        // ⭐ Bez tohohle by „přetrvává" znamenalo jen „zapsalo se to" --
        //    test ověřuje, že se stav i ČTE: hod se podruhé nehází.
        $state = $this->treeman();
        $state = $state->withPlayer($state->getPlayer(1)->withRooted(true));

        // Prázdná kostka: kdyby se hodilo, test spadne na „no more rolls".
        $resolver = new ActionResolver(new FixedDiceRoller([]));
        $result = $resolver->resolve($state, ActionType::MOVE, [
            'playerId' => 1, 'x' => 6, 'y' => 7,
        ]);

        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertNotContains('take_root', $types,
            'zakořeněný hráč už hod neopakuje');
    }

    public function testRootingOnBlitzStopsTheAction(): void
    {
        // r. 8582-8584: „if a player fails his Take Root roll as part of
        //   a **Blitz** Action he **may not block that turn**."
        $state = $this->treeman();

        $resolver = new ActionResolver(new FixedDiceRoller([1]));
        $result = $resolver->resolve($state, ActionType::BLITZ, [
            'playerId' => 1, 'targetId' => 2,
        ]);

        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('take_root', $types);
        $this->assertNotContains('block', $types,
            'po zakořenění v blitzu se blokovat NESMÍ');
        // A tým o deklarovaný blitz přijde (r. 351-352: limit visí na
        // DEKLARACI, ne na dokončení).
        $this->assertTrue(
            $result->getNewState()->getTeamState(TeamSide::HOME)->isBlitzUsedThisTurn(),
            'tým měl o deklarovaný blitz přijít',
        );
    }

    public function testRootingOnFoulDoesNotStopTheAction(): void
    {
        // r. 8581-8582 dovoluje blok; FOUL, PASS a HAND-OFF pohyb taky
        // nepotřebují, takže je zakořenění neblokuje.
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, movement: 2, id: 1,
                        skills: [SkillName::TakeRoot])
            ->addPlayer(TeamSide::AWAY, 6, 7, id: 2)
            ->withBallOffPitch()
            ->build();
        $state = $state->withPlayer(
            $state->getPlayer(2)->withState(\App\Enum\PlayerState::PRONE),
        );

        $resolver = new ActionResolver(new FixedDiceRoller([1, 3, 3, 3, 3]));
        $result = $resolver->resolve($state, ActionType::FOUL, [
            'playerId' => 1, 'targetId' => 2,
        ]);

        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('take_root', $types, 'hod se hází i u faulu');
        $this->assertContains('foul', $types,
            'zakořenění faulu nebrání -- pohyb k němu netřeba');
    }
}
