<?php
declare(strict_types=1);

/**
 * PHP16 — ZMĚNÍ NATRÉNOVANÁ VALUE HLAVA VŮBEC NĚJAKÉ ROZHODNUTÍ? (13.09.2026)
 *
 * Předregistrace: `evidence/PREREG_php16_value_head_20260911.md`
 * PŘEDPOVĚĎ ZAPSANÁ PŘED BĚHEM: **přesně 0 změněných rozhodnutí**, protože
 * `evaluateState` se volá jen nad SOUČASNÝM stavem, takže `baseScore` je pro
 * všechny kandidáty stejný a v argmaxu se vykrátí.
 *
 * Měří se PÁROVĚ nad TÝMŽ stavem: kouč s nulovými vahami vs. kouč s náhodnými
 * nenulovými. Rozhodnutí se porovnává včetně parametrů.
 */

require_once __DIR__ . '/../vendor/autoload.php';

use App\AI\LearningAICoach;
use App\DTO\GameState;
use App\Engine\{ActionResolver, RandomDiceRoller, RulesEngine};
use App\Enum\{ActionType, GamePhase, TeamSide};

require_once __DIR__ . '/race_rosters.php';
require_once __DIR__ . '/developed_rosters.php';

$games = (int) ($argv[1] ?? 4);
$seed  = (int) ($argv[2] ?? 20260913);

// Kouč B dostane náhodné NENULOVÉ value váhy.
mt_srand($seed);
$vahy = [];
for ($i = 0; $i < 73; $i++) {
    $vahy[] = round((mt_rand(-1000, 1000) / 1000), 3);
}
$soubor = sys_get_temp_dir() . '/probe_value_weights.json';
file_put_contents($soubor, json_encode(['type' => 'alphazero_linear', 'value_weights' => $vahy]));

$A = new LearningAICoach(__DIR__ . '/../weights.json', 0.0);   // nuly
$B = new LearningAICoach($soubor, 0.0);                        // nenulové

echo "POZITIVNÍ KONTROLA MĚŘIDLA (běží PŘED korpusem):\n";
$stav = (new \App\Tests\Engine\GameStateBuilder())
    ->addPlayer(TeamSide::HOME, 10, 7, movement: 6, id: 1)
    ->addPlayer(TeamSide::AWAY, 14, 7, id: 2)
    ->withBallCarried(1)->build();
$ev = new ReflectionMethod(LearningAICoach::class, 'evaluateState');
$hodnotaA = (float) $ev->invoke($A, $stav, TeamSide::HOME);
$hodnotaB = (float) $ev->invoke($B, $stav, TeamSide::HOME);
printf("    [%s] váhy kouče B se opravdu načetly: A = %.4f, B = %.4f\n",
    ($hodnotaA === 0.0 && $hodnotaB !== 0.0) ? 'OK ' : 'CHYBA', $hodnotaA, $hodnotaB);
if ($hodnotaA !== 0.0 || $hodnotaB === 0.0) {
    fwrite(STDERR, "MERIDLO JE VADNE: bez rozdilu ve vahach nulu cist nelze\n");
    exit(9);
}

// (b) umí čítač vůbec najít rozdíl? Kouč proti sobě s epsilon = 1 (náhodný výběr).
$nahodny = new LearningAICoach(__DIR__ . '/../weights.json', 1.0);
$rules = new RulesEngine();
$rozdilKontrola = 0;
for ($i = 0; $i < 40; $i++) {
    $a = $A->decideAction($stav, $rules);
    $b = $nahodny->decideAction($stav, $rules);
    if ($a != $b) { $rozdilKontrola++; }
}
printf("    [%s] čítač umí najít rozdíl: %d ze 40 při epsilon = 1\n",
    $rozdilKontrola > 0 ? 'OK ' : 'CHYBA', $rozdilKontrola);
if ($rozdilKontrola === 0) { fwrite(STDERR, "MERIDLO NEUMI NAJIT JEDNICKU\n"); exit(9); }
echo "  ⇒ měřidlo umí rozlišit načtené váhy i rozdílné rozhodnutí.\n\n";

