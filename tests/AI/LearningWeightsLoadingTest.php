<?php
declare(strict_types=1);

namespace App\Tests\AI;

use App\AI\FeatureExtractor;
use App\AI\LearningAICoach;
use PHPUnit\Framework\TestCase;

/**
 * ⛔ VADA (11.09.2026): konstruktor dělal
 *    `array_values(array_map('floatval', $data))` nad CELÝM objektem JSON,
 *    tedy nad klíči `type` / `value_weights` / `policy_weights` /
 *    `policy_bias` / `policy_temperature` -- ne nad vahami.
 *    `floatval('alphazero_linear')`=0, `floatval(pole)`=1 ⇒ vektor vah byl
 *    **[0, 1, 1, ~0, 1]**, tedy PĚT čísel místo 73.
 *
 * ⛔ A nebylo to vidět: `dotProduct` bere `min(count($a), count($b))` a zbytek
 *    TIŠE USEKNE. Stav se hodnotil jako `f1 + f2 + f4`.
 */
final class LearningWeightsLoadingTest extends TestCase
{
    private function writeWeights(array $data): string
    {
        $path = sys_get_temp_dir() . '/bb_weights_' . uniqid() . '.json';
        file_put_contents($path, json_encode($data));
        return $path;
    }

    public function testStructuredFileLoadsValueWeightsNotTheTopLevelKeys(): void
    {
        $vw = array_fill(0, 70, 0.0);
        $vw[0] = 0.25;
        $vw[69] = -0.5;

        $path = $this->writeWeights([
            'type' => 'alphazero_linear',
            'value_weights' => $vw,
            'policy_weights' => array_fill(0, 85, 1.0),
            'policy_bias' => 0.0,
            'policy_temperature' => 1.0,
        ]);

        $w = (new LearningAICoach($path, 0.0))->getWeights();
        unlink($path);

        // ⛔ Tohle je ta vada: dřív vyšla délka 5.
        $this->assertCount(FeatureExtractor::NUM_FEATURES, $w,
            'vektor vah musí mít tolik prvků, kolik je příznaků');
        $this->assertSame(0.25, $w[0], 'první váha se má vzít z value_weights');
        $this->assertSame(-0.5, $w[69], 'a poslední natrénovaná taky');
    }

    public function testShorterWeightVectorIsPaddedNotSilentlyTruncatedByDotProduct(): void
    {
        // Soubor má 70 vah, příznaků je 73 (tři přibyly NA KONEC, `30539d65`).
        // Doplnění nulami drží indexy 0-69 na tomtéž významu.
        $vw = array_fill(0, 70, 0.125);
        $path = $this->writeWeights([
            'type' => 'alphazero_linear',
            'value_weights' => $vw,
            'policy_weights' => [],
            'policy_bias' => 0.0,
            'policy_temperature' => 1.0,
        ]);

        $w = (new LearningAICoach($path, 0.0))->getWeights();
        unlink($path);

        $this->assertCount(73, $w);
        $this->assertSame(0.125, $w[69], 'natrénované indexy se nesmí posunout');
        $this->assertSame(0.0, $w[70], 'nové příznaky dostanou nulu');
        $this->assertSame(0.0, $w[72]);
    }

    public function testFlatLegacyListStillLoads(): void
    {
        // Starší formát: holé pole floatů. Kvůli němu tam `array_map` byl.
        $path = $this->writeWeights(array_fill(0, 73, 0.5));

        $w = (new LearningAICoach($path, 0.0))->getWeights();
        unlink($path);

        $this->assertCount(73, $w);
        $this->assertSame(0.5, $w[0]);
        $this->assertSame(0.5, $w[72]);
    }

    public function testNoFileGivesZeroWeightsOfTheRightLength(): void
    {
        $w = (new LearningAICoach(null, 0.0))->getWeights();
        $this->assertCount(FeatureExtractor::NUM_FEATURES, $w);
        $this->assertSame(0.0, $w[0]);
    }
}
