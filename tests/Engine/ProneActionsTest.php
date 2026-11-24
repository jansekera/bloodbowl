<?php

declare(strict_types=1);

namespace App\Tests\Engine;

use App\DTO\GameState;
use App\DTO\TeamStateDTO;
use App\Engine\ActionResolver;
use App\Engine\FixedDiceRoller;
use App\Engine\RulesEngine;
use App\Enum\ActionType;
use App\Enum\PlayerState;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use PHPUnit\Framework\TestCase;

/**
 * P316 (uživatel 24.11.2026: „1 vše, 2 ano, 3 ano“) — `rules_bb2016.txt` ř. 670-676: ležící „may do nothing before
 * standing up at a cost of three squares of his movement when he next takes an Action … The player may take any
 * Action other than a Block Action.“ Ř. 690-695: pod 3 MA vstane na 4+, neúspěch není turnover. Ř. 1839-1843: faul
 * pole pohybu nestojí. Dřív web ležícímu nabízel a pouštěl jen pohyb a blitz (P86). Parita s C++ `standsUpFirst`.
 */
final class ProneActionsTest extends TestCase
{
    /** Ležící HOME (5,5) vedle ležícího soupeře AWAY (6,5). */
    private function stav(int $ma = 6, int $prehozy = 3): GameState
    {
        return (new GameStateBuilder())
            ->withHomeTeam(TeamStateDTO::create(1, 'Home', 'Human', TeamSide::HOME, $prehozy))
            ->addPronePlayer(TeamSide::HOME, 5, 5, movement: $ma, id: 1)
            ->addPronePlayer(TeamSide::AWAY, 6, 5, id: 2)
            ->withBallOffPitch()
            ->build();
    }

    /** @return list<string> */
    private function nabidka(GameState $s, int $hrac): array
    {
        $typy = [];
        foreach ((new RulesEngine())->getAvailableActions($s) as $a) {
            if (($a['playerId'] ?? null) === $hrac) {
                $typy[] = $a['type'];
            }
        }

        return $typy;
    }

    public function testLeziciDostaneFaulAleNeBlok(): void
    {
        $typy = $this->nabidka($this->stav(), 1);

        $this->assertContains(ActionType::FOUL->value, $typy, 'ř. 676 „any Action other than a Block Action“');
        $this->assertNotContains(ActionType::BLOCK->value, $typy);
        $this->assertSame([], (new RulesEngine())->validate($this->stav(), ActionType::FOUL, ['playerId' => 1, 'targetId' => 2]));
    }

    public function testLeziciVstaneZaTriPolePakFauluje(): void
    {
        $r = (new ActionResolver(new FixedDiceRoller([1, 2])))->resolve($this->stav(), ActionType::FOUL, ['playerId' => 1, 'targetId' => 2]);
        $s = $r->getNewState();

        $this->assertFalse($r->isTurnover());
        $this->assertSame(PlayerState::STANDING, $s->requirePlayer(1)->getState());
        $this->assertTrue($s->getTeamState(TeamSide::HOME)->isFoulUsedThisTurn());
        $this->assertContains('foul', array_map(static fn($e): string => $e->getType(), $r->getEvents()));
    }

    /** MA 2: vstání 4+; 3 nevyjde ⇒ faul propadá (ne turnover, ř. 694-695), faul týmu spotřebovalo ohlášení. */
    public function testPomalyLeziciNevstaneFaulPropada(): void
    {
        $r = (new ActionResolver(new FixedDiceRoller([3])))->resolve($this->stav(2, 0), ActionType::FOUL, ['playerId' => 1, 'targetId' => 2]);
        $s = $r->getNewState();

        $this->assertFalse($r->isTurnover());
        $this->assertSame(PlayerState::PRONE, $s->requirePlayer(1)->getState());
        $this->assertTrue($s->requirePlayer(1)->hasActed());
        $this->assertTrue($s->getTeamState(TeamSide::HOME)->isFoulUsedThisTurn());
        $this->assertNotContains('foul', array_map(static fn($e): string => $e->getType(), $r->getEvents()));
    }

    public function testLeziciSHypnotickymPohledemDostaneGaze(): void
    {
        $s = (new GameStateBuilder())
            ->addPronePlayer(TeamSide::HOME, 5, 5, skills: [SkillName::HypnoticGaze], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, id: 2)
            ->withBallOffPitch()
            ->build();

        $this->assertContains(ActionType::HYPNOTIC_GAZE->value, $this->nabidka($s, 1));
    }

    /** Bomba ne: „may not move or stand up before throwing it“ (ř. 7953-7954). */
    public function testLeziciBombardierBombuNedostane(): void
    {
        $s = (new GameStateBuilder())
            ->addPronePlayer(TeamSide::HOME, 5, 5, skills: [SkillName::Bombardier], id: 1)
            ->addPlayer(TeamSide::AWAY, 10, 5, id: 2)
            ->withBallOffPitch()
            ->build();

        $this->assertNotContains(ActionType::BOMB_THROW->value, $this->nabidka($s, 1));
    }
}
