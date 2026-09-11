<?php
declare(strict_types=1);

namespace App\Tests\AI;

use App\AI\GreedyAICoach;
use App\AI\LearningAICoach;
use App\Engine\ActionResolver;
use App\Engine\FixedDiceRoller;
use App\Engine\RulesEngine;
use App\Enum\ActionType;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use App\Tests\Engine\GameStateBuilder;
use PHPUnit\Framework\TestCase;

/**
 * ⛔ VADA (11.09.2026): OBA kouči stavěli akci Ball & Chain s `'params' => []`,
 *    tedy BEZ `playerId`. `BallAndChainHandler:36` dělá
 *    `(int) $params['playerId']` → chybějící klíč → `(int) null` = 0 →
 *    `getPlayer(0)` = null → `throw InvalidArgumentException('Player not found')`.
 *
 * ⭐ NENÍ TO OKRAJOVÝ PŘÍPAD: Ball & Chain je pro takového hráče JEDINÁ
 *    povolená akce (nabídka mu žádnou jinou nedá), takže se nemohl pohnout
 *    NIKDY. A živá cesta (`AITurnService::playTurn`) výjimku nechytá.
 */
final class BallAndChainActionTest extends TestCase
{
    private function stateWithFanatic(): \App\DTO\GameState
    {
        return (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, movement: 3, id: 1,
                        skills: [SkillName::BallAndChain])
            ->addPlayer(TeamSide::AWAY, 20, 7, id: 2)
            ->withBallOffPitch()
            ->build();
    }

    public function testGreedyGivesBallAndChainActionAPlayerId(): void
    {
        $state = $this->stateWithFanatic();
        $rules = new RulesEngine();

        // SEBEKONTROLA FIXTURY: nabídka tomu hráči opravdu NIC JINÉHO nedává.
        // Bez toho by test mohl projít nad stavem, kde kouč zvolí jinou akci.
        $offered = array_values(array_filter(
            $rules->getAvailableActions($state),
            fn(array $a) => ($a['playerId'] ?? null) === 1,
        ));
        $this->assertNotSame([], $offered, 'fixtura je vadná: hráč nemá co hrát');
        foreach ($offered as $a) {
            $this->assertSame(ActionType::BALL_AND_CHAIN->value, $a['type'],
                'fixtura je vadná: nabízí se i něco jiného než B&C');
        }

        $decision = (new GreedyAICoach())->decideAction($state, $rules);

        $this->assertSame(ActionType::BALL_AND_CHAIN, $decision['action']);
        $this->assertArrayHasKey('playerId', $decision['params'],
            'bez playerId spadne handler na "Player not found"');
        $this->assertSame(1, $decision['params']['playerId']);
    }

    public function testLearningGivesBallAndChainActionAPlayerId(): void
    {
        $state = $this->stateWithFanatic();
        $rules = new RulesEngine();

        $decision = (new LearningAICoach(null, 0.0))->decideAction($state, $rules);

        $this->assertSame(ActionType::BALL_AND_CHAIN, $decision['action']);
        $this->assertArrayHasKey('playerId', $decision['params']);
        $this->assertSame(1, $decision['params']['playerId']);
    }

    public function testBallAndChainActionActuallyResolves(): void
    {
        // ⭐ Druhá půlka: nestačí, že se `playerId` posílá -- resolver to
        //    musí PŘIJMOUT. Tohle je to, co v živé hře padalo.
        $state = $this->stateWithFanatic();
        $rules = new RulesEngine();
        $decision = (new GreedyAICoach())->decideAction($state, $rules);

        $dice = new FixedDiceRoller([1, 1, 1, 4, 4, 4, 4, 4, 4, 4, 4, 4]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, $decision['action'], $decision['params']);

        $this->assertTrue($result->isSuccess(),
            'akce Ball & Chain musí projít resolverem, ne spadnout na výjimku');
    }
}
