<?php
declare(strict_types=1);

namespace App\Tests\AI;

use App\AI\RandomAICoach;
use App\Engine\RulesEngine;
use App\Enum\ActionType;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use App\Tests\Engine\GameStateBuilder;
use PHPUnit\Framework\TestCase;

/**
 * ⛔ VADA (položka #2 auditu „kde engine ukončuje tah", 10.09.2026):
 *    `RandomAICoach::decideAction` losovala ze VŠECH akcí a teprve pak typ
 *    překládala přes `match` s `default => END_TURN`. Když los padl na typ,
 *    který kouč neumí postavit (TTM, bomba, gaze, Ball & Chain), ukončil se
 *    CELÝ TAH TÝMU.
 *
 * ⭐ `rules_bb2016.txt` r. 363-367 + uzavřený sedmičlenný seznam turnoverů
 *    (r. 368-384): „kouč neumí zvolený typ" v tom seznamu NENÍ.
 */
final class RandomCoachDoesNotEndTurnOnUnsupportedTypeTest extends TestCase
{
    public function testDoesNotEndTheTurnWhenAnUnsupportedTypeIsOffered(): void
    {
        // Hráč 1 má Hypnotic Gaze (typ, který kouč postavit NEUMÍ);
        // hráč 3 je běžný a MŮŽE se hýbat.
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, movement: 6, id: 1,
                        skills: [SkillName::HypnoticGaze])
            ->addPlayer(TeamSide::AWAY, 6, 7, id: 2)
            ->addPlayer(TeamSide::HOME, 10, 3, movement: 6, id: 3)
            ->withBallOffPitch()
            ->build();

        $rules = new RulesEngine();
        $offered = $rules->getAvailableActions($state);

        // SEBEKONTROLA FIXTURY: nepodporovaný typ je OPRAVDU v nabídce --
        // jinak se do opravované větve nedá dojít a test neměří nic.
        $types = array_column($offered, 'type');
        $this->assertContains(ActionType::HYPNOTIC_GAZE->value, $types,
            'fixtura je vadná: gaze se nenabízí, vada se nemůže projevit');
        // A je co hrát místo něj.
        $this->assertContains(ActionType::MOVE->value, $types,
            'fixtura je vadná: není čím gaze nahradit');

        // Losuje se, tak se to zkusí mnohokrát: dřív stačilo, aby los padl
        // na gaze, a kolo skončilo. Teď nesmí skončit ANI JEDNOU.
        $ai = new RandomAICoach();
        for ($i = 0; $i < 300; $i++) {
            $decision = $ai->decideAction($state, $rules);
            $this->assertNotSame(ActionType::END_TURN, $decision['action'],
                'kouč ukončil kolo, přestože bylo co hrát (pokus ' . $i . ')');
        }
    }

    public function testStillEndsTheTurnWhenNothingIsPlayable(): void
    {
        // ⛔ Druhá půlka páru: oprava NESMÍ vyrábět akci tam, kde žádná není.
        // Jediný hráč v týmu už jednal ⇒ nabídka nemá nic než END_TURN.
        // `GameStateBuilder::addPlayer` parametr `hasActed` nemá, tak se
        // nastaví až na hotovém stavu.
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, movement: 6, id: 1)
            ->addPlayer(TeamSide::AWAY, 20, 7, id: 2)
            ->withBallOffPitch()
            ->build();
        // MOVE se nabízí podle `canMove()` (tj. `hasMoved`), ostatní podle
        // `canAct()` (tj. `hasActed`) -- musí se nastavit OBOJÍ.
        $state = $state->withPlayer($state->getPlayer(1)->withHasActed(true)->withHasMoved(true));

        $rules = new RulesEngine();
        $offered = $rules->getAvailableActions($state);
        $playable = array_filter($offered,
            fn(array $a) => $a['type'] !== ActionType::END_TURN->value);
        $this->assertSame([], $playable,
            'fixtura je vadná: pořád je co hrát, tohle není ten případ');

        $decision = (new RandomAICoach())->decideAction($state, $rules);
        $this->assertSame(ActionType::END_TURN, $decision['action']);
    }
}
