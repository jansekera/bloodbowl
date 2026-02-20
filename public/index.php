<?php

declare(strict_types=1);

require __DIR__ . '/../vendor/autoload.php';

use App\Container\Container;
use App\Container\ServiceProvider;
use App\Controller\ApiController;
use App\Controller\PageController;
use App\Controller\MatchApiController;
use App\Controller\MatchPageController;
use App\Controller\TeamApiController;
use App\Controller\TeamPageController;

$container = new Container();
ServiceProvider::register($container);

$uri = parse_url($_SERVER['REQUEST_URI'] ?? '/', PHP_URL_PATH);
$method = $_SERVER['REQUEST_METHOD'] ?? 'GET';

// API versioning: normalize /api/v1/* to /api/*
$apiPath = null;
if (preg_match('#^/api/v1(/.*)?$#', $uri, $m)) {
    $apiPath = '/api' . ($m[1] ?? '');
}

// CORS for API
if ($apiPath !== null) {
    header('Access-Control-Allow-Origin: *');
    header('Access-Control-Allow-Methods: GET, POST, PUT, PATCH, DELETE, OPTIONS');
    header('Access-Control-Allow-Headers: Content-Type, Authorization');

    if ($method === 'OPTIONS') {
        http_response_code(204);
        exit;
    }
}

// ========== API Routes ==========
if ($apiPath !== null) {
    $api = $container->get(ApiController::class);
    $teamApi = $container->get(TeamApiController::class);

    // Races
    if ($apiPath === '/api/races' && $method === 'GET') {
        $api->getRaces();
        exit;
    }
    if (preg_match('#^/api/races/(\d+)$#', $apiPath, $m) && $method === 'GET') {
        $api->getRace((int) $m[1]);
        exit;
    }

    // Skills
    if ($apiPath === '/api/skills' && $method === 'GET') {
        $api->getSkills();
        exit;
    }
    if (preg_match('#^/api/skills/(\d+)$#', $apiPath, $m) && $method === 'GET') {
        $api->getSkill((int) $m[1]);
        exit;
    }

    // Teams
    if ($apiPath === '/api/teams' && $method === 'GET') {
        $teamApi->list();
        exit;
    }
    if ($apiPath === '/api/teams' && $method === 'POST') {
        $teamApi->create();
        exit;
    }
    if (preg_match('#^/api/teams/(\d+)$#', $apiPath, $m) && $method === 'GET') {
        $teamApi->show((int) $m[1]);
        exit;
    }

    // Team players
    if (preg_match('#^/api/teams/(\d+)/players$#', $apiPath, $m) && $method === 'POST') {
        $teamApi->hirePlayer((int) $m[1]);
        exit;
    }
    if (preg_match('#^/api/teams/(\d+)/players/(\d+)$#', $apiPath, $m) && $method === 'DELETE') {
        $teamApi->firePlayer((int) $m[1], (int) $m[2]);
        exit;
    }

    // Player advancement
    if (preg_match('#^/api/players/(\d+)/available-skills$#', $apiPath, $m) && $method === 'GET') {
        $teamApi->getAvailableSkills((int) $m[1]);
        exit;
    }
    if (preg_match('#^/api/players/(\d+)/advance$#', $apiPath, $m) && $method === 'POST') {
        $teamApi->advancePlayer((int) $m[1]);
        exit;
    }

    // Team purchases
    if (preg_match('#^/api/teams/(\d+)/rerolls$#', $apiPath, $m) && $method === 'POST') {
        $teamApi->buyReroll((int) $m[1]);
        exit;
    }
    if (preg_match('#^/api/teams/(\d+)/apothecary$#', $apiPath, $m) && $method === 'POST') {
        $teamApi->buyApothecary((int) $m[1]);
        exit;
    }

    // Matches
    $matchApi = $container->get(MatchApiController::class);

    if ($apiPath === '/api/matches' && $method === 'POST') {
        $matchApi->create();
        exit;
    }
    if (preg_match('#^/api/matches/(\d+)/state$#', $apiPath, $m) && $method === 'GET') {
        $matchApi->getState((int) $m[1]);
        exit;
    }
    if (preg_match('#^/api/matches/(\d+)/actions$#', $apiPath, $m) && $method === 'POST') {
        $matchApi->submitAction((int) $m[1]);
        exit;
    }
    if (preg_match('#^/api/matches/(\d+)/actions$#', $apiPath, $m) && $method === 'GET') {
        $matchApi->getAvailableActions((int) $m[1]);
        exit;
    }
    if (preg_match('#^/api/matches/(\d+)/players/(\d+)/moves$#', $apiPath, $m) && $method === 'GET') {
        $matchApi->getValidMoves((int) $m[1], (int) $m[2]);
        exit;
    }
    if (preg_match('#^/api/matches/(\d+)/players/(\d+)/block-targets$#', $apiPath, $m) && $method === 'GET') {
        $matchApi->getBlockTargets((int) $m[1], (int) $m[2]);
        exit;
    }
    if (preg_match('#^/api/matches/(\d+)/players/(\d+)/pass-targets$#', $apiPath, $m) && $method === 'GET') {
        $matchApi->getPassTargets((int) $m[1], (int) $m[2]);
        exit;
    }
    if (preg_match('#^/api/matches/(\d+)/players/(\d+)/handoff-targets$#', $apiPath, $m) && $method === 'GET') {
        $matchApi->getHandOffTargets((int) $m[1], (int) $m[2]);
        exit;
    }
    if (preg_match('#^/api/matches/(\d+)/players/(\d+)/foul-targets$#', $apiPath, $m) && $method === 'GET') {
        $matchApi->getFoulTargets((int) $m[1], (int) $m[2]);
        exit;
    }
    if (preg_match('#^/api/matches/(\d+)/events$#', $apiPath, $m) && $method === 'GET') {
        $matchApi->getEvents((int) $m[1]);
        exit;
    }

    // 404 for unknown API routes
    http_response_code(404);
    header('Content-Type: application/json');
    echo json_encode(['error' => 'Not found']);
    exit;
}

