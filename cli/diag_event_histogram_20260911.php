<?php
declare(strict_types=1);

/**
 * HISTOGRAM TYPŮ UDÁLOSTÍ — A HLAVNĚ SEZNAM TĚCH, KTERÉ NENASTALY (11.09.2026)
 *
 * ⭐ PROČ VZNIKL: uživatel se zeptal, proč pořád nacházíme nesoulad s pravidly,
 *   když prohlídka „proti pravidlům" byla vedená jako první skupina úkolů.
 *   Odpověď je, že vady nenacházíme ČTENÍM, ale měřením a jeho vlastními
 *   větami. Chybí detektor CELÝCH CHYBĚJÍCÍCH TŘÍD.
 *
 * ⛔ DOKLAD, ŽE BY FUNGOVAL: `PHP27` (sražený hráč nedostal hod na brnění) by
 *   z tohohle výpisu vypadl sám — na cestě `move` by kategorie `armour_roll`
 *   NEBYLA VŮBEC, přestože `move -> player_fell` je 64,8 % všech turnoverů.
 *
 * ⛔ JMENOVATEL SE NESMÍ ZTRATIT: tiskne se počet her, kol, rozhodnutí
 *   i událostí, a u rozpadu podle akcí i ZBYTEK.
 *
 * Použití: php cli/diag_event_histogram_20260911.php [kouc] [her] [seed]
 */

require_once __DIR__ . '/../vendor/autoload.php';

use App\AI\{AICoachInterface, GreedyAICoach, LearningAICoach, RandomAICoach};
use App\DTO\GameState;
use App\Engine\{ActionResolver, FixedDiceRoller, RandomDiceRoller, RulesEngine};
use App\Enum\{ActionType, GameEventType, GamePhase, TeamSide};

require_once __DIR__ . '/race_rosters.php';

$coachName = (string) ($argv[1] ?? 'learning');
$games     = (int) ($argv[2] ?? 30);
$seed      = (int) ($argv[3] ?? 20260911);

$makeCoach = static function () use ($coachName): AICoachInterface {
    return match ($coachName) {
        'greedy'   => new GreedyAICoach(),
        'learning' => new LearningAICoach(__DIR__ . '/../weights.json', 0.0),
        'random'   => new RandomAICoach(),
        default    => throw new \InvalidArgumentException("neznamy kouc: {$coachName}"),
    };
};

// ─────────────────────────────────────────────────────────────────────────────
// POZITIVNÍ KONTROLA MĚŘIDLA — BĚŽÍ PŘED KORPUSEM
// ⭐ Prázdný koš má cenu jedině tehdy, když měřidlo umí ukázat plný.
// ─────────────────────────────────────────────────────────────────────────────
echo "POZITIVNÍ KONTROLA MĚŘIDLA (běží PŘED korpusem):\n";
$kontrola = static function (string $co, bool $ok, string $detail): void {
    printf("    [%s] %s: %s\n", $ok ? 'OK ' : 'CHYBA', $co, $detail);
    if (!$ok) { fwrite(STDERR, "MERIDLO JE VADNE, koncim\n"); exit(9); }
};

// (a) Neúspěšný dodge musí vyrobit `armour_roll` — to je přesně ta kategorie,
//     která do 11.09. chyběla. Když ji měřidlo nevidí ani tady, nevidí nic.
$s = (new \App\Tests\Engine\GameStateBuilder())
    ->addPlayer(TeamSide::HOME, 5, 7, movement: 6, id: 1)
    ->addPlayer(TeamSide::AWAY, 6, 7, id: 2)
    ->addPlayer(TeamSide::AWAY, 6, 8, id: 3)
    ->withBallOffPitch()->build();
$s = $s->withTeamState(TeamSide::HOME, $s->getTeamState(TeamSide::HOME)->withRerolls(0));
$r = (new ActionResolver(new FixedDiceRoller([1, 1, 1])))
        ->resolve($s, ActionType::MOVE, ['playerId' => 1, 'x' => 4, 'y' => 6]);
$typyKontrola = array_map(static fn($e) => $e->getType(), $r->getEvents());
$kontrola('umí najít armour_roll po pádu', in_array('armour_roll', $typyKontrola, true),
    implode(', ', $typyKontrola));

// (b) A musí umět říct i NENASTALO: nad týmž výsledkem tam `touchdown` být nesmí.
$kontrola('umí říct „nenastalo"', !in_array('touchdown', $typyKontrola, true),
    'touchdown v tomhle výsledku není');
echo "  ⇒ měřidlo umí najít jedničku i nulu.\n\n";

mt_srand($seed);
$races = array_keys(RACE_ROSTERS);

$celkem      = ['hry' => 0, 'kola' => 0, 'rozhodnuti' => 0, 'udalosti' => 0];
$podleTypu   = [];   // typ události -> kolikrát
$podleAkce   = [];   // akce -> typ události -> kolikrát
$vyjimky     = 0;

