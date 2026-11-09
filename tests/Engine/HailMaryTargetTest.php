<?php

declare(strict_types=1);

namespace App\Tests\Engine;

use App\Engine\RulesEngine;
use App\Enum\ActionType;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use App\Enum\Weather;
use PHPUnit\Framework\TestCase;

/**
 * P263 + P265 (9.11.2026): cíl mimo pravítko s Hail Mary Pass.
 *
 * `rules_bb2016.txt` r. 8164-8171: hráč s HMP smí hodit „to any square on the pitch, the range ruler is not used“;
 * „may not be used in a blizzard“. R. 7952-7954: totéž platí pro bombu („including … use of Hail Mary Pass“).
 * Do 9.11. ji handler PassResolver uměl, ale validátor (brána každé akce člověka, GameOrchestrator) i seznam
 * cílů pro frontend pole mimo pravítko odmítaly ⇒ člověk HMP použít nemohl.
 */
final class HailMaryTargetTest extends TestCase
{
    /** @param list<SkillName> $skilly */
    private function stav(array $skilly, Weather $pocasi = Weather::NICE): \App\DTO\GameState
    {
        return (new GameStateBuilder())
            ->withWeather($pocasi)
            ->addPlayer(TeamSide::HOME, 3, 7, id: 1, skills: $skilly)
            ->addPlayer(TeamSide::HOME, 23, 7, id: 2)          // 20 polí — mimo pravítko
            ->withBallCarried(1)
            ->build();
    }

    /**
     * @param list<SkillName> $skilly
     * @return list<string>
     */
    private function chyby(ActionType $akce, array $skilly, Weather $pocasi = Weather::NICE): array
    {
        return (new RulesEngine())->validate($this->stav($skilly, $pocasi), $akce, [
            'playerId' => 1, 'targetX' => 23, 'targetY' => 7,
        ]);
    }

    public function testHmpPrihravkaMimoPravitkoJePlatna(): void
    {
        $this->assertSame([], $this->chyby(ActionType::PASS, [SkillName::HailMaryPass]));
    }

    public function testBezHmpJePrihravkaMimoPravitkoNeplatna(): void
    {
        $this->assertNotSame([], $this->chyby(ActionType::PASS, []));
    }

    public function testVeVaniciHmpPrihravkaNeplatna(): void
    {
        $this->assertNotSame([], $this->chyby(ActionType::PASS, [SkillName::HailMaryPass], Weather::BLIZZARD));
    }

    public function testHmpBombaMimoPravitkoJePlatna(): void
    {
        $this->assertSame([], $this->chyby(ActionType::BOMB_THROW, [SkillName::Bombardier, SkillName::HailMaryPass]));
    }

    public function testBezHmpBombaMimoPravitkoNeplatna(): void
    {
        $this->assertNotSame([], $this->chyby(ActionType::BOMB_THROW, [SkillName::Bombardier]));
    }

    public function testVeVaniciHmpBombaNeplatna(): void
    {
        $this->assertNotSame([], $this->chyby(ActionType::BOMB_THROW, [SkillName::Bombardier, SkillName::HailMaryPass], Weather::BLIZZARD));
    }

    /** Seznam cílů pro člověka nabídne i pole mimo pravítko jako „hail_mary“. */
    public function testCileProClovekaObsahujiHailMary(): void
    {
        $s = $this->stav([SkillName::HailMaryPass]);
        $cile = (new RulesEngine())->getPassTargets($s, $s->requirePlayer(1), sHailMary: true);
        $cil = array_values(array_filter($cile, fn($t) => $t['x'] === 23 && $t['y'] === 7));
        $this->assertSame('hail_mary', $cil[0]['range'] ?? null);
    }

    /** Kouči AI dostávají seznam beze změny (HMP pro AI = nová volba, jen po představení uživateli). */
    public function testCileProAiBezHailMary(): void
    {
        $s = $this->stav([SkillName::HailMaryPass]);
        $dosahy = array_unique(array_column((new RulesEngine())->getPassTargets($s, $s->requirePlayer(1)), 'range'));
        $this->assertNotContains('hail_mary', $dosahy);
        $this->assertCount(4, $dosahy);   // pozitivní kontrola: pravítko je celé
    }

    public function testVeVaniciCileBezHailMary(): void
    {
        $s = $this->stav([SkillName::HailMaryPass], Weather::BLIZZARD);
        $dosahy = array_unique(array_column((new RulesEngine())->getPassTargets($s, $s->requirePlayer(1), sHailMary: true), 'range'));
        sort($dosahy);
        $this->assertSame(['quick_pass', 'short_pass'], $dosahy);
    }
}
