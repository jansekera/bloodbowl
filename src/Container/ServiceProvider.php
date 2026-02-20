<?php

declare(strict_types=1);

namespace App\Container;

use App\AI\AICoachInterface;
use App\AI\GreedyAICoach;
use App\AI\LearningAICoach;
use App\Controller\ApiController;
use App\Controller\MatchApiController;
use App\Controller\MatchPageController;
use App\Controller\PageController;
use App\Controller\TeamApiController;
use App\Controller\TeamPageController;
use App\Engine\ActionResolver;
use App\Engine\BallResolver;
use App\Engine\DiceRollerInterface;
use App\Engine\GameFlowResolver;
use App\Engine\InjuryResolver;
use App\Engine\KickoffResolver;
use App\Engine\Pathfinder;
use App\Engine\PassResolver;
use App\Engine\RandomDiceRoller;
use App\Engine\RulesEngine;
use App\Engine\ScatterCalculator;
use App\Engine\StrengthCalculator;
use App\Engine\TacklezoneCalculator;
use App\Repository\CoachRepository;
use App\Repository\MatchEventRepository;
use App\Repository\MatchPlayerRepository;
use App\Repository\MatchRepository;
use App\Repository\PlayerRepository;
use App\Repository\RaceRepository;
use App\Repository\SkillRepository;
use App\Repository\TeamRepository;
use App\Event\EventDispatcher;
use App\Event\EventDispatcherInterface;
use App\Event\GameEventOccurred;
use App\Event\Listener\EventCollector;
use App\Event\Listener\GameLogListener;
use App\Service\AuthService;
use App\Service\MatchService;
use App\Service\SPPService;
use App\Service\TeamService;
use App\Validation\RosterValidator;
use Twig\Environment;
use Twig\Loader\FilesystemLoader;

