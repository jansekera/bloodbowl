<?php
declare(strict_types=1);

namespace App\Tests\Engine;

use App\Engine\ActionResolver;
use App\Engine\FixedDiceRoller;
use App\Enum\ActionType;
use App\Enum\TeamSide;
use PHPUnit\Framework\TestCase;

/**
 * ⛔ VADA (11.09.2026): `HandOffHandler` vyhlašoval turnover hned, jak
 *    příjemce nechytil -- `$catchResult['success'] === false`.
 *
 * ⭐ `rules_bb2016.txt` r. 371-373 (bod 2 uzavřeného katalogu turnoverů):
 *    „A passed ball, or hand-off, is not caught by any member of the moving
 *    team **before the ball comes to rest**." A bod 3 (r. 376-378) dodává:
 *    „failing a catch roll, as opposed to a pick up, **is by itself never a
 *    turnover**."
 *    `resolveCatch` přitom míč po neúspěchu ODRAZÍ a odraz může skončit
 *    v rukou spoluhráče -- tým pak o kolo přijít nemá.
 */
final class HandOffBounceTest extends TestCase
{
    public function testBounceCaughtByTeammateIsNotATurnover(): void
    {
        // Podávající (5,7) -> příjemce (6,7). Příjemce nechytí, míč se
        // odrazí směrem 3 (dx=+1, dy=0) na (7,7), kde stojí spoluhráč.
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, agility: 3, id: 1)
            ->addPlayer(TeamSide::HOME, 6, 7, agility: 3, id: 2)
            ->addPlayer(TeamSide::HOME, 7, 7, agility: 3, id: 3)
            ->withBallCarried(1)
            ->build();
        // ⚠️ Bez tohohle spadne fixtura na vlastní sebekontrole: stavitel dává
        //    HOME 3 týmové rerolly, takže neúspěšné chycení se PŘEHODÍ a druhá
        //    kostka se spotřebuje na reroll, ne na odraz. (Přesně tak to
        //    poprvé spadlo.)
        $state = $state->withTeamState(TeamSide::HOME,
            $state->getTeamState(TeamSide::HOME)->withRerollUsed());

        // SEBEKONTROLA FIXTURY: míč nese podávající a spoluhráč stojí přesně
        // tam, kam odraz míří -- jinak test neměří, co má.
        $this->assertSame(1, $state->getBall()->getCarrierId(),
            'fixtura je vadná: míč nenese podávající');
        $this->assertSame(7, $state->getPlayer(3)->getPosition()->getX(),
            'fixtura je vadná: spoluhráč nestojí na poli odrazu');

        // chycení příjemcem: 1 (neúspěch; reroll je vyčerpaný, viz výš),
        // odraz D8 = 3 (na (7,7)), chycení spoluhráčem: 6 (úspěch)
        $dice = new FixedDiceRoller([1, 3, 6]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::HAND_OFF, [
            'playerId' => 1, 'targetId' => 2,
        ]);

        $ball = $result->getNewState()->getBall();
        // SEBEKONTROLA VÝSLEDKU: odraz opravdu skončil u spoluhráče.
        $this->assertTrue($ball->isHeld(), 'míč měl skončit v rukou, ne na zemi');
        $this->assertSame(3, $ball->getCarrierId(),
            'míč měl chytit spoluhráč na poli odrazu');

        $this->assertFalse($result->isTurnover(),
            'míč zůstal našemu týmu -- kolo končit nemá (r. 371-373, 376-378)');
    }

    public function testBallComingToRestOnTheGroundIsStillATurnover(): void
    {
        // ⛔ Druhá polovina páru: oprava NESMÍ turnover zrušit tam, kde patří.
        // Nikdo na poli odrazu nestojí -> míč skončí na zemi -> turnover.
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, agility: 3, id: 1)
            ->addPlayer(TeamSide::HOME, 6, 7, agility: 3, id: 2)
            ->withBallCarried(1)
            ->build();
        $state = $state->withTeamState(TeamSide::HOME,
            $state->getTeamState(TeamSide::HOME)->withRerollUsed());

        $this->assertNull($state->getPlayerAtPosition(new \App\ValueObject\Position(7, 7)),
            'fixtura je vadná: na poli odrazu někdo stojí, míč by se chytil');

        $dice = new FixedDiceRoller([1, 3, 6]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::HAND_OFF, [
            'playerId' => 1, 'targetId' => 2,
        ]);

        $this->assertFalse($result->getNewState()->getBall()->isHeld(),
            'míč měl skončit na zemi');
        $this->assertTrue($result->isTurnover(),
            'míč se zastavil nechycený -- to turnover JE (bod 2)');
    }
}
