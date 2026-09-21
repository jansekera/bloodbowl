<?php
declare(strict_types=1);

/**
 * DIAGNOSTIC ONLY - NOT for commit. Verifies package G persistence claims:
 *   - DEAD/game
 *   - players in Reserves (OFF_PITCH) at final whistle
 *   - whether KO'd players return between drives (track transitions)
 *   - bodies on pitch at start of 2nd half
 *
 * Usage: php cli/diag_package_g_20260914.php --matches=20
 */

require_once __DIR__ . '/../vendor/autoload.php';
require_once __DIR__ . '/dice_factory.php';

use App\AI\AICoachInterface;
use App\AI\GreedyAICoach;
use App\AI\LearningAICoach;
use App\AI\RandomAICoach;
use App\DTO\GameState;
use App\Engine\ActionResolver;
use App\Engine\RandomDiceRoller;
use App\Engine\RulesEngine;
use App\Enum\ActionType;
use App\Enum\GamePhase;
use App\Enum\PlayerState;
use App\Enum\TeamSide;

$options = getopt('', ['matches:', 'home-ai:', 'away-ai:']);
$numMatches = (int) ($options['matches'] ?? 20);
$homeAiType = $options['home-ai'] ?? 'learning';
$awayAiType = $options['away-ai'] ?? 'greedy';

require_once __DIR__ . '/race_rosters.php';
require_once __DIR__ . '/developed_rosters.php';

function createAI(string $type): AICoachInterface
{
    return match ($type) {
        'greedy' => new GreedyAICoach(),
        'random' => new RandomAICoach(),
        'learning' => new LearningAICoach(null, 0.0),
        default => throw new \InvalidArgumentException("Unknown AI type: {$type}"),
    };
}

$totalDead = 0;
$totalReservesAtEnd = 0;
$totalOnPitchStartOfHalf2 = 0;
$totalKoReturnEvents = 0;   // count of players observed as KO at some point, then OFF_PITCH later same match
$totalKoObserved = 0;
$totalInjuredAtEnd = 0;
$totalKoAtEnd = 0;
$totalStandingAtEnd = 0;
$gamesRun = 0;