final class ServiceProvider
{
    public static function register(Container $container): void
    {
        // Twig
        $container->set(Environment::class, function () {
            $loader = new FilesystemLoader(__DIR__ . '/../../templates');
            return new Environment($loader, ['cache' => false]);
        });

        // Repositories
        $container->set(CoachRepository::class, fn() => new CoachRepository());
        $container->set(RaceRepository::class, fn() => new RaceRepository());
        $container->set(SkillRepository::class, fn() => new SkillRepository());
        $container->set(TeamRepository::class, fn() => new TeamRepository());
        $container->set(PlayerRepository::class, fn() => new PlayerRepository());
        $container->set(MatchRepository::class, fn() => new MatchRepository());
        $container->set(MatchPlayerRepository::class, fn() => new MatchPlayerRepository());
        $container->set(MatchEventRepository::class, fn() => new MatchEventRepository());

        // Engine
        $container->set(DiceRollerInterface::class, fn() => new RandomDiceRoller());
        $container->set(TacklezoneCalculator::class, fn() => new TacklezoneCalculator());
        $container->set(ScatterCalculator::class, fn() => new ScatterCalculator());
        $container->set(StrengthCalculator::class, fn() => new StrengthCalculator());
        $container->set(InjuryResolver::class, fn() => new InjuryResolver());
        $container->set(Pathfinder::class, fn(Container $c) => new Pathfinder(
            $c->get(TacklezoneCalculator::class),
        ));
        $container->set(BallResolver::class, fn(Container $c) => new BallResolver(
            $c->get(DiceRollerInterface::class),
            $c->get(TacklezoneCalculator::class),
            $c->get(ScatterCalculator::class),
        ));
        $container->set(PassResolver::class, fn(Container $c) => new PassResolver(
            $c->get(DiceRollerInterface::class),
            $c->get(TacklezoneCalculator::class),
            $c->get(ScatterCalculator::class),
            $c->get(BallResolver::class),
        ));
        $container->set(KickoffResolver::class, fn(Container $c) => new KickoffResolver(
            $c->get(DiceRollerInterface::class),
            $c->get(ScatterCalculator::class),
            $c->get(BallResolver::class),
        ));
        $container->set(GameFlowResolver::class, fn(Container $c) => new GameFlowResolver(
            $c->get(DiceRollerInterface::class),
        ));
        $container->set(ActionResolver::class, fn(Container $c) => new ActionResolver(
            $c->get(DiceRollerInterface::class),
            $c->get(TacklezoneCalculator::class),
            $c->get(Pathfinder::class),
            $c->get(StrengthCalculator::class),
            $c->get(InjuryResolver::class),
            $c->get(BallResolver::class),
            $c->get(ScatterCalculator::class),
            $c->get(PassResolver::class),
            $c->get(KickoffResolver::class),
            $c->get(GameFlowResolver::class),
        ));
        $container->set(RulesEngine::class, fn(Container $c) => new RulesEngine(
            $c->get(TacklezoneCalculator::class),
            $c->get(Pathfinder::class),
        ));

        // Validation
        $container->set(RosterValidator::class, fn(Container $c) => new RosterValidator(
            $c->get(PlayerRepository::class),
        ));

        // AI — use LearningAICoach if trained weights exist, otherwise GreedyAICoach
        $container->set(AICoachInterface::class, function () {
            $weightsFile = __DIR__ . '/../../weights.json';
            if (file_exists($weightsFile)) {
                return new LearningAICoach($weightsFile, 0.0);
            }
            return new GreedyAICoach();
        });

        // Event system
        $container->set(EventDispatcherInterface::class, function (Container $c) {
            $dispatcher = new EventDispatcher();
            $dispatcher->subscribe(GameEventOccurred::class, new GameLogListener(
                $c->get(MatchEventRepository::class),
            ));
            return $dispatcher;
        });

        // Services
        $container->set(AuthService::class, fn(Container $c) => new AuthService(
            $c->get(CoachRepository::class),
        ));
        $container->set(SPPService::class, fn() => new SPPService());
        $container->set(TeamService::class, fn(Container $c) => new TeamService(
            $c->get(TeamRepository::class),
            $c->get(PlayerRepository::class),
            $c->get(RaceRepository::class),
            $c->get(RosterValidator::class),
            $c->get(SkillRepository::class),
            $c->get(SPPService::class),
        ));
        $container->set(MatchService::class, function (Container $c) {
            $service = new MatchService(
                $c->get(MatchRepository::class),
                $c->get(MatchPlayerRepository::class),
                $c->get(MatchEventRepository::class),
                $c->get(TeamRepository::class),
                $c->get(RulesEngine::class),
                $c->get(DiceRollerInterface::class),
                $c->get(AICoachInterface::class),
                $c->get(SPPService::class),
                $c->get(PlayerRepository::class),
            );
            $service->setLogDir(__DIR__ . '/../../logs/games');
            return $service;
        });

        // Controllers
        $container->set(ApiController::class, fn(Container $c) => new ApiController(
            $c->get(RaceRepository::class),
            $c->get(SkillRepository::class),
        ));
        $container->set(TeamApiController::class, fn(Container $c) => new TeamApiController(
            $c->get(AuthService::class),
            $c->get(TeamService::class),
            $c->get(TeamRepository::class),
        ));
        $container->set(TeamPageController::class, fn(Container $c) => new TeamPageController(
            $c->get(AuthService::class),
            $c->get(TeamService::class),
            $c->get(TeamRepository::class),
            $c->get(RaceRepository::class),
            $c->get(Environment::class),
        ));
        $container->set(MatchApiController::class, fn(Container $c) => new MatchApiController(
            $c->get(AuthService::class),
            $c->get(MatchService::class),
        ));
        $container->set(MatchPageController::class, fn(Container $c) => new MatchPageController(
            $c->get(AuthService::class),
            $c->get(MatchService::class),
            $c->get(TeamRepository::class),
            $c->get(MatchRepository::class),
            $c->get(Environment::class),
        ));
        $container->set(PageController::class, fn(Container $c) => new PageController(
            $c->get(AuthService::class),
            $c->get(RaceRepository::class),
            $c->get(Environment::class),
        ));
    }
}
