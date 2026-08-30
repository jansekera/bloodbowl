<?php
declare(strict_types=1);
// Zavaděč pro běh testů z WORKTREE. `vendor/` je symlink do hlavního stromu
// a jeho autoloader má absolutní cesty tam, takže nové soubory z worktree
// nevidí. Tenhle autoloader se registruje PŘED ním (prepend) a míří do
// zdejšího `src/`. Použití: phpunit --bootstrap tests/bootstrap_worktree.php
require __DIR__ . '/../vendor/autoload.php';
spl_autoload_register(static function (string $class): void {
    if (!str_starts_with($class, 'App\\')) return;
    $rel = str_replace('\\', '/', substr($class, 4)) . '.php';
    $file = __DIR__ . '/../src/' . $rel;
    if (is_file($file)) require $file;
}, true, true);
