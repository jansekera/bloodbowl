<?php

declare(strict_types=1);

namespace App\Twig;

/**
 * URL statického souboru s verzí podle času změny (P313, 20.11.2026): bez verze prohlížeč po nasazení drží starý
 * `/dist/*.js` z cache. Ve šablonách jako `{{ asset('/dist/match.js') }}`.
 */
final class AssetUrl
{
    public function __construct(private readonly string $verejnyAdresar) {}

    public function __invoke(string $cesta): string
    {
        $cas = @filemtime($this->verejnyAdresar . $cesta);
        if ($cas === false) {
            throw new \RuntimeException("Statický soubor {$cesta} neexistuje (sestavit: npx vite build)");
        }

        return $cesta . '?v=' . $cas;
    }
}