for ($g = 0; $g < $numMatches; $g++) {
    $homeAi = createAI($homeAiType);
    $awayAi = createAI($awayAiType);
    $dice = bbDice($g);
    $rules = new RulesEngine();

    $homePlayers = getRaceRoster(TeamSide::HOME, 'Human');
    $awayPlayers = getRaceRoster(TeamSide::AWAY, 'Human');
    $players = $homePlayers + $awayPlayers;

    $homeTeam = \App\DTO\TeamStateDTO::create(1, 'Home', 'Human', TeamSide::HOME, 3);
    $awayTeam = \App\DTO\TeamStateDTO::create(2, 'Away', 'Human', TeamSide::AWAY, 3);

    $state = GameState::create(
        matchId: 1,
        homeTeam: $homeTeam,
        awayTeam: $awayTeam,
        players: $players,
        receivingTeam: TeamSide::HOME,
    );

    $state = $homeAi->setupFormation($state, TeamSide::HOME);
    $state = $awayAi->setupFormation($state, TeamSide::AWAY);

    $resolver = new ActionResolver($dice);
    $result = $resolver->resolve($state, ActionType::END_SETUP, []);
    $state = $result->getNewState();

    $totalActions = 0;
    $maxTotalActions = 2000;
    $turnActions = 0;
    $maxTurnActions = 50;

    $wasKoAtSomePoint = [];  // playerId => true, seen KO
    $sawHalf2Start = false;
    $onPitchAtHalf2 = null;

    while ($state->getPhase() !== GamePhase::GAME_OVER && $totalActions < $maxTotalActions) {
        // Track anyone currently KO
        foreach ($state->getPlayers() as $p) {
            if ($p->getState() === PlayerState::KO) {
                $wasKoAtSomePoint[$p->getId()] = true;
            }
        }

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

                // Capture bodies on pitch right after 2nd-half setup (both sides)
                if ($state->getHalf() === 2 && !$sawHalf2Start) {
                    $onH = count($state->getPlayersOnPitch(TeamSide::HOME));
                    $onA = count($state->getPlayersOnPitch(TeamSide::AWAY));
                    if ($onH > 0 || $onA > 0) {
                        // Only latch once both are set up (heuristic: after this end-setup call, check phase)
                        if (!$state->getPhase()->isSetup()) {
                            $onPitchAtHalf2 = $onH + $onA;
                            $sawHalf2Start = true;
                        }
                    }
                }
            } elseif ($state->getPhase() === GamePhase::HALF_TIME) {
                $htResult = $resolver->getGameFlowResolver()->resolveHalfTime($state);
                $state = $htResult['state'];
                $totalActions++;
            } else {
                break;
            }
            continue;
        }

        $activeTeam = $state->getActiveTeam();
        $ai = $activeTeam === TeamSide::HOME ? $homeAi : $awayAi;

        $decision = $ai->decideAction($state, $rules);
        $totalActions++;
        $turnActions++;

        if ($turnActions > $maxTurnActions) {
            $decision = ['action' => ActionType::END_TURN, 'params' => []];
            $turnActions = 0;
        }

        try {
            $result = $resolver->resolve($state, $decision['action'], $decision['params']);
        } catch (\Exception $e) {
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

        if ($state->getActiveTeam() !== $prevActiveTeam) {
            $turnActions = 0;
        }

        $gameFlow = $resolver->getGameFlowResolver();
        $scoringTeam = $gameFlow->checkTouchdown($state);
        if ($scoringTeam !== null) {
            $tdResult = $gameFlow->resolveTouchdown($state, $scoringTeam);
            $state = $tdResult['state'];
            $postResult = $gameFlow->resolvePostTouchdown($state);
            $state = $postResult['state'];
            $turnActions = 0;
        }
    }

    // Final tally for this match
    $deadThisGame = 0;
    $reservesThisGame = 0;
    $injuredThisGame = 0;
    $koThisGame = 0;
    $standingThisGame = 0;
    foreach ($state->getPlayers() as $p) {
        switch ($p->getState()) {
            case PlayerState::DEAD: $deadThisGame++; break;
            case PlayerState::OFF_PITCH: $reservesThisGame++; break;
            case PlayerState::INJURED: $injuredThisGame++; break;
            case PlayerState::KO: $koThisGame++; break;
            case PlayerState::STANDING:
            case PlayerState::PRONE:
            case PlayerState::STUNNED:
                $standingThisGame++; break;
        }
        if (isset($wasKoAtSomePoint[$p->getId()]) && $p->getState() === PlayerState::OFF_PITCH) {
            $totalKoReturnEvents++;
        }
    }
    $totalKoObserved += count($wasKoAtSomePoint);

    $totalDead += $deadThisGame;
    $totalReservesAtEnd += $reservesThisGame;
    $totalInjuredAtEnd += $injuredThisGame;
    $totalKoAtEnd += $koThisGame;
    $totalStandingAtEnd += $standingThisGame;
    if ($onPitchAtHalf2 !== null) {
        $totalOnPitchStartOfHalf2 += $onPitchAtHalf2;
    }
    $gamesRun++;

    fwrite(STDERR, sprintf(
        "game %d/%d: dead=%d reserves=%d injured=%d ko=%d standing=%d half2_onpitch=%s koObservedThisGame=%d koReturnedThisGame=%d\n",
        $g + 1, $numMatches, $deadThisGame, $reservesThisGame, $injuredThisGame, $koThisGame, $standingThisGame,
        $onPitchAtHalf2 === null ? 'N/A' : (string)$onPitchAtHalf2,
        count($wasKoAtSomePoint),
        array_sum(array_map(fn($pid) => (isset($wasKoAtSomePoint[$pid]) && $state->getPlayer($pid)?->getState() === PlayerState::OFF_PITCH) ? 1 : 0, array_keys($wasKoAtSomePoint)))
    ));
}

echo "\n=== SOUHRN ({$gamesRun} her) ===\n";
printf("DEAD/hru:                 %.4f (celkem %d)\n", $totalDead / max(1,$gamesRun), $totalDead);
printf("Reservy (OFF_PITCH) na konci zapasu / hru: %.3f (celkem %d)\n", $totalReservesAtEnd / max(1,$gamesRun), $totalReservesAtEnd);
printf("INJURED na konci / hru:   %.3f (celkem %d)\n", $totalInjuredAtEnd / max(1,$gamesRun), $totalInjuredAtEnd);
printf("KO na konci / hru:        %.3f (celkem %d)\n", $totalKoAtEnd / max(1,$gamesRun), $totalKoAtEnd);
printf("Standing/Prone/Stunned na konci / hru: %.3f (celkem %d)\n", $totalStandingAtEnd / max(1,$gamesRun), $totalStandingAtEnd);
printf("Hraci na hristi na zacatku 2. pulky (soucet obou tymu) / hru: %.3f\n", $totalOnPitchStartOfHalf2 / max(1,$gamesRun));
printf("KO pozorovano celkem (unikatni hraci-zapasy): %d, z toho vratilo se do OFF_PITCH do konce zapasu: %d (%.1f%%)\n",
    $totalKoObserved, $totalKoReturnEvents, $totalKoObserved > 0 ? 100.0 * $totalKoReturnEvents / $totalKoObserved : 0.0);
