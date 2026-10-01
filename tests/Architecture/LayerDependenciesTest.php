<?php

declare(strict_types=1);

namespace App\Tests\Architecture;

use PHPUnit\Framework\Attributes\DataProvider;
use PHPUnit\Framework\TestCase;

/**
 * P102 — vrstvy PHP podle tří pravidel z Brasty (Domain Protection, Repository Pattern, No Direct DB Access).
 * Tabulka a zdůvodnění: CLAUDE.md, oddíl „Vrstvy PHP“. Jeden test na chokepointu: projde VŠECHNY soubory
 * vrstvy, takže nový soubor je hlídaný hned, jak vznikne.
 *
 * Závislost = jakákoli zmínka `App\Vrstva\…` v kódu (use i plný název uvnitř), komentáře se nepočítají.
 */
final class LayerDependenciesTest extends TestCase
{
    private const SRC = __DIR__ . '/../../src';

    /** vrstva => vrstvy, na kterých smí záviset (vlastní vrstva je povolená vždy) */
    private const ALLOWED = [
        'Engine' => ['DTO', 'Enum', 'ValueObject', 'Exception'],
        'DTO' => ['Enum', 'ValueObject', 'Exception'],
        'Entity' => ['Enum', 'ValueObject', 'Exception'],
        'ValueObject' => ['Enum', 'Exception'],
        'Enum' => [],
        'AI' => ['Engine', 'DTO', 'Enum', 'ValueObject', 'Event', 'Exception'],
        'Controller' => ['Service', 'Repository', 'Exception', 'Enum', 'Http', 'Entity', 'DTO'],
    ];

    /** @return iterable<string, array{string}> */
    public static function layers(): iterable
    {
        foreach (array_keys(self::ALLOWED) as $layer) {
            yield $layer => [$layer];
        }
    }

    #[DataProvider('layers')]
    public function testLayerDependsOnlyOnAllowedLayers(string $layer): void
    {
        $allowed = [...self::ALLOWED[$layer], $layer];
        $violations = [];
        foreach (self::phpFiles($layer) as $file) {
            foreach (self::referencedLayers($file) as $used) {
                if (!in_array($used, $allowed, true)) {
                    $violations[] = self::rel($file) . " → App\\$used";
                }
            }
        }
        $this->assertSame([], $violations, "Vrstva $layer smí jen na: " . implode(', ', $allowed));
    }

    public function testOnlyRepositoriesTouchTheDatabase(): void
    {
        $violations = [];
        foreach (self::phpFiles('') as $file) {
            $rel = self::rel($file);
            if (str_starts_with($rel, 'Repository/') || $rel === 'Database.php' || str_starts_with($rel, 'Container/')) {
                continue; // Container jen skládá repozitáře s připojením
            }
            $code = self::codeWithoutComments($file);
            if (preg_match('/\\\\?\bPDO\b|App\\\\Database\b|\bDatabase::/', $code)) {
                $violations[] = $rel;
            }
        }
        $this->assertSame([], $violations, 'PDO / Database smí jen Repository/ (No Direct DB Access)');
    }

    /** Pozitivní kontrola: skener opravdu najde zakázanou závislost, když ji kód obsahuje. */
    public function testScannerFindsAReferenceInCodeButNotInComments(): void
    {
        $tmp = tempnam(sys_get_temp_dir(), 'arch') . '.php';
        file_put_contents($tmp, "<?php\n// App\\Controller\\X jen v komentáři\nuse App\\Repository\\TeamRepository;\n\$x = new \\App\\Service\\TeamService();\n");
        try {
            $this->assertSame(['Repository', 'Service'], self::referencedLayers($tmp));
        } finally {
            unlink($tmp);
        }
    }

    /** @return list<string> */
    private static function phpFiles(string $layer): array
    {
        $dir = rtrim(self::SRC . '/' . $layer, '/');
        $it = new \RecursiveIteratorIterator(new \RecursiveDirectoryIterator($dir, \FilesystemIterator::SKIP_DOTS));
        $files = [];
        foreach ($it as $f) {
            if ($f instanceof \SplFileInfo && $f->getExtension() === 'php') {
                $files[] = $f->getPathname();
            }
        }
        sort($files);
        return $files;
    }

    /** @return list<string> */
    private static function referencedLayers(string $file): array
    {
        preg_match_all('/\bApp\\\\([A-Za-z]+)\\\\/', self::codeWithoutComments($file), $m);
        $layers = array_values(array_unique($m[1]));
        sort($layers);
        return $layers;
    }

    private static function codeWithoutComments(string $file): string
    {
        $code = '';
        foreach (token_get_all((string) file_get_contents($file)) as $t) {
            if (is_array($t) && in_array($t[0], [T_COMMENT, T_DOC_COMMENT], true)) {
                continue;
            }
            $code .= is_array($t) ? $t[1] : $t;
        }
        return $code;
    }

    private static function rel(string $file): string
    {
        return ltrim(substr((string) realpath($file), strlen((string) realpath(self::SRC))), '/');
    }
}
