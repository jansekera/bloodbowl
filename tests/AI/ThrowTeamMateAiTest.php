<?php

declare(strict_types=1);

namespace App\Tests\AI;

use App\AI\CoachHeuristics;
use App\AI\GreedyAICoach;
use App\AI\LearningAICoach;
use App\Engine\RulesEngine;
use App\Enum\ActionType;
use App\Enum\PassRange;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use App\Tests\Engine\GameStateBuilder;
use PHPUnit\Framework\TestCase;

/**
 * P270 (9.11.2026, převod P268 do PHP; uživatel: „bomba i hod spoluhráčem“). Parita s C++ `macro_actions.cpp`
 * offerBombAndTtmMacros: hráč s Throw Team-Mate hodí sousedního stojícího nosiče míče s Right Stuff na prázdné pole
 * rychlou/krátkou přihrávkou (r. 8608-8609) aspoň 4 sloupce k endzóně — vždy 3 rozptyly (r. 8609-8612) a po
 * přistání smí nosič ještě táhnout (Right Stuff, P269). Dřív PHP kouči TTM nehráli vůbec.
 */
final class ThrowTeamMateAiTest extends TestCase
{
    private function stav(bool $rightStuff = true, bool $prihravkaPouzita = false): \App\DTO\GameState
    {
        $b = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, movement: 4, strength: 5, agility: 1, armour: 9, skills: [SkillName::ThrowTeamMate], id: 1)
            ->addPlayer(TeamSide::HOME, 6, 7, strength: 2, armour: 7, skills: $rightStuff ? [SkillName::RightStuff, SkillName::Stunty] : [SkillName::Stunty], id: 2)
            ->addPlayer(TeamSide::AWAY, 3, 1, id: 12)
            ->withBallCarried(2);
        $s = $b->build();
        if ($prihravkaPouzita) {
            $s = $s->withTeamState(TeamSide::HOME, $s->getTeamState(TeamSide::HOME)->withPassUsed());
        }
        return $s;
    }

    public function testHodiNosiceAsponCtyriSloupceVpred(): void
    {
        $s = $this->stav();
        $p = CoachHeuristics::hodSpoluhraceProAi($s, $s->requirePlayer(1));
        $this->assertNotNull($p);
        $this->assertSame(2, $p['targetId']);
        $this->assertGreaterThanOrEqual(6 + 4, $p['targetX']);
        $dosah = PassRange::fromOffset($p['targetX'] - 5, $p['targetY'] - 7);
        $this->assertContains($dosah, [PassRange::QUICK_PASS, PassRange::SHORT_PASS]);
        $this->assertSame([], (new RulesEngine())->validate($s, ActionType::THROW_TEAM_MATE, ['playerId' => 1] + $p));
    }

    public function testBezRightStuffNebOPouzitePrihravceNic(): void
    {
        $s = $this->stav(rightStuff: false);
        $this->assertNull(CoachHeuristics::hodSpoluhraceProAi($s, $s->requirePlayer(1)));
        $s = $this->stav(prihravkaPouzita: true);
        $this->assertNull(CoachHeuristics::hodSpoluhraceProAi($s, $s->requirePlayer(1)));
    }

    /** Kouči akci opravdu zvolí (dřív `default => null`). */
    public function testKouciHodNabidnou(): void
    {
        $s = $this->stav();
        $volani = [
            [new GreedyAICoach(), 'scoreAction', [$s, new RulesEngine(), ActionType::THROW_TEAM_MATE, 1, TeamSide::HOME]],
            [new LearningAICoach(), 'buildScoredAction', [$s, new RulesEngine(), ActionType::THROW_TEAM_MATE, 1, TeamSide::HOME, 0.5]],
        ];
        foreach ($volani as [$kouc, $metoda, $argy]) {
            $r = (new \ReflectionMethod($kouc, $metoda))->invoke($kouc, ...$argy);
            $this->assertNotNull($r, $kouc::class);
            $this->assertSame(ActionType::THROW_TEAM_MATE, $r['action']);
            $this->assertSame(2, $r['params']['targetId']);
        }
    }
}
