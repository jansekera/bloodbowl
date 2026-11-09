<?php

declare(strict_types=1);

namespace App\Tests\AI;

use App\AI\CoachHeuristics;
use App\Engine\RulesEngine;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use App\Enum\Weather;
use App\Tests\Engine\GameStateBuilder;
use PHPUnit\Framework\TestCase;

/**
 * P266 (uživatel 9.11.2026: „přidej možnost hrát hail mary když je oponent daleko“, upřesnil „obojí“).
 * Parita s C++ `rules_engine.cpp` / `macro_actions.cpp` (test_hail_mary_ai.cpp):
 *  - bomba s HMP na stojícího soupeře mimo pravítko (r. 7952-7954, 8164-8169);
 *  - přihrávka s HMP stojícímu spoluhráči mimo pravítko vpřed, když do 3 polí od cíle nestojí soupeř
 *    (3 rozptyly, r. 8167-8168). PHP kouči navíc jen jako POSLEDNÍ akce tahu — nechycená HMP je turnover
 *    a PHP kouči tah nehledají jako C++ MCTS.
 * Ve vánici HMP ne (r. 8170-8171).
 */
final class HailMaryAiTest extends TestCase
{
    private function bombaStav(bool $hmp, Weather $pocasi = Weather::NICE): \App\DTO\GameState
    {
        $skilly = $hmp ? [SkillName::Bombardier, SkillName::HailMaryPass] : [SkillName::Bombardier];
        return (new GameStateBuilder())
            ->withWeather($pocasi)
            ->addPlayer(TeamSide::HOME, 3, 7, id: 1, skills: $skilly)
            ->addPlayer(TeamSide::AWAY, 22, 7, id: 12)      // 19 polí — mimo pravítko
            ->withBallOffPitch()
            ->build();
    }

    /** @return list<string> */
    private function hmpCile(\App\DTO\GameState $s, bool $bomba): array
    {
        $cile = CoachHeuristics::cileHoduProAi(new RulesEngine(), $s, $s->requirePlayer(1), $bomba);
        $hmp = array_filter($cile, fn($t) => $t['range'] === 'hail_mary');
        return array_values(array_map(fn($t) => $t['x'] . ',' . $t['y'], $hmp));
    }

    public function testBombaSHmpNaVzdalenehoSoupere(): void
    {
        $this->assertSame(['22,7'], $this->hmpCile($this->bombaStav(true), true));
    }

    public function testBombaBezHmpNicMimoPravitko(): void
    {
        $this->assertSame([], $this->hmpCile($this->bombaStav(false), true));
    }

    public function testBombaVeVaniciBezHmp(): void
    {
        $this->assertSame([], $this->hmpCile($this->bombaStav(true, Weather::BLIZZARD), true));
    }

    /** Pravítko zůstává: pozitivní kontrola, že funkce cíle na pravítku nezahodí. */
    public function testPravitkoZustava(): void
    {
        $s = $this->bombaStav(true);
        $cile = CoachHeuristics::cileHoduProAi(new RulesEngine(), $s, $s->requirePlayer(1), true);
        $this->assertContains('quick_pass', array_column($cile, 'range'));
    }

    private function prihravkaStav(bool $hmp, int $souperX = 3, int $souperY = 1, bool $dalsiCeka = false): \App\DTO\GameState
    {
        $b = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 3, 7, id: 1, skills: $hmp ? [SkillName::HailMaryPass] : [])
            ->addPlayer(TeamSide::HOME, 22, 7, id: 2)       // 19 polí vpřed
            ->addPlayer(TeamSide::AWAY, $souperX, $souperY, id: 12)
            ->withBallCarried(1);
        $s = $b->build();
        // všichni naši kromě házeče už táhli (HMP = poslední akce tahu)
        $s = $s->withPlayer($s->requirePlayer(2)->withHasActed(!$dalsiCeka));
        return $s;
    }

    public function testPrihravkaSHmpDoVolna(): void
    {
        $this->assertSame(['22,7'], $this->hmpCile($this->prihravkaStav(true), false));
    }

    public function testPrihravkaSHmpNeKdyzJeSouperDo3Poli(): void
    {
        $this->assertSame([], $this->hmpCile($this->prihravkaStav(true, 20, 9), false));
    }

    public function testPrihravkaSHmpNeKdyzJesteNekdoNetahl(): void
    {
        $this->assertSame([], $this->hmpCile($this->prihravkaStav(true, dalsiCeka: true), false));
    }

    public function testPrihravkaBezHmpNic(): void
    {
        $this->assertSame([], $this->hmpCile($this->prihravkaStav(false), false));
    }

    public function testPrihravkaSHmpNeDozadu(): void
    {
        $s = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 23, 7, id: 1, skills: [SkillName::HailMaryPass])
            ->addPlayer(TeamSide::HOME, 3, 7, id: 2)
            ->addPlayer(TeamSide::AWAY, 12, 1, id: 12)
            ->withBallCarried(1)
            ->build();
        $s = $s->withPlayer($s->requirePlayer(2)->withHasActed(true));
        $this->assertSame([], $this->hmpCile($s, false));
    }
}
