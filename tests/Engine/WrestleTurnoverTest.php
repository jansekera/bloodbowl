<?php
declare(strict_types=1);

namespace App\Tests\Engine;

use App\Engine\ActionResolver;
use App\Engine\FixedDiceRoller;
use App\Enum\ActionType;
use App\Enum\PlayerState;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use PHPUnit\Framework\TestCase;

/**
 * ⛔ VADA (11.09.2026): `BlockHandler` vracel po Wrestle vždycky `success`.
 *
 * ⭐ `rules_bb2016.txt` r. 8677-8678: „Use of this skill does not cause
 *    a turnover **unless the active player was holding the ball**."
 *    Katalog turnoverů r. 368-372 to říká stejně -- Wrestle se počítá jako
 *    „Placed Prone", a to turnover NENÍ, **pokud hráč nedrží míč**.
 *
 * ⚠️ C++ engine to opravil už 24.08.2026 jako `F11`
 *    (`engine/src/block_handler.cpp:826-848`); PHP kopie opravu nedostala.
 */
final class WrestleTurnoverTest extends TestCase
{
    private function wrestleState(bool $attackerCarriesBall): \App\DTO\GameState
    {
        $b = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, strength: 3, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 7, strength: 3, id: 2,
                        skills: [SkillName::Wrestle]);
        $b = $attackerCarriesBall ? $b->withBallCarried(1) : $b->withBallOffPitch();

        return $b->build();
    }

    public function testWrestleWithoutTheBallIsNotATurnover(): void
    {
        $state = $this->wrestleState(attackerCarriesBall: false);

        // SEBEKONTROLA FIXTURY: obránce Wrestle MÁ a útočník míč NEMÁ.
        $this->assertTrue($state->getPlayer(2)->hasSkill(SkillName::Wrestle),
            'fixtura je vadná: nikdo nemá Wrestle, větev se nespustí');
        $this->assertFalse($state->getBall()->isHeld(),
            'fixtura je vadná: míč někdo drží, tohle není ten případ');

        // Both Down (die = 2), rovná síla => 1 kostka, útočník vybírá
        $dice = new FixedDiceRoller([2]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::BLOCK, [
            'playerId' => 1, 'targetId' => 2,
        ]);

        // SEBEKONTROLA VÝSLEDKU: Wrestle se OPRAVDU spustil.
        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('wrestle', $types,
            'Wrestle se nespustil -- test neměří, co má');
        $this->assertSame(PlayerState::PRONE, $result->getNewState()->getPlayer(1)->getState());
        $this->assertSame(PlayerState::PRONE, $result->getNewState()->getPlayer(2)->getState());

        $this->assertFalse($result->isTurnover(),
            'Placed Prone bez míče turnover NENÍ (r. 8677-8678)');
    }

    public function testWrestleWhileHoldingTheBallIsATurnover(): void
    {
        // ⭐ Druhá půlka páru -- a právě tahle vada tam byla.
        $state = $this->wrestleState(attackerCarriesBall: true);

        $this->assertSame(1, $state->getBall()->getCarrierId(),
            'fixtura je vadná: míč nenese aktivní hráč, pár nic neměří');

        $dice = new FixedDiceRoller([2, 4, 4, 4, 4]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::BLOCK, [
            'playerId' => 1, 'targetId' => 2,
        ]);

        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('wrestle', $types, 'Wrestle se nespustil');

        $this->assertTrue($result->isTurnover(),
            'aktivní hráč s míčem jde na zem -- to turnover JE (r. 8677-8678)');
    }
}
