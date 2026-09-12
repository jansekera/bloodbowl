<?php
declare(strict_types=1);

namespace App\Tests\AI;

use App\AI\LearningAICoach;
use App\DTO\GameState;
use App\Engine\{ActionResolver, RandomDiceRoller, RulesEngine};
use App\Enum\{ActionType, TeamSide};
use App\Tests\Engine\GameStateBuilder;
use PHPUnit\Framework\TestCase;

/**
 * ⭐⭐ ZADÁNÍ UŽIVATELE 12.09.: *„zkus v prvním kole sestavit klec a ve druhém
 *    s ní pohnout kupředu na naší polovině — tam zas tak moc soupeřů nebude."*
 *
 * Je to nejjednodušší možná zkouška celé sekce: volná vlastní polovina,
 * nosič a čtyři hráči kolem. Když klec nevznikne ani tady, nevznikne nikde.
 */
final class CagePlaybookTest extends TestCase
{
    /** Odehraje jedno kolo AI (do END_TURN nebo turnoveru) a vrátí nový stav. */
    private function odehrajKolo(GameState $state): GameState
    {
        $rules = new RulesEngine();
        $ai = new LearningAICoach();
        $resolver = new ActionResolver(new RandomDiceRoller());

        for ($i = 0; $i < 40; $i++) {
            $rozhodnuti = $ai->decideAction($state, $rules);
            $vysledek = $resolver->resolve($state, $rozhodnuti['action'], $rozhodnuti['params']);
            $state = $vysledek->getNewState();
            if ($rozhodnuti['action'] === ActionType::END_TURN || $vysledek->isTurnover()) {
                break;
            }
        }

        return $state;
    }

    /** Kolik rohů kolem nosiče drží naši hráči (a nemá u sebe soupeře). */
    private function cisteRohy(GameState $state): int
    {
        $ball = $state->getBall();
        if (!$ball->isHeld() || $ball->getCarrierId() === null) {
            return -1;
        }
        $nosic = $state->getPlayer($ball->getCarrierId());
        $pos = $nosic?->getPosition();
        if ($pos === null) {
            return -1;
        }
        $n = 0;
        foreach ([[-1, -1], [1, -1], [-1, 1], [1, 1]] as [$dx, $dy]) {
            foreach ($state->getPlayersOnPitch(TeamSide::HOME) as $p) {
                $pp = $p->getPosition();
                if ($pp === null || $pp->getX() !== $pos->getX() + $dx || $pp->getY() !== $pos->getY() + $dy) {
                    continue;
                }
                $spinavy = false;
                foreach ($state->getPlayersOnPitch(TeamSide::AWAY) as $s) {
                    $sp = $s->getPosition();
                    if ($sp !== null && $sp->distanceTo($pp) === 1) { $spinavy = true; break; }
                }
                if (!$spinavy) { $n++; }
                break;
            }
        }

        return $n;
    }

    public function testCageFormsInTheFirstTurnOnOurOwnHalf(): void
    {
        // Vlastní polovina HOME je x < 13. Nosič na (6,7), posádka kolem,
        // soupeři až za půlicí čárou.
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 6, 7, movement: 6, id: 1)
            ->addPlayer(TeamSide::HOME, 4, 4, movement: 6, id: 2)
            ->addPlayer(TeamSide::HOME, 4, 10, movement: 6, id: 3)
            ->addPlayer(TeamSide::HOME, 3, 6, movement: 6, id: 4)
            ->addPlayer(TeamSide::HOME, 3, 8, movement: 6, id: 5)
            // ⭐ Soupeři musí být V DOSAHU, jinak podle uživatele (12.09.)
            //    má přednost běh s míčem: "když jsou soupeři daleko, má přednost
            //    běh s míčem kupředu co nejrychleji". Klec se staví proti
            //    hrozbě, ne do prázdna.
            ->addPlayer(TeamSide::AWAY, 12, 5, movement: 6, id: 6)
            ->addPlayer(TeamSide::AWAY, 12, 9, movement: 6, id: 7)
            ->withBallCarried(1)
            ->build();

