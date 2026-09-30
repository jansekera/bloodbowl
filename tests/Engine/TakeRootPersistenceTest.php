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
        $this->assertFalse($state->requirePlayer(1)->isRooted(),
            'fixtura je vadná: hráč už je zakořeněný');

        $resolver = new ActionResolver(new FixedDiceRoller([1]));
        $result = $resolver->resolve($state, ActionType::MOVE, [
            'playerId' => 1, 'x' => 6, 'y' => 7,
        ]);

        $after = $result->getNewState()->requirePlayer(1);

        $this->assertTrue($after->isRooted(),
            'zakořenění má přetrvat (r. 8575-8576)');
        $this->assertSame(0, $after->getMovementRemaining(),
            'MA je od té chvíle 0');
        $this->assertSame(5, $after->requirePosition()->getX(),
            'zakořeněný se hnout nesmí');
    }

    public function testAlreadyRootedPlayerDoesNotRollAgain(): void
    {
        // ⭐ Bez tohohle by „přetrvává" znamenalo jen „zapsalo se to" --
        //    test ověřuje, že se stav i ČTE: hod se podruhé nehází.
        // ⛔ 30.09.2026 (P83): test dřív POSÍLAL zakořeněného o pole dál --
        //    fixtura měla kořeny a MA 2, což pravidla vylučují (r. 8575-8580).
        //    Ověřuje se teď přes BLOK, který zakořeněný smí (r. 8580-8581).
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, movement: 2, id: 1,
                        skills: [SkillName::TakeRoot])
            ->addPlayer(TeamSide::AWAY, 6, 7, id: 2)
            ->withBallOffPitch()
            ->build();
        $state = $state->withPlayer(
            $state->requirePlayer(1)->withRooted(true)->withMovementRemaining(0),
        );

        // Jen kostka bloku (3 = PUSHED): kdyby se házel Take Root, spotřebuje
        // ji on a blok spadne na „no more rolls".
        $resolver = new ActionResolver(new FixedDiceRoller([3]));
        $result = $resolver->resolve($state, ActionType::BLOCK, [
            'playerId' => 1, 'targetId' => 2,
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
            $state->requirePlayer(2)->withState(\App\Enum\PlayerState::PRONE),
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

    /**
     * 30.09.2026 (kontrola na zadost uzivatele, port C++ N14 z 01.09.):
     * r. 8583-8584 konci zavorkou „(he can still roll to stand up if he is
     * Prone)". Lezici Treeman, ktery pri ohlaseni pohybu hodi na Take Root 1,
     * se zakoreni, ale VSTAVAT smi (MA 0 < 3 => hod 4+, r. 690-695).
     */
    public function testRootedWhileProneMayStillRollToStandUp(): void
    {
        $state = $this->treeman();
        $state = $state->withPlayer(
            $state->requirePlayer(1)->withState(\App\Enum\PlayerState::PRONE),
        );

        // 1 = zakoreni · 4 = vstani na 4+
        $resolver = new ActionResolver(new FixedDiceRoller([1, 4]));
        $result = $resolver->resolve($state, ActionType::MOVE, [
            'playerId' => 1, 'x' => 5, 'y' => 7,
        ]);

        $after = $result->getNewState()->requirePlayer(1);
        $this->assertFalse($result->isTurnover());
        $this->assertTrue($after->isRooted(), 'hod na Take Root se pri vstavani nehodil');
        $this->assertSame(\App\Enum\PlayerState::STANDING, $after->getState(),
            'zakoreneni zabranilo vstani, ackoli r. 8583-8584 ho vyslovne dovoluje');
    }

    /**
     * P83 (30.09.2026), port C++ 6e2f084c. r. 8577-8580: zakoreneny „may not
     * Go For It ... or use any skill that would allow him to move out of his
     * current square". PHP pathfinder mu daval 2 GFI.
     */
    public function testRootedPlayerHasNoMovesNotEvenGfi(): void
    {
        $state = $this->treeman();
        $state = $state->withPlayer(
            $state->requirePlayer(1)->withRooted(true)->withMovementRemaining(0),
        );
        $moves = (new \App\Engine\Pathfinder())->findValidMoves($state, $state->requirePlayer(1));
        $this->assertSame([], $moves, 'zakoreneny ma nabidnute GFI');
    }

    /**
     * P83: „may block adjacent players WITHOUT FOLLOWING-UP" (r. 8580-8581).
     */
    public function testRootedAttackerDoesNotFollowUp(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, movement: 2, strength: 6, id: 1,
                        skills: [SkillName::TakeRoot])
            ->addPlayer(TeamSide::AWAY, 6, 7, id: 2)
            ->withBallOffPitch()
            ->build();
        $state = $state->withPlayer($state->requirePlayer(1)->withRooted(true)->withMovementRemaining(0));

        // 3 kostky (ST6 proti 3), vse PUSHED
        $resolver = new ActionResolver(new FixedDiceRoller([3, 3, 3, 3, 3, 3]));
        $result = $resolver->resolve($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);

        $this->assertSame(5, $result->getNewState()->requirePlayer(1)->requirePosition()->getX(),
            'zakoreneny sel follow-upem za odtlacenym');
    }
}
