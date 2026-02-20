<?php
declare(strict_types=1);

/**
 * Headless Blood Bowl match simulator.
 *
 * Usage: php cli/simulate.php --home-ai=greedy --away-ai=random [--matches=1] [--verbose]
 *        [--weights=weights.json] [--epsilon=0.0] [--log=<dir>]
 *        [--home-race=Human] [--away-race=Human]
 */

require_once __DIR__ . '/../vendor/autoload.php';

use App\AI\AICoachInterface;
use App\AI\GameLogger;
use App\AI\GreedyAICoach;
use App\AI\LearningAICoach;
use App\AI\RandomAICoach;
use App\DTO\GameState;
use App\DTO\MatchPlayerDTO;
use App\DTO\TeamStateDTO;
use App\Engine\ActionResolver;
use App\Engine\RandomDiceRoller;
use App\Engine\RulesEngine;
use App\Enum\ActionType;
use App\Enum\GamePhase;
use App\Enum\PlayerState;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use App\ValueObject\PlayerStats;
use App\ValueObject\Position;

// Parse arguments
$options = getopt('', ['home-ai:', 'away-ai:', 'matches:', 'verbose', 'weights:', 'epsilon:', 'log:', 'home-race:', 'away-race:', 'away-weights:', 'away-epsilon:', 'tv:']);
$homeAiType = $options['home-ai'] ?? 'greedy';
$awayAiType = $options['away-ai'] ?? 'random';
$numMatches = (int) ($options['matches'] ?? 1);
$verbose = isset($options['verbose']);
$weightsFile = $options['weights'] ?? null;
$epsilon = (float) ($options['epsilon'] ?? 0.0);
$logDir = $options['log'] ?? null;
$homeRace = $options['home-race'] ?? 'Human';
$awayRace = $options['away-race'] ?? 'Human';
$awayWeightsFile = $options['away-weights'] ?? $weightsFile;
$awayEpsilon = (float) ($options['away-epsilon'] ?? $epsilon);
$tv = (int) ($options['tv'] ?? 1000);

function createAI(string $type, ?string $weightsFile, float $epsilon): AICoachInterface
{
    return match ($type) {
        'greedy' => new GreedyAICoach(),
        'random' => new RandomAICoach(),
        'learning' => new LearningAICoach($weightsFile, $epsilon),
        default => throw new \InvalidArgumentException("Unknown AI type: {$type}"),
    };
}

require_once __DIR__ . '/race_rosters.php';
require_once __DIR__ . '/developed_rosters.php';

/** @var list<string> Available race names for random selection */
$availableRaces = array_keys(RACE_ROSTERS);

