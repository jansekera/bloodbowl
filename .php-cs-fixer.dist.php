<?php

declare(strict_types=1);

// Formát PHP: standard PER-CS (nástupce PSR-12). Jen formát, žádná „risky“ pravidla měnící chování.
// Kontrola: make check-lint · oprava: vendor/bin/php-cs-fixer fix
$finder = PhpCsFixer\Finder::create()
    ->in([__DIR__ . '/src', __DIR__ . '/tests', __DIR__ . '/public'])
    ->append([__DIR__ . '/config.php', __DIR__ . '/migrate.php', __DIR__ . '/seed.php']);

return (new PhpCsFixer\Config())
    ->setRiskyAllowed(false)
    ->setRules(['@PER-CS' => true])
    ->setFinder($finder);