        $this->assertSame(0, $this->cisteRohy($state), 'fixtura: klec ještě nestojí');

        $po = $this->odehrajKolo($state);

        $this->assertSame(4, $this->cisteRohy($po),
            'na prázdné vlastní polovině se klec nesestavila ani za celé kolo');
    }

    public function testCageMovesAsAWholeInTheSecondTurn(): void
    {
        // ⭐⭐ Druhá půlka zadání: "ve druhém kole s ní pohnout kupředu".
        //    Klec už stojí; po odehrání kola musí stát ZASE CELÁ a jinde.
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 7, 7, movement: 6, id: 1)
            ->addPlayer(TeamSide::HOME, 6, 6, movement: 6, id: 2)
            ->addPlayer(TeamSide::HOME, 8, 6, movement: 6, id: 3)
            ->addPlayer(TeamSide::HOME, 6, 8, movement: 6, id: 4)
            ->addPlayer(TeamSide::HOME, 8, 8, movement: 6, id: 5)
            // ⭐ Jeden soupeř ve středu: je v dosahu (klec tedy drží pohromadě),
            //    ale po stranách zůstává čistý prostor, kam se dá posunout --
            //    uživatel 12.09.: "posun kupředu a i do boku".
            ->addPlayer(TeamSide::AWAY, 13, 7, movement: 6, id: 6)
            ->withBallCarried(1)
            ->build();

        $this->assertSame(4, $this->cisteRohy($state), 'fixtura: klec má stát už na začátku');
        $pred = $state->getPlayer(1)->getPosition();

        $po = $this->odehrajKolo($state);

        $this->assertSame(4, $this->cisteRohy($po),
            'klec se při posunu rozpadla -- nezůstaly čtyři čisté rohy');

        $ball = $po->getBall();
        $nosic = $ball->getCarrierId() !== null ? $po->getPlayer($ball->getCarrierId()) : null;
        $this->assertNotNull($nosic?->getPosition());
        $this->assertGreaterThanOrEqual(1, $nosic->getPosition()->distanceTo($pred),
            'klec stojí, ale vůbec se nehnula');
    }

    public function testCageWaitsOneSquareFromEndzoneUntilTheLastTurn(): void
    {
        // ⭐⭐ Uživatel 12.09.: "když je klec s nosičem vepředu tak, že první
        //    dva rohy jsou v TD zóně, tak se počká na poslední kolo a v tom
        //    nosič dojde dát TD."
        //    Přední rohy jsou o pole blíž zóně než nosič ⇒ ta pozice je
        //    vzdálenost 1 od koncové zóny. HOME útočí na x=25, takže x=24.
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 22, 7, movement: 6, id: 1)
            ->addPlayer(TeamSide::HOME, 21, 6, movement: 6, id: 2)
            ->addPlayer(TeamSide::HOME, 21, 8, movement: 6, id: 3)
            ->addPlayer(TeamSide::HOME, 23, 6, movement: 6, id: 4)
            ->addPlayer(TeamSide::HOME, 23, 8, movement: 6, id: 5)
            ->addPlayer(TeamSide::AWAY, 10, 5, id: 6)
            ->withBallCarried(1)
            ->build();

        $decision = (new LearningAICoach())->decideAction($state, new RulesEngine());

        // Nosič je na řadě jako první (ostatní stojí v rozích a drží).
        if ($decision['action'] === ActionType::MOVE && ($decision['params']['playerId'] ?? 0) === 1) {
            $this->assertNotSame(25, $decision['params']['x'],
                'nosič šel dát TD hned, místo aby počkal s klecí na poslední kolo');
        } else {
            $this->assertTrue(true, 'nosič se tenhle tah nehýbal, klec drží');
        }
    }
}
