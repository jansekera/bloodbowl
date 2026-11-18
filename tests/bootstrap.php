<?php

declare(strict_types=1);

require __DIR__ . '/../vendor/autoload.php';

// Review P306 (/simplify): každá akce v testech hlídá, že log akce nemá zahozené události (výpočet nanečisto)
App\Engine\ActionResolver::hlidatLog(true);
