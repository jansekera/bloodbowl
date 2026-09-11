<?php
declare(strict_types=1);

namespace App\Tests\Engine;

use App\Engine\ActionResolver;
use App\Engine\FixedDiceRoller;
use App\Engine\RulesEngine;
use App\Enum\ActionType;
use App\Enum\TeamSide;
use PHPUnit\Framework\TestCase;

/**
 * ⛔ VADA (11.09.2026, PHP21): hand-off neměl ŽÁDNÝ týmový limit.
 *    `RulesEngine` ho nabízel jen podle „nosič může jednat", a protože
 *    podávající dostal `hasActed`, ale **příjemce ne**, směl příjemce podat
 *    dál — a tak pořád dokola. Míč tedy mohl putovat **řetězem přes celé
 *    hřiště** v jednom kole.
 *
 * ⭐ BB2016 dává týmu jednu Hand-off Action za kolo, stejně jako jeden Blitz,
 *    Pass a Foul.
 * ⚠️ C++ engine má `handOffUsedThisTurn` od 17.08. (`f5998575`, položka P7);
 *    PHP kopie ho nedostala.
 */
final class HandOffOncePerTurnTest extends TestCase
{
    public function testSecondHandOffIsNotOfferedInTheSameTurn(): void
    {
        // Tři hráči v řadě: 1 (s míčem) -> 2 -> 3.
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, agility: 4, id: 1)
            ->addPlayer(TeamSide::HOME, 6, 7, agility: 4, id: 2)
            ->addPlayer(TeamSide::HOME, 7, 7, agility: 4, id: 3)
            ->withBallCarried(1)
            ->build();

        $rules = new RulesEngine();

        // SEBEKONTROLA FIXTURY: hand-off se PŘED akcí nabízí.
        $before = array_column($rules->getAvailableActions($state), 'type');
        $this->assertContains(ActionType::HAND_OFF->value, $before,
            'fixtura je vadná: hand-off se nenabízí ani napoprvé');

        // Chycení: 6 (úspěch)
        $resolver = new ActionResolver(new FixedDiceRoller([6]));
        $result = $resolver->resolve($state, ActionType::HAND_OFF, [
            'playerId' => 1, 'targetId' => 2,
        ]);

        $after = $result->getNewState();

        // SEBEKONTROLA VÝSLEDKU: míč OPRAVDU přešel na hráče 2, který ještě
        // nejednal -- bez toho by druhý hand-off nešel z jiného důvodu.
        $this->assertSame(2, $after->getBall()->getCarrierId(),
            'míč nepřešel, test neměří limit');
        $this->assertFalse($after->getPlayer(2)->hasActed(),
            'příjemce už jednal -- druhý hand-off by nešel i bez limitu');

        $types = array_column($rules->getAvailableActions($after), 'type');
        $this->assertNotContains(ActionType::HAND_OFF->value, $types,
            'druhý hand-off v témž kole -- míč by putoval řetězem');
    }

    public function testHandOffIsOfferedAgainInTheNextTurn(): void
    {
        // ⛔ Druhá polovina páru: limit se musí na novém kole UVOLNIT.
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, agility: 4, id: 1)
            ->addPlayer(TeamSide::HOME, 6, 7, agility: 4, id: 2)
            ->withBallCarried(1)
            ->build();
        $state = $state->withTeamState(TeamSide::HOME,
            $state->getTeamState(TeamSide::HOME)->withHandOffUsed());

        $rules = new RulesEngine();
        $this->assertNotContains(ActionType::HAND_OFF->value,
            array_column($rules->getAvailableActions($state), 'type'),
            'fixtura je vadná: limit není nastavený');

        $state = $state->withTeamState(TeamSide::HOME,
            $state->getTeamState(TeamSide::HOME)->resetForNewTurn());

        $this->assertContains(ActionType::HAND_OFF->value,
            array_column($rules->getAvailableActions($state), 'type'),
            'nové kolo musí hand-off zase dovolit');
    }
}