for ($g = 0; $g < $games; $g++) {
    $homeRace = $races[mt_rand(0, count($races) - 1)];
    $awayRace = $races[mt_rand(0, count($races) - 1)];

    $dice = new RandomDiceRoller();
    $rules = new RulesEngine();
    $homeAi = $makeCoach();
    $awayAi = $makeCoach();

    $players = getRaceRoster(TeamSide::HOME, $homeRace) + getRaceRoster(TeamSide::AWAY, $awayRace);
    $state = GameState::create(
        matchId: 1,
        homeTeam: \App\DTO\TeamStateDTO::create(1, 'H', $homeRace, TeamSide::HOME, 3),
        awayTeam: \App\DTO\TeamStateDTO::create(2, 'A', $awayRace, TeamSide::AWAY, 3),
        players: $players,
        receivingTeam: TeamSide::HOME,
    );
    $state = $homeAi->setupFormation($state, TeamSide::HOME);
    $state = $awayAi->setupFormation($state, TeamSide::AWAY);

    $resolver = new ActionResolver($dice);
    $state = $resolver->resolve($state, ActionType::END_SETUP, [])->getNewState();
    $gameFlow = $resolver->getGameFlowResolver();
    $celkem['hry']++;

    $total = 0; $turnActions = 0; $klic = null;
    while ($state->getPhase() !== GamePhase::GAME_OVER && $total < 2000) {
        if (!$state->getPhase()->isPlayable()) {
            if ($state->getPhase()->isSetup()) {
                $side = $state->getActiveTeam();
                $ai = $side === TeamSide::HOME ? $homeAi : $awayAi;
                if (count($state->getPlayersOnPitch($side)) < 11) {
                    $state = $ai->setupFormation($state, $side);
                }
                $state = $resolver->resolve($state, ActionType::END_SETUP, [])->getNewState();
                $total++;
            } elseif ($state->getPhase() === GamePhase::HALF_TIME) {
                $state = $gameFlow->resolveHalfTime($state)['state'];
                $total++;
            } else {
                break;
            }
            continue;
        }

        $side = $state->getActiveTeam();
        $ai = $side === TeamSide::HOME ? $homeAi : $awayAi;

        $novyKlic = $state->getHalf() . '/' . $side->value . '/' .
            $state->getTeamState($side)->getTurnNumber();
        if ($klic !== null && $novyKlic !== $klic) { $celkem['kola']++; }
        $klic = $novyKlic;

        $decision = $ai->decideAction($state, $rules);
        $total++; $turnActions++; $celkem['rozhodnuti']++;

        if ($turnActions > 50) { $decision = ['action' => ActionType::END_TURN, 'params' => []]; $turnActions = 0; }

        try {
            $result = $resolver->resolve($state, $decision['action'], $decision['params']);
        } catch (\Exception $e) {
            $vyjimky++;
            $result = $resolver->resolve($state, ActionType::END_TURN, []);
        }

        $akce = $decision['action']->value;
        foreach ($result->getEvents() as $ev) {
            $t = $ev->getType();
            $podleTypu[$t] = ($podleTypu[$t] ?? 0) + 1;
            $podleAkce[$akce][$t] = ($podleAkce[$akce][$t] ?? 0) + 1;
            $celkem['udalosti']++;
        }

        $state = $result->getNewState();
        if ($result->isTurnover()) {
            $state = $resolver->resolve($state, ActionType::END_TURN, [])->getNewState();
            $turnActions = 0;
        }
        $sc = $gameFlow->checkTouchdown($state);
        if ($sc !== null) {
            $state = $gameFlow->resolveTouchdown($state, $sc)['state'];
            $state = $gameFlow->resolvePostTouchdown($state)['state'];
            $turnActions = 0;
        }
        if ($decision['action'] === ActionType::END_TURN) { $turnActions = 0; }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
echo "KOUČ: {$coachName}   KORPUS: {$games} her, seed {$seed}\n\n";
printf("JMENOVATELÉ: %d her · %d kol · %d rozhodnutí · %d událostí · %d výjimek\n\n",
    $celkem['hry'], $celkem['kola'], $celkem['rozhodnuti'], $celkem['udalosti'], $vyjimky);

arsort($podleTypu);
echo "TYPY UDÁLOSTÍ, KTERÉ NASTALY (" . count($podleTypu) . "):\n";
foreach ($podleTypu as $t => $n) {
    printf("    %-24s %8d   %6.2f na 1000 rozhodnutí\n",
        $t, $n, $celkem['rozhodnuti'] > 0 ? $n * 1000 / $celkem['rozhodnuti'] : 0);
}

// ⭐⭐⭐ TOHLE JE TEN VÝSTUP, KVŮLI KTERÉMU SKRIPT VZNIKL.
$vsechny = array_column(GameEventType::cases(), 'value');
$chybi = array_values(array_diff($vsechny, array_keys($podleTypu)));
sort($chybi);
echo "\n⛔ TYPY, KTERÉ NENASTALY ANI JEDNOU (" . count($chybi) . " z " . count($vsechny) . "):\n";
foreach ($chybi as $t) { echo "    {$t}\n"; }
echo "\n   ⚠️ Každý řádek je otázka, ne nález: buď to korpus nedosáhl (vzácná\n";
echo "      dovednost, žádný roster ji nemá), nebo tu cestu engine NEUMÍ.\n";
echo "      Rozhoduje se to čtením, ne odhadem.\n";

echo "\nROZPAD PODLE AKCE (co která akce vůbec vyrábí):\n";
ksort($podleAkce);
foreach ($podleAkce as $akce => $typy) {
    arsort($typy);
    $sum = array_sum($typy);
    printf("  %s  (%d událostí)\n", $akce, $sum);
    foreach ($typy as $t => $n) { printf("      %-24s %7d\n", $t, $n); }
}
