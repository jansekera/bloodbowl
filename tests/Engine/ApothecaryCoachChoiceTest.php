<?php

declare(strict_types=1);

namespace App\Tests\Engine;

use App\DTO\GameState;
use App\Engine\ActionResolver;
use App\Engine\FixedDiceRoller;
use App\Engine\RulesEngine;
use App\Enum\ActionType;
use App\Enum\PlayerState;
use App\Enum\TeamSide;
use PHPUnit\Framework\TestCase;

/**
 * P206 (b), uživatel 16.10.2026: „lékárník se má ptát hráče na vše“. `rules_bb2016.txt` ř. 1200-1210:
 * KO ⇒ „leave him on the pitch Stunned, or in the Reserves box if not on the pitch“; Casualty ⇒ soupeř
 * hodí znovu na tabulku a kouč vybere jeden z výsledků; Badly Hurt ⇒ rezervy. Lékárník jednou za zápas.
 * Volba se nabídne PO akci, ve které ke zranění došlo (akce doběhne celá), jen kouči-člověku.
 */
final class ApothecaryCoachChoiceTest extends TestCase
{
    private function state(): GameState
    {
        return (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 7, id: 2)
            ->withBallOffPitch()
            ->build();
    }

    /** @param list<int> $dice @param array<string, mixed> $params */
    private function resolver(array $dice): ActionResolver
    {
        $r = new ActionResolver(new FixedDiceRoller($dice));
        $r->setInteractiveApothecary([TeamSide::HOME]);
        return $r;
    }

    public function testAKnockedOutPlayerIsOfferedTheApothecaryAndReturnsStunned(): void
    {
        // Attacker Down (1), brnění 6+6 = 12 > 8, zranění 4+4 = 8 ⇒ KO
        $r = $this->resolver([1, 6, 6, 4, 4])->resolve($this->state(), ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);
        $s = $r->getNewState();
        $this->assertSame(PlayerState::KO, $s->requirePlayer(1)->getState(), 'fixtura: KO');
        $this->assertSame(1, $s->getPlayerAtPosition(new \App\ValueObject\Position(5, 7))?->getId(), 'do volby drží své pole — nikdo jiný tam nevstoupí');
        $this->assertNotNull($s->getPendingApothecary(), 'lékárník se nabízí');
        $this->assertSame([ActionType::RESOLVE_APOTHECARY->value], array_column((new RulesEngine())->getAvailableActions($s), 'type'));

        $u = $this->resolver([])->resolve($s, ActionType::RESOLVE_APOTHECARY, ['playerId' => 1]);
        $p = $u->getNewState()->requirePlayer(1);
        $this->assertSame(PlayerState::STUNNED, $p->getState(), 'KO s lékárníkem ⇒ omráčený na hřišti');
        $this->assertSame([5, 7], [$p->requirePosition()->getX(), $p->requirePosition()->getY()]);
        $this->assertFalse($u->getNewState()->getTeamState(TeamSide::HOME)->canUseApothecary(), 'jednou za zápas');
        $this->assertTrue($u->isTurnover(), 'turnover z akce platí až po volbě');
    }

    public function testDecliningKeepsTheKnockOutAndTheApothecary(): void
    {
        $s = $this->resolver([1, 6, 6, 4, 4])->resolve($this->state(), ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2])->getNewState();
        $u = $this->resolver([])->resolve($s, ActionType::RESOLVE_APOTHECARY, ['playerId' => 0]);
        $this->assertSame(PlayerState::KO, $u->getNewState()->requirePlayer(1)->getState());
        $this->assertNull($u->getNewState()->requirePlayer(1)->getPosition(), 'nepoužito ⇒ teprve teď z hřiště');
        $this->assertTrue($u->getNewState()->getTeamState(TeamSide::HOME)->canUseApothecary());
        $this->assertNull($u->getNewState()->getPendingApothecary());
    }

    public function testACasualtyRollsAgainAndTheCoachChoosesTheResult(): void
    {
        // Attacker Down, brnění 12, zranění 6+6 = 12 ⇒ Casualty: 6/1 = DEAD
        $s = $this->resolver([1, 6, 6, 6, 6, 6, 1])->resolve($this->state(), ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2])->getNewState();
        $this->assertNotNull($s->getPendingApothecary());
        // použít: druhý hod 1/1 = Badly Hurt ⇒ volba ze dvou
        $s2 = $this->resolver([1, 1])->resolve($s, ActionType::RESOLVE_APOTHECARY, ['playerId' => 1])->getNewState();
        $pending = $s2->getPendingApothecary();
        $this->assertNotNull($pending, 'druhá otázka: který výsledek');
        $this->assertSame(['dead', 'badly_hurt'], $pending->getOptions());
        $u = $this->resolver([])->resolve($s2, ActionType::RESOLVE_APOTHECARY, ['result' => 'badly_hurt']);
        $this->assertSame(PlayerState::OFF_PITCH, $u->getNewState()->requirePlayer(1)->getState(), 'Badly Hurt ⇒ rezervy');
        $this->assertNull($u->getNewState()->getPendingApothecary());
    }

    public function testWithoutInteractiveApothecaryNothingIsAsked(): void
    {
        $s = (new ActionResolver(new FixedDiceRoller([1, 6, 6, 4, 4])))
            ->resolve($this->state(), ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2])->getNewState();
        $this->assertNull($s->getPendingApothecary(), 'AI / simulace se neptá');
    }
}
