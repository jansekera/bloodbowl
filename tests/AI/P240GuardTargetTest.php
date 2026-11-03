<?php

declare(strict_types=1);

namespace App\Tests\AI;

use App\AI\LearningAICoach;
use App\DTO\GameState;
use App\Engine\RulesEngine;
use App\Enum\{ActionType, SkillName, TeamSide};
use App\Tests\Engine\GameStateBuilder;
use PHPUnit\Framework\TestCase;

/**
 * ⭐ P240 (uživatel 3.11.2026): „block s block nebo wrestle je OK na 1 kostku - účel je navíc jako
 * preference koho blokovat přednostně“. Účel z 2.11.: „…nebo to má jinou výhodu při sražení - např.
 * nepřítele s guard“. Ze dvou jinak stejných cílů volí kouč ten s Guard. Každý test se ptá oběma
 * směry (Guard jednou na jednom, jednou na druhém cíli), aby nerozhodlo pořadí cílů.
 */
final class P240GuardTargetTest extends TestCase
{
    /** @param list<SkillName> $utocnik */
    private function cil(ActionType $typ, int $sGuard, array $utocnik, int $x, int $y, int $y1, int $y2): ?int
    {
        $skills = fn(int $id): array => $id === $sGuard ? [SkillName::Guard] : [];
        $state = (new GameStateBuilder())->withActiveTeam(TeamSide::HOME)
            ->addPlayer(TeamSide::HOME, $x, $y, strength: 4, skills: $utocnik, id: 1)
            ->addPlayer(TeamSide::AWAY, $x + ($typ === ActionType::BLOCK ? 1 : 3), $y1, strength: 3, skills: $skills(12), id: 12)
            ->addPlayer(TeamSide::AWAY, $x + ($typ === ActionType::BLOCK ? 1 : 3), $y2, strength: 3, skills: $skills(13), id: 13)
            ->withBallOffPitch()->build();

        return $this->navrh($state, $typ)['params']['targetId'] ?? null;
    }

    /** @return array<string, mixed>|null */
    private function navrh(GameState $state, ActionType $typ): ?array
    {
        $build = new \ReflectionMethod(LearningAICoach::class, 'buildScoredAction');

        return $build->invoke(new LearningAICoach(null, 0.0), $state, new RulesEngine(), $typ, 1, TeamSide::HOME, 0.0);
    }

    /** Útočník ST 4 s Block mezi dvěma ST 3, kteří si navzájem asistují ⇒ na každého 1 kostka. */
    public function testBlokNaJednuKostkuVoliHraceSGuard(): void
    {
        $this->assertSame(13, $this->cil(ActionType::BLOCK, 13, [SkillName::Block], 10, 7, 6, 8));
        $this->assertSame(12, $this->cil(ActionType::BLOCK, 12, [SkillName::Block], 10, 7, 6, 8));
    }

    /** Wrestle bez Block: blok na 1 kostku zůstává v nabídce (uživatel: „je OK“) a míří na Guard. */
    public function testWrestleBezBlockBlokujeNaJednuKostkuHraceSGuard(): void
    {
        $this->assertSame(13, $this->cil(ActionType::BLOCK, 13, [SkillName::Wrestle], 10, 7, 6, 8));
        $this->assertSame(12, $this->cil(ActionType::BLOCK, 12, [SkillName::Wrestle], 10, 7, 6, 8));
    }

    /** Blitz: dva stejně vzdálení ST 3 daleko od sebe (bez asistencí, 2 kostky naše), Guard rozhodne. */
    public function testBlitzVoliHraceSGuard(): void
    {
        $this->assertSame(13, $this->cil(ActionType::BLITZ, 13, [], 7, 7, 4, 10));
        $this->assertSame(12, $this->cil(ActionType::BLITZ, 12, [], 7, 7, 4, 10));
    }

    /** Guard nepřebije kostky: 2 kostky naše na hráče bez Guard mají přednost před 1 kostkou na Guard. */
    public function testGuardNeprebijeLepsiKostky(): void
    {
        $state = (new GameStateBuilder())->withActiveTeam(TeamSide::HOME)
            ->addPlayer(TeamSide::HOME, 10, 7, strength: 3, skills: [SkillName::Block], id: 1)
            ->addPlayer(TeamSide::AWAY, 11, 6, strength: 3, skills: [SkillName::Guard], id: 12)   // 1 kostka
            ->addPlayer(TeamSide::AWAY, 11, 8, strength: 3, id: 13)
            // dva asistenti jen u #13: #12 s Guard mu asistuje i ze zóny (Guard), proto dva ⇒ 3+2 proti 3+1
            ->addPlayer(TeamSide::HOME, 12, 9, strength: 3, id: 2)
            ->addPlayer(TeamSide::HOME, 10, 9, strength: 3, id: 3)
            ->withBallOffPitch()->build();
        // sebekontrola fixtury: kostky opravdu 1 a 2
        $kostky = fn(int $cil) => \App\AI\CoachHeuristics::kostkyBloku(
            new \App\Engine\StrengthCalculator(),
            $state,
            $state->requirePlayer(1),
            $state->requirePlayer($cil),
            0,
            null,
            0,
        );
        $this->assertSame(1, $kostky(12)['count']);
        $this->assertSame(2, $kostky(13)['count']);
        $this->assertTrue($kostky(13)['attackerChooses']);

        $this->assertSame(13, $this->navrh($state, ActionType::BLOCK)['params']['targetId'] ?? null);
    }
}