// ========== Page Routes ==========
$page = $container->get(PageController::class);
$teamPage = $container->get(TeamPageController::class);

// Team routes (check before default)
if ($uri === '/teams' && $method === 'GET') {
    $teamPage->list();
    exit;
}
if ($uri === '/teams/create' && $method === 'GET') {
    $teamPage->createForm();
    exit;
}
if ($uri === '/teams/create' && $method === 'POST') {
    $teamPage->createAction();
    exit;
}
if (preg_match('#^/teams/(\d+)$#', $uri, $m) && $method === 'GET') {
    $teamPage->show((int) $m[1]);
    exit;
}
if (preg_match('#^/teams/(\d+)/hire$#', $uri, $m) && $method === 'POST') {
    $teamPage->hirePlayerAction((int) $m[1]);
    exit;
}
if (preg_match('#^/teams/(\d+)/players/(\d+)/fire$#', $uri, $m) && $method === 'POST') {
    $teamPage->firePlayerAction((int) $m[1], (int) $m[2]);
    exit;
}
if (preg_match('#^/teams/(\d+)/buy-reroll$#', $uri, $m) && $method === 'POST') {
    $teamPage->buyRerollAction((int) $m[1]);
    exit;
}
if (preg_match('#^/teams/(\d+)/buy-apothecary$#', $uri, $m) && $method === 'POST') {
    $teamPage->buyApothecaryAction((int) $m[1]);
    exit;
}
if (preg_match('#^/teams/(\d+)/retire$#', $uri, $m) && $method === 'POST') {
    $teamPage->retireAction((int) $m[1]);
    exit;
}

// Match routes
$matchPage = $container->get(MatchPageController::class);

if ($uri === '/matches' && $method === 'GET') {
    $matchPage->list();
    exit;
}
if ($uri === '/matches/new' && $method === 'GET') {
    $matchPage->newMatch();
    exit;
}
if ($uri === '/matches/new' && $method === 'POST') {
    $matchPage->createMatch();
    exit;
}
if (preg_match('#^/matches/(\d+)$#', $uri, $m) && $method === 'GET') {
    $matchPage->show((int) $m[1]);
    exit;
}

match (true) {
    $uri === '/' && $method === 'GET' => $page->dashboard(),
    $uri === '/login' && $method === 'GET' => $page->loginPage(),
    $uri === '/login' && $method === 'POST' => $page->loginAction(),
    $uri === '/register' && $method === 'GET' => $page->registerPage(),
    $uri === '/register' && $method === 'POST' => $page->registerAction(),
    $uri === '/logout' && $method === 'GET' => $page->logout(),
    $uri === '/races' && $method === 'GET' => $page->races(),
    $uri === '/match/demo' && $method === 'GET' => $page->matchDemo(),
    default => (function () {
        http_response_code(404);
        echo '404 Not Found';
    })(),
};