function simulateMatch(AICoachInterface $homeAi, AICoachInterface $awayAi, bool $verbose, ?GameLogger $logger, string $homeRace = 'Human', string $awayRace = 'Human', int $tv = 1000): array
{
    $dice = new RandomDiceRoller();
    $rules = new RulesEngine();

    // Build initial state
    if ($tv >= 1500) {
        $homePlayers = getDevelopedRaceRoster(TeamSide::HOME, $homeRace);
        $awayPlayers = getDevelopedRaceRoster(TeamSide::AWAY, $awayRace);
    } else {
        $homePlayers = getRaceRoster(TeamSide::HOME, $homeRace);
        $awayPlayers = getRaceRoster(TeamSide::AWAY, $awayRace);
    }
    $players = $homePlayers + $awayPlayers;

    $homeTeam = TeamStateDTO::create(1, "Home ({$homeRace})", $homeRace, TeamSide::HOME, 3);
    $awayTeam = TeamStateDTO::create(2, "Away ({$awayRace})", $awayRace, TeamSide::AWAY, 3);

    $state = GameState::create(
        matchId: 1,
        homeTeam: $homeTeam,
        awayTeam: $awayTeam,
        players: $players,
        receivingTeam: TeamSide::HOME,
    );

    // Setup phase: auto-setup both teams
    $state = $homeAi->setupFormation($state, TeamSide::HOME);
    $state = $awayAi->setupFormation($state, TeamSide::AWAY);

    // Kickoff
    $resolver = new ActionResolver($dice);
    $result = $resolver->resolve($state, ActionType::END_SETUP, []);
    $state = $result->getNewState();
    $gameFlow = $resolver->getGameFlowResolver();

    $totalActions = 0;
    $maxTotalActions = 2000;
    $turnActions = 0;
    $maxTurnActions = 50;

    while ($state->getPhase() !== GamePhase::GAME_OVER && $totalActions < $maxTotalActions) {
        if (!$state->getPhase()->isPlayable()) {
            if ($state->getPhase()->isSetup()) {
                $side = $state->getActiveTeam();
                $ai = $side === TeamSide::HOME ? $homeAi : $awayAi;
                $onPitch = $state->getPlayersOnPitch($side);
                if (count($onPitch) < 11) {
                    $state = $ai->setupFormation($state, $side);
                }
                $endSetupResult = $resolver->resolve($state, ActionType::END_SETUP, []);
                $state = $endSetupResult->getNewState();
                $totalActions++;
            } elseif ($state->getPhase() === GamePhase::HALF_TIME) {
                // Resolve half-time (KO recovery, reset for 2nd half)
                $htResult = $resolver->getGameFlowResolver()->resolveHalfTime($state);
                $state = $htResult['state'];
                $totalActions++;
                if ($verbose) {
                    fwrite(STDERR, "Half-time resolved, entering 2nd half setup.\n");
                }
            } else {
                if ($verbose) {
                    fwrite(STDERR, "Stuck in phase: {$state->getPhase()->value}\n");
                }
                break;
            }
            continue;
        }

        $activeTeam = $state->getActiveTeam();
        $ai = $activeTeam === TeamSide::HOME ? $homeAi : $awayAi;

        // Log state before AI decision
        if ($logger !== null) {
            $logger->logState($state, $activeTeam);
        }

        $decision = $ai->decideAction($state, $rules);
        $totalActions++;
        $turnActions++;

        if ($verbose && $totalActions % 50 === 0) {
            fwrite(STDERR, "  Actions: {$totalActions}, Half: {$state->getHalf()}, Turn H:{$state->getHomeTeam()->getTurnNumber()} A:{$state->getAwayTeam()->getTurnNumber()}, Active: {$activeTeam->value}, Action: {$decision['action']->value}\n");
        }

        // Force END_TURN if too many actions in one turn (safety valve)
        if ($turnActions > $maxTurnActions) {
            $decision = ['action' => ActionType::END_TURN, 'params' => []];
            $turnActions = 0;
        }

        try {
            $result = $resolver->resolve($state, $decision['action'], $decision['params']);
        } catch (\Exception $e) {
            if ($verbose) {
                fwrite(STDERR, "Error: {$e->getMessage()} - forcing END_TURN\n");
            }
            $result = $resolver->resolve($state, ActionType::END_TURN, []);
        }

        $prevActiveTeam = $activeTeam;
        $state = $result->getNewState();

        if ($result->isTurnover() || $decision['action'] === ActionType::END_TURN) {
            if ($decision['action'] !== ActionType::END_TURN) {
                $endResult = $resolver->resolve($state, ActionType::END_TURN, []);
                $state = $endResult->getNewState();
            }
            $turnActions = 0;
        }

        // Reset turn actions when active team changes
        if ($state->getActiveTeam() !== $prevActiveTeam) {
            $turnActions = 0;
        }

        // Check touchdown
        $gameFlow = $resolver->getGameFlowResolver();
        $scoringTeam = $gameFlow->checkTouchdown($state);
        if ($scoringTeam !== null) {
            $tdResult = $gameFlow->resolveTouchdown($state, $scoringTeam);
            $state = $tdResult['state'];

            $postResult = $gameFlow->resolvePostTouchdown($state);
            $state = $postResult['state'];
            $turnActions = 0;

            if ($verbose) {
                fwrite(STDERR, "TOUCHDOWN by {$scoringTeam->value}! Score: {$state->getHomeTeam()->getScore()} - {$state->getAwayTeam()->getScore()}\n");
            }
        }
    }

    // Log final result
    if ($logger !== null) {
        $logger->logResult($state);
        $logger->close();
    }

    return [
        'homeScore' => $state->getHomeTeam()->getScore(),
        'awayScore' => $state->getAwayTeam()->getScore(),
        'totalActions' => $totalActions,
        'phase' => $state->getPhase()->value,
        'half' => $state->getHalf(),
    ];
}

// --- Main ---

$homeAi = createAI($homeAiType, $weightsFile, $epsilon);
$awayAi = createAI($awayAiType, $awayWeightsFile, $awayEpsilon);

$results = [];
$homeWins = 0;
$awayWins = 0;
$draws = 0;

// Prepare log directory
if ($logDir !== null && !is_dir($logDir)) {
    mkdir($logDir, 0777, true);
}

for ($i = 0; $i < $numMatches; $i++) {
    // Resolve "random" race per game
    $matchHomeRace = $homeRace === 'random' ? $availableRaces[array_rand($availableRaces)] : $homeRace;
    $matchAwayRace = $awayRace === 'random' ? $availableRaces[array_rand($availableRaces)] : $awayRace;

    if ($verbose) {
        fwrite(STDERR, "Match " . ($i + 1) . "/{$numMatches} ({$matchHomeRace} vs {$matchAwayRace})...\n");
    }

    $logger = null;
    if ($logDir !== null) {
        $gameNum = str_pad((string) ($i + 1), 3, '0', STR_PAD_LEFT);
        $logger = new GameLogger("{$logDir}/game_{$gameNum}.jsonl");
    }

    $gameStart = microtime(true);
    $result = simulateMatch($homeAi, $awayAi, $verbose, $logger, $matchHomeRace, $matchAwayRace, $tv);
    $results[] = $result;

    if ($result['homeScore'] > $result['awayScore']) {
        $homeWins++;
    } elseif ($result['awayScore'] > $result['homeScore']) {
        $awayWins++;
    } else {
        $draws++;
    }

    // Progress to stderr
    $elapsed = round(microtime(true) - $gameStart, 1);
    $score = "{$result['homeScore']}-{$result['awayScore']}";
    fwrite(STDERR, "GAME_DONE|" . ($i + 1) . "|{$numMatches}|{$elapsed}|{$score}\n");
}

$output = [
    'homeAi' => $homeAiType,
    'awayAi' => $awayAiType,
    'homeRace' => $homeRace,
    'awayRace' => $awayRace,
    'matches' => $numMatches,
    'homeWins' => $homeWins,
    'awayWins' => $awayWins,
    'draws' => $draws,
    'results' => $results,
];

echo json_encode($output, JSON_PRETTY_PRINT) . "\n";
