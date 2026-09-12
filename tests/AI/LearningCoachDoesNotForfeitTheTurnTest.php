<?php
declare(strict_types=1);

namespace App\Tests\AI;

use App\AI\LearningAICoach;
use App\Engine\RulesEngine;
use App\Enum\ActionType;
use App\Enum\TeamSide;
use App\Tests\Engine\GameStateBuilder;
use PHPUnit\Framework\TestCase;

/**
 * ⛔ VADA (PHP24, 11.09.2026) — MĚKČÍ TVAR TÉHOŽ, CO OPRAVIL `PHP13`:
 *    `LearningAICoach::decideAction` dávala END_TURN jako SKÓROVANÉHO
 *    KANDIDÁTA se skóre `baseScore - 0.01`. Nestačí tedy, aby se nic
 *    nepostavilo — stačí, aby všichni postavení kandidáti spadli pod tu
 *    hranici, a END_TURN **vyhraje skórem**, i když je co hrát.
 *
 * ⭐ ZMĚŘENO 11.09. (`cli/diag_ai_endturn_20260911.php`): 23 ze 7 666
 *    rozhodnutí = **1,2 % kol**, průměr **8,09 hráče** propadlo.
 *    ⚠️ A je to ten kouč, proti kterému hraje ČLOVĚK
 *    (`ServiceProvider.php:121-127`).
 *
 * ⭐ `rules_bb2016.txt` r. 363-367 + uzavřený katalog turnoverů r. 368-384:
 *    „kandidáti měli nízké skóre" v tom katalogu NENÍ.
 *
 * ⇒ Oprava je TÁŽ jako v C++ (`0630f854`), v `RandomAICoach` (`b0d01ccb`)
 *    a v `GreedyAICoach` (`f44270f5`): END_TURN až tehdy, když nabídka nic
 *    jiného nemá. Žádné nové skóre se nevymýšlí.
 */
final class LearningCoachDoesNotForfeitTheTurnTest extends TestCase
{
    public function testDoesNotEndTheTurnWhenEverythingScoresBelowEndTurn(): void
    {
        // ⭐ JAK SE TEN STAV VYROBÍ: `buildMoveAction` dává za pohyb
        //    `advancement * 0.1` a ODEČÍTÁ za riziko (`gfis * 0.08`) a za
        //    postranní čáru (y=0 → -0.2, y=1 → -0.05). Hráč HOME útočí na
        //    x=25 a UŽ V KONCOVÉ ZÓNĚ STOJÍ, v rohu, s **MA=0** a se spoluhráčem
        //    na (25,1). Každý dosažitelný cíl je tedy buď couvnutí
        //    (advancement < 0), nebo krok po koncové čáře — a VŽDY přes GFI,
        //    protože MA je 0. ⭐ To je podstatné od 12.09.: bezrizikový tah
        //    dostává bonus `RISK_FREE_BONUS`, takže s MA=1 by pole bez hodu
        //    skórovalo nad END_TURN a vada by se neprojevila. S MA=0 potřebuje
        //    hod každý cíl, bonus nedostane žádný, a nejlepší vyjde záporně.
        //    ⇒ Bez opravy vyhraje END_TURN, i když je co hrát.
        //    Míč mimo hřiště (žádné zvednutí), blitz vyčerpaný a soupeř na
        //    druhém konci hřiště (žádný blok) ⇒ v nabídce nezbude nic než
        //    MOVE. Spoluhráč už jednal, takže vlastní nabídku nemá.
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 25, 0, movement: 0, id: 1)
            ->addPlayer(TeamSide::HOME, 25, 1, movement: 6, id: 3)
            ->addPlayer(TeamSide::AWAY, 0, 7, id: 2)
            ->withBallOffPitch()
            ->build();
        $state = $state->withPlayer(
            $state->getPlayer(3)->withHasActed(true)->withHasMoved(true),
        );
        $state = $state->withTeamState(TeamSide::HOME,
            $state->getTeamState(TeamSide::HOME)->withBlitzUsed());

        $rules = new RulesEngine();
        $ai = new LearningAICoach();

        // --- SEBEKONTROLA FIXTURY: postavil jsem opravdu ten stav? ---
        // (a) je co hrát — nabídka MOVE dává.
        $types = array_column($rules->getAvailableActions($state), 'type');
        $this->assertContains(ActionType::MOVE->value, $types,
            'fixtura je vadná: nabídka nemá MOVE, hrát není co');
        // (b) a hráč má kam šlápnout.
        $this->assertNotSame([], $rules->getValidMoveTargets($state, 1),
            'fixtura je vadná: hráč nemá kam, vada se nemůže projevit');
        // (c) ⭐ KLÍČOVÉ TVRZENÍ: každý postavený kandidát má skóre POD
        //     hranicí END_TURN. Bez tohohle test projde i nad stavem, kde
        //     něco skóruje výš — a neměří pak vůbec nic.
        $evaluate = new \ReflectionMethod(LearningAICoach::class, 'evaluateState');
        $baseScore = (float) $evaluate->invoke($ai, $state, TeamSide::HOME);
        $endTurnScore = $baseScore - 0.01;

        $build = new \ReflectionMethod(LearningAICoach::class, 'buildScoredAction');
        $seenCandidate = false;
        foreach ($rules->getAvailableActions($state) as $offered) {
            $t = ActionType::from($offered['type']);
            $pid = $offered['playerId'] ?? null;
            if ($t === ActionType::END_TURN || $pid === null) {
                continue;
            }
            $built = $build->invoke($ai, $state, $rules, $t, (int) $pid, TeamSide::HOME, $baseScore);
            if ($built === null) {
                continue;
            }
            $seenCandidate = true;
            $this->assertLessThan($endTurnScore, $built['score'],
                'fixtura je vadná: ' . $t->value . ' skóruje nad END_TURN ('
                . $built['score'] . ' >= ' . $endTurnScore . '), vada se nemůže projevit');
        }
        $this->assertTrue($seenCandidate,
            'fixtura je vadná: nepostavil se ANI JEDEN kandidát — to je vada PHP13, ne PHP24');

        $decision = $ai->decideAction($state, $rules);

        $this->assertNotSame(ActionType::END_TURN, $decision['action'],
            'všichni kandidáti byli pod hranicí END_TURN, ale hrát bylo co -- kolo končit nesmí');
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
        $state = $state->withPlayer(
            $state->getPlayer(1)->withHasActed(true)->withHasMoved(true),
        );

        $rules = new RulesEngine();
        $playable = array_filter($rules->getAvailableActions($state),
            fn(array $a) => $a['type'] !== ActionType::END_TURN->value);
        $this->assertSame([], $playable,
            'fixtura je vadná: pořád je co hrát, tohle není ten případ');

        $decision = (new LearningAICoach())->decideAction($state, $rules);

        $this->assertSame(ActionType::END_TURN, $decision['action']);
    }
}
