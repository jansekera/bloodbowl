<?php

declare(strict_types=1);

namespace App\Tests\Twig;

use App\Twig\AssetUrl;
use PHPUnit\Framework\TestCase;

/**
 * P313 (20.11.2026): skript webu se načítá s verzí v URL, jinak prohlížeč po nasazení drží starého klienta z cache
 * (u P186 se tím zasekl blok „obránce-AI vybírá“).
 */
final class AssetUrlTest extends TestCase
{
    public function testUrlNeseVerziPodleZmenySouboru(): void
    {
        $verejne = dirname(__DIR__, 2) . '/public';
        $url = (new AssetUrl($verejne))('/dist/match.js');

        $this->assertSame('/dist/match.js?v=' . filemtime($verejne . '/dist/match.js'), $url);
    }

    public function testChybejiciSouborJeChyba(): void
    {
        $this->expectException(\RuntimeException::class);
        (new AssetUrl(dirname(__DIR__, 2) . '/public'))('/dist/neexistuje.js');
    }

    public function testSablonyNeodkazujiNaDistBezVerze(): void
    {
        foreach (glob(dirname(__DIR__, 2) . '/templates/**/*.twig') ?: [] as $f) {
            $this->assertDoesNotMatchRegularExpression('#src="/dist/#', (string) file_get_contents($f), $f);
        }
        foreach (glob(dirname(__DIR__, 2) . '/templates/*.twig') ?: [] as $f) {
            $this->assertDoesNotMatchRegularExpression('#src="/dist/#', (string) file_get_contents($f), $f);
        }
    }
}