// ─── KORPUS ──────────────────────────────────────────────────────────────────
$races = array_keys(RACE_ROSTERS);
$stat = ['rozhodnuti' => 0, 'shodne' => 0, 'rozdilne' => 0];

for ($g = 0; $g < $games; $g++) {
    $homeRace = $races[mt_rand(0, count($races) - 1)];
    $awayRace = $races[mt_rand(0, count($races) - 1)];
    $dice = new RandomDiceRoller();
    $players = getDevelopedRaceRoster(TeamSide::HOME, $homeRace)
        + getDevelopedRaceRoster(TeamSide::AWAY, $awayRace);
    $state = GameState::create(1,
        \App\DTO\TeamStateDTO::create(1, 'H', $homeRace, TeamSide::HOME, 3),
        \App\DTO\TeamStateDTO::create(2, 'A', $awayRace, TeamSide::AWAY, 3),
        $players, TeamSide::HOME);
    $state = $A->setupFormation($state, TeamSide::HOME);
    $state = $A->setupFormation($state, TeamSide::AWAY);
    $resolver = new ActionResolver($dice);
    $state = $resolver->resolve($state, ActionType::END_SETUP, [])->getNewState();
    $gameFlow = $resolver->getGameFlowResolver();

    $total = 0;
    while ($state->getPhase() !== GamePhase::GAME_OVER && $total < 1500) {
        if (!$state->getPhase()->isPlayable()) {
            if ($state->getPhase()->isSetup()) {
                $side = $state->getActiveTeam();
                if (count($state->getPlayersOnPitch($side)) < 11) {
                    $state = $A->setupFormation($state, $side);
                }
                $state = $resolver->resolve($state, ActionType::END_SETUP, [])->getNewState();
            } elseif ($state->getPhase() === GamePhase::HALF_TIME) {
                $state = $gameFlow->resolveHalfTime($state)['state'];
            } else { break; }
            $total++;
            continue;
        }

        // ⭐ PÁROVĚ NAD TÝMŽ STAVEM
        $rozA = $A->decideAction($state, $rules);
        $rozB = $B->decideAction($state, $rules);
        $stat['rozhodnuti']++;
        if ($rozA == $rozB) { $stat['shodne']++; } else { $stat['rozdilne']++; }

        $vysl = $resolver->resolve($state, $rozA['action'], $rozA['params']);
        $state = $vysl->getNewState();
        if ($vysl->isTurnover()) {
            $state = $resolver->resolve($state, ActionType::END_TURN, [])->getNewState();
        }
        $sc = $gameFlow->checkTouchdown($state);
        if ($sc !== null) {
            $state = $gameFlow->resolveTouchdown($state, $sc)['state'];
            $state = $gameFlow->resolvePostTouchdown($state)['state'];
        }
        $total++;
    }
}

printf("KORPUS: %d her, seed %d\n\n", $games, $seed);
printf("ROZHODNUTÍ CELKEM        %6d\n", $stat['rozhodnuti']);
printf("  shodná                 %6d\n", $stat['shodne']);
printf("  ⭐ ROZDÍLNÁ             %6d   %5.2f %%\n", $stat['rozdilne'],
    $stat['rozhodnuti'] > 0 ? 100 * $stat['rozdilne'] / $stat['rozhodnuti'] : 0);
printf("  ZBYTEK (musí být 0)    %6d\n\n",
    $stat['rozhodnuti'] - $stat['shodne'] - $stat['rozdilne']);
echo $stat['rozdilne'] === 0
    ? "⇒ PŘEDPOVĚĎ POTVRZENA: natrénovaná value hlava nezmění ANI JEDNO rozhodnutí.\n"
      . "   Trénink ani oprava loaderu nemá co ovlivnit, dokud skórer hodnotí\n"
      . "   jen SOUČASNÝ stav (`baseScore` se v argmaxu vykrátí).\n"
    : "⇒ PŘEDPOVĚĎ VYVRÁCENA: cesta, kterou váhy ovlivňují rozhodnutí, existuje.\n"
      . "   Najít ji PŘED utracením výpočetního času na trénink.\n";
