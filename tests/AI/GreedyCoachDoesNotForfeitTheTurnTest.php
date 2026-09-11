<?php
declare(strict_types=1);

namespace App\Tests\AI;

use App\AI\GreedyAICoach;
use App\Engine\RulesEngine;
use App\Enum\ActionType;
use App\Enum\TeamSide;
use App\Tests\Engine\GameStateBuilder;
use PHPUnit\Framework\TestCase;

/**
 * ⛔ VADA (položka #1 auditu „kde engine ukončuje tah", 10.09.2026):
 *    `GreedyAICoach::decideAction` měla `$bestAction = END_TURN` jako
 *    VÝCHOZÍ HODNOTU. Stačilo, aby `scoreAction` vrátil `null` pro všechny
 *    nabídnuté akce, a ukončilo se kolo CELÉHO TÝMU. `null` přitom vrací
 *    snadno: `scoreMove` zahazuje tah se skóre `<= 0`.
 *
 * ⭐ ZMĚŘENO (60 her, 12 135 rozhodnutí): 0,6 % rozhodnutí, ale **3,7 % KOL**,
 *    průměr **7,19 hráče** propadlo, maximum **11 = celý tým**.
 *
 * ⭐ `rules_bb2016.txt` r. 363-367 + uzavřený katalog turnoverů r. 368-384:
 *    „skórer nic neohodnotil" v tom katalogu NENÍ.
 */
final class GreedyCoachDoesNotForfeitTheTurnTest extends TestCase
{
    public function testDoesNotEndTheTurnWhenNothingScoresButSomethingIsPlayable(): void
    {
        // ⭐ JAK SE TEN STAV VYROBÍ: `scoreMove` dá kladné skóre jen za
        //    POSTUP VPŘED (`50 + advancement*10`), za zvednutí míče nebo za
        //    postavení se ke svému nosiči. Hráč HOME útočí na x=25 -- když
        //    UŽ V KONCOVÉ ZÓNĚ STOJÍ, je `advancement <= 0` pro každé pole,
        //    žádná větev nesepne, skóre zůstane 0 a `scoreMove` vrátí `null`
        //    (zahazuje `<= 0`). Míč je mimo hřiště a blitz vyčerpaný, takže
        //    v nabídce nezůstane nic jiného než MOVE.
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 25, 7, movement: 6, id: 1)
            ->addPlayer(TeamSide::AWAY, 0, 7, id: 2)
            ->withBallOffPitch()
            ->build();
        $state = $state->withTeamState(TeamSide::HOME,
            $state->getTeamState(TeamSide::HOME)->withBlitzUsed());

        $rules = new RulesEngine();
        $ai = new GreedyAICoach();

        // --- SEBEKONTROLA FIXTURY: postavil jsem opravdu ten stav? ---
        // (a) je co hrát -- nabídka MOVE dává.
        $types = array_column($rules->getAvailableActions($state), 'type');
        $this->assertContains(ActionType::MOVE->value, $types,
            'fixtura je vadná: nabídka nemá MOVE, hrát není co');
        // (b) a hráč má kam šlápnout.
        $this->assertNotSame([], $rules->getValidMoveTargets($state, 1),
            'fixtura je vadná: hráč nemá kam, vada se nemůže projevit');
        // (c) ⭐ KLÍČOVÉ: skórer NEOHODNOTÍ ANI JEDNU nabídnutou akci.
        //     Bez tohohle tvrzení test projde i nad stavem, kde skórer něco
        //     najde -- a neměří pak vůbec nic. (Přesně tak vypadala první
        //     verze tohohle testu: prošla i s odstraněnou opravou.)
        $ref = new \ReflectionMethod(GreedyAICoach::class, 'scoreAction');
        foreach ($rules->getAvailableActions($state) as $offered) {
            $t = ActionType::from($offered['type']);
            if ($t === ActionType::END_TURN) {
                continue;
            }
            $pid = $offered['playerId'] ?? null;
            if ($pid === null) {
                continue;
            }
            $this->assertNull(
                $ref->invoke($ai, $state, $rules, $t, (int) $pid, TeamSide::HOME),
                'fixtura je vadná: skórer ohodnotil ' . $t->value
                . ', do opravované větve se nedojde',
            );
        }

        $decision = $ai->decideAction($state, $rules);

        $this->assertNotSame(ActionType::END_TURN, $decision['action'],
            'skórer neohodnotil nic, ale hrát bylo co -- kolo končit nesmí');
        $this->assertSame(ActionType::MOVE, $decision['action']);
        $this->assertSame(1, $decision['params']['playerId']);
    }

    public function testStillEndsTheTurnWhenNothingIsPlayable(): void
    {
        // ⛔ Druhá polovina páru: oprava NESMÍ vyrábět akci tam, kde žádná
        //    není. Jediný hráč už jednal ⇒ nabídka má jen END_TURN.
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, movement: 6, id: 1)
            ->addPlayer(TeamSide::AWAY, 20, 7, id: 2)
            ->withBallOffPitch()
            ->build();
        // MOVE se nabízí podle `canMove()` (`hasMoved`), ostatní podle
        // `canAct()` (`hasActed`) -- musí se nastavit OBOJÍ.
        $state = $state->withPlayer(
            $state->getPlayer(1)->withHasActed(true)->withHasMoved(true),
        );

        $rules = new RulesEngine();
        $playable = array_filter($rules->getAvailableActions($state),
            fn(array $a) => $a['type'] !== ActionType::END_TURN->value);
        $this->assertSame([], $playable,
            'fixtura je vadná: pořád je co hrát, tohle není ten případ');

        $decision = (new GreedyAICoach())->decideAction($state, $rules);

        $this->assertSame(ActionType::END_TURN, $decision['action']);
    }

    public function testBallAndChainPlayerIsNeverLeftIdle(): void
    {
        // Ball & Chain je pro takového hráče jediná povolená akce; skórer ji
        // ohodnotí, ale i kdyby ne, záchranná větev ji umí postavit.
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, movement: 3, id: 1,
                        skills: [\App\Enum\SkillName::BallAndChain])
            ->addPlayer(TeamSide::AWAY, 20, 7, id: 2)
            ->withBallOffPitch()
            ->build();

        $decision = (new GreedyAICoach())->decideAction($state, new RulesEngine());

        $this->assertSame(ActionType::BALL_AND_CHAIN, $decision['action']);
        $this->assertSame(1, $decision['params']['playerId']);
    }
}
