<?php
declare(strict_types=1);

/**
 * KOLIKRÁT `GreedyAICoach` UKONČÍ KOLO, PŘESTOŽE JE CO HRÁT? (11.09.2026)
 *
 * Položka #1 auditu „kde engine ukončuje tah" (10.09.). PHP je to, co hraje
 * člověk, takže tahle vada se projevuje v reálné hře, ne jen v korpusu.
 *
 * ⭐ MĚŘÍ SE ZVENČÍ, produkční kód se nemění. Jde to proto, že
 *   `GreedyAICoach::decideAction` nad END_TURN v nabídce dělá `continue`
 *   ⇒ END_TURN může vrátit JEDINĚ tím počátečním `$bestAction`, tedy
 *   fallbackem. „Vrátil END_TURN" a „sepnul fallback" jsou tedy totéž.
 *
 * ⛔ JMENOVATEL SE NESMÍ ZTRATIT: každé rozhodnutí padne právě do jednoho
 *   koše a na konci se tiskne ZBYTEK, který musí být 0.
 *
 * Použití: php cli/diag_ai_endturn_20260911.php [her] [seed]
 */

require_once __DIR__ . '/../vendor/autoload.php';

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
use App\Enum\TeamSide;

require_once __DIR__ . '/race_rosters.php';
require_once __DIR__ . '/developed_rosters.php';

/**
 * Kolik akcí v nabídce by skórer vůbec vzal do ruky.
 * Musí to kopírovat filtr z `decideAction` (END_TURN / SETUP / bez playerId),
 * jinak by se počítalo něco jiného, než o čem se rozhoduje.
 */
function playableOffered(array $actions): int
{
    $n = 0;
    foreach ($actions as $a) {
        $type = ActionType::from($a['type']);
        if ($type === ActionType::END_TURN) continue;
        // ⛔ PHP25 (11.09.): STAND_PAT je v nabidce od te doby, co engine umi
        //   "nic nedelat" -- ale kouc ho ZATIM ignoruje, takze do mericka
        //   nepatri. Kdyby se pocital, "nabidka nemela nic hratelneho" by uz
        //   nenastalo NIKDY a kos "END_TURN pravem" by se tise vynuloval.
        if ($type === ActionType::STAND_PAT) continue;
        if ($type === ActionType::SETUP_PLAYER || $type === ActionType::END_SETUP) continue;
        if (($a['playerId'] ?? null) === null) continue;
        $n++;
    }
    return $n;
}

/** Kolik hráčů týmu ještě mohlo jednat = velikost ztráty při ukončení kola. */
function idleActors(GameState $state, TeamSide $side): int
{
    $n = 0;
    foreach ($state->getPlayersOnPitch($side) as $p) {
        if ($p->canAct() && !$p->hasActed()) $n++;
    }
    return $n;
}

// ---------------------------------------------------------------------------
// ⭐⭐⭐ POZITIVNÍ KONTROLA MĚŘIDLA — BĚŽÍ PŘED KORPUSEM
//   Bez ní se nedá odlišit „vada nenastává" od „detektor je slepý".
// ---------------------------------------------------------------------------
echo "POZITIVNÍ KONTROLA MĚŘIDLA (běží PŘED korpusem):\n";
$ok = true;
$check = function (string $name, $got, $want) use (&$ok) {
    $good = $got === $want;
    if (!$good) $ok = false;
    printf("    [%s] %s: dostal %s, čekal %s\n",
        $good ? 'OK ' : 'VADA', $name, var_export($got, true), var_export($want, true));
};
// detektor hratelných akcí musí najít jedničku...
$check('playableOffered najde MOVE', playableOffered([
    ['type' => 'end_turn'],
    ['type' => 'move', 'playerId' => 3],
]), 1);
// ...i nulu, když je v nabídce jen END_TURN
$check('playableOffered na samotném END_TURN', playableOffered([
    ['type' => 'end_turn'],
]), 0);
// ...a nesmí počítat akce bez hráče ani setup
$check('playableOffered ignoruje setup', playableOffered([
    ['type' => 'end_turn'],
    ['type' => 'setup_player', 'playerId' => 3],
    ['type' => 'end_setup'],
]), 0);
$check('playableOffered ignoruje akci bez playerId', playableOffered([
    ['type' => 'move'],
]), 0);
echo $ok ? "  ⇒ měřidlo umí najít jedničku i nulu.\n\n"
         : "  ⛔ MĚŘIDLO JE ROZBITÉ, čísla níž nemají smysl.\n\n";

// ---------------------------------------------------------------------------
$coachName = (string) ($argv[1] ?? 'greedy');
$games = (int) ($argv[2] ?? 30);
$seed  = (int) ($argv[3] ?? 20260911);

// ⭐ Kouč se vyrábí TOUTÉŽ cestou jako v aplikaci, ať se neměří něco jiného,
//    než co hraje člověk. `ServiceProvider.php:121-127`: když existuje
//    `weights.json`, aplikace bere LearningAICoach, jinak GreedyAICoach.
$makeCoach = static function () use ($coachName): AICoachInterface {
    return match ($coachName) {
        'greedy'   => new GreedyAICoach(),
        'learning' => new LearningAICoach(__DIR__ . '/../weights.json', 0.0),
        'random'   => new RandomAICoach(),
        default    => throw new \InvalidArgumentException("neznamy kouc: {$coachName}"),
    };
};
mt_srand($seed);

$races = array_keys(RACE_ROSTERS);

$stat = [
    'rozhodnuti'       => 0,   // všechna volání decideAction (jmenovatel)
    'akce'             => 0,   // vrátil normální akci
    'endturn_vada'     => 0,   // vrátil END_TURN, PŘESTOŽE bylo co hrát
    'endturn_pravem'   => 0,   // vrátil END_TURN a nabídka opravdu nic neměla
    'ztracene_aktivace'=> 0,   // součet hráčů, kteří ještě mohli jednat
    'kola_celkem'      => 0,
    'kola_s_vadou'     => 0,
    // ⛔ JAK KOLA VŮBEC KONČÍ. Bez tohohle rozpadu se nepozná "vada nenastává"
    //    od "do té větve se vůbec nechodí" -- a nula v koši "END_TURN právem"
    //    je sama o sobě podezřelá: kolo, kde všech 11 hráčů jednalo, JINAK
    //    skončit neumí.
    'konec_turnover'   => 0,
    'konec_endturn'    => 0,
    'konec_pojistka'   => 0,   // safety valve > 50 akcí
    'konec_hra'        => 0,   // došly akce / konec zápasu, kolo nedoběhlo
    'vyjimky'          => 0,   // ⛔ kouč vyrobil akci, kterou resolver odmítl
];
$vyjimkyPodle = [];   // a JAKOU -- v živé aplikaci to NIKDO nechytá
$hist = [];   // kolik hráčů propadlo, histogram
$typy = [];   // ⭐ PŘÍČINA: co bylo v nabídce, když vada sepnula
$typyMax = [];// a kolik nejvíc

for ($g = 0; $g < $games; $g++) {
    $homeRace = $races[mt_rand(0, count($races) - 1)];
    $awayRace = $races[mt_rand(0, count($races) - 1)];

    $dice  = new RandomDiceRoller();
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

    $total = 0; $turnActions = 0;
    $turnHadDefect = false;
    // ⛔⛔ OPRAVA JMENOVATELE 11.09.: puvodne se kolo pocitalo jen v vetvi
    //   "turnover NEBO END_TURN" -- kola ukoncena TOUCHDOWNEM a POLOCASEM
    //   tise propadala (14,7 kola na zapas misto 32). Podil se tim pocital
    //   na vybrane podmnozine. Ted se kolo pozna podle ZMENY trojice
    //   (pulka, aktivni tym, cislo kola), coz je stav hry, ne muj odhad.
    $klic = null;
    // ⛔ ZAPORNY ZBYTEK (-4) prozradil, ze se konce pocitaly JINDY nez kola:
    //   jeden prubeh smyckou mohl zapsat konec, aniz se kolo zmenilo (a naopak).
    //   Ted se konec jen ZAPAMATUJE a zauctuje se az ve chvili, kdy se kolo
    //   skutecne prepne => kose scitaji na `kola_celkem` KONSTRUKCI, ne nahodou.
    $cekaKonec = null;
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
        if ($klic !== null && $novyKlic !== $klic) {
            $stat['kola_celkem']++;
            $stat[$cekaKonec ?? 'konec_hra']++;
            $cekaKonec = null;
            if ($turnHadDefect) $stat['kola_s_vadou']++;
            $turnHadDefect = false;
        }
        $klic = $novyKlic;

        $offered  = $rules->getAvailableActions($state);
        $playable = playableOffered($offered);
        $idle     = idleActors($state, $side);

        $decision = $ai->decideAction($state, $rules);
        $total++; $turnActions++;
        $stat['rozhodnuti']++;

        if ($decision['action'] === ActionType::END_TURN) {
            if ($playable > 0) {
                $stat['endturn_vada']++;
                $stat['ztracene_aktivace'] += $idle;
                $hist[$idle] = ($hist[$idle] ?? 0) + 1;
                $turnHadDefect = true;
                // ⭐ MĚŘIT PŘÍČINU, NE NÁSLEDEK: skórer tyhle akce VIDĚL
                //    a všechny je zahodil. Který typ to tedy je?
                $vid = [];
                foreach ($offered as $a) {
                    $t = ActionType::from($a['type']);
                    if ($t === ActionType::END_TURN) continue;
                    if ($t === ActionType::SETUP_PLAYER || $t === ActionType::END_SETUP) continue;
                    if (($a['playerId'] ?? null) === null) continue;
                    $vid[$t->value] = ($vid[$t->value] ?? 0) + 1;
                }
                foreach ($vid as $k => $v) {
                    $typy[$k] = ($typy[$k] ?? 0) + 1;          // v kolika vadach se typ vyskytl
                    $typyMax[$k] = max($typyMax[$k] ?? 0, $v);  // a nejvic kusu naraz
                }
            } else {
                $stat['endturn_pravem']++;
            }
        } else {
            $stat['akce']++;
        }

        $pojistka = false;
        if ($turnActions > 50) {
            $decision = ['action' => ActionType::END_TURN, 'params' => []];
            $turnActions = 0;
            $pojistka = true;
        }

        // ⛔⛔ POZOR: tenhle catch je TÁŽ ZÁPLATA jako `cli/simulate.php:157`
        //    -- a je to přesně to, co vadu maskuje. `AITurnService::playTurn`
        //    (živá hra) try/catch NEMÁ, takže tam táž výjimka spadne ven.
        //    Proto se tu nepolyká, ale POČÍTÁ.
        try {
            $result = $resolver->resolve($state, $decision['action'], $decision['params']);
        } catch (\Exception $e) {
            $stat['vyjimky']++;
            $kl = $decision['action']->value . ': ' . $e->getMessage();
            $vyjimkyPodle[$kl] = ($vyjimkyPodle[$kl] ?? 0) + 1;
            $result = $resolver->resolve($state, ActionType::END_TURN, []);
        }
        $state = $result->getNewState();

        if ($result->isTurnover()) {
            $ev = $result->getEvents();
            $posl = '(zadna udalost)';
            for ($j = count($ev) - 1; $j >= 0; $j--) {
                $t = $ev[$j]->getType();
                $t = is_string($t) ? $t : ($t->value ?? '?');
                // `end_turn` je dusledek, ne pricina -- preskocit
                if ($t !== 'end_turn' && $t !== 'turnover') { $posl = $t; break; }
            }
            $kl = $decision['action']->value . ' -> ' . $posl;
            $turnoverPricina[$kl] = ($turnoverPricina[$kl] ?? 0) + 1;
        }

        if ($result->isTurnover() || $decision['action'] === ActionType::END_TURN) {
            if ($decision['action'] !== ActionType::END_TURN) {
                $state = $resolver->resolve($state, ActionType::END_TURN, [])->getNewState();
            }
            $turnActions = 0;
            if ($pojistka)                 $cekaKonec = 'konec_pojistka';
            elseif ($result->isTurnover()) $cekaKonec = 'konec_turnover';
            else                           $cekaKonec = 'konec_endturn';
        }

        $scoring = $gameFlow->checkTouchdown($state);
        if ($scoring !== null) {
            $state = $gameFlow->resolveTouchdown($state, $scoring)['state'];
            $state = $gameFlow->resolvePostTouchdown($state)['state'];
        }
    }
}

// ---------------------------------------------------------------------------
$d = $stat['rozhodnuti'];
$zbytek = $d - $stat['akce'] - $stat['endturn_vada'] - $stat['endturn_pravem'];
printf("KOUČ: %s   KORPUS: %d her, seed %d\n\n", $coachName, $games, $seed);
printf("ROZHODNUTÍ CELKEM (jmenovatel)                 %6d\n", $d);
printf("  vrátil normální akci                         %6d  %5.1f %%\n", $stat['akce'], 100*$stat['akce']/max(1,$d));
printf("  ⛔ END_TURN, PŘESTOŽE BYLO CO HRÁT           %6d  %5.1f %%   <- VADA\n", $stat['endturn_vada'], 100*$stat['endturn_vada']/max(1,$d));
printf("  END_TURN právem (nabídka prázdná)            %6d  %5.1f %%\n", $stat['endturn_pravem'], 100*$stat['endturn_pravem']/max(1,$d));
printf("  ZBYTEK (musí být 0)                          %6d\n\n", $zbytek);

$k = $stat['kola_celkem'];
printf("KOLA: %d celkem, z toho %d (%.1f %%) skončilo touhle vadou\n",
    $k, $stat['kola_s_vadou'], 100*$stat['kola_s_vadou']/max(1,$k));
printf("  JAK KOLA KONČÍ (kontrola, že se do té větve vůbec chodí):\n");
printf("    turnoverem                                 %6d  %5.1f %%\n", $stat['konec_turnover'], 100*$stat['konec_turnover']/max(1,$k));
printf("    rozhodnutím END_TURN                       %6d  %5.1f %%\n", $stat['konec_endturn'], 100*$stat['konec_endturn']/max(1,$k));
printf("    pojistkou (>50 akcí v kole)                %6d  %5.1f %%\n", $stat['konec_pojistka'], 100*$stat['konec_pojistka']/max(1,$k));
printf("    JINAK (touchdown / poločas / konec hry)    %6d  %5.1f %%   <- tenhle koš dřív CHYBĚL\n",
    $stat['konec_hra'], 100*$stat['konec_hra']/max(1,$k));
$zb2 = $k - $stat['konec_turnover'] - $stat['konec_endturn'] - $stat['konec_pojistka'] - $stat['konec_hra'];
printf("    ZBYTEK (musí být 0)                        %6d\n", $zb2);
printf("    kol na zápas: %.1f   (celý zápas má 32 -- kontrola jmenovatele)\n\n",
    $k/max(1,$games));

printf("ZTRACENÉ AKTIVACE (hráči, kteří ještě mohli jednat): celkem %d\n", $stat['ztracene_aktivace']);
if ($stat['endturn_vada'] > 0) {
    printf("  průměr na jednu vadu %.2f hráče\n", $stat['ztracene_aktivace']/$stat['endturn_vada']);
}
ksort($hist);
foreach ($hist as $k => $v) {
    printf("    %2d hráčů propadlo   %6d  %5.1f %%\n", $k, $v, 100*$v/max(1,$stat['endturn_vada']));
}

printf("\n⛔⛔ VÝJIMKY Z RESOLVERU (živá hra `AITurnService` je NECHYTÁ):  %d = %.2f %% rozhodnutí\n",
    $stat['vyjimky'], 100*$stat['vyjimky']/max(1,$d));
arsort($vyjimkyPodle);
foreach ($vyjimkyPodle as $kl => $v) {
    printf("    %-58s %6d\n", mb_substr($kl, 0, 58), $v);
}
if ($vyjimkyPodle === []) { echo "    (žádná -- kouč vyrobil jen akce, které resolver přijal)\n"; }

$tSum = array_sum($turnoverPricina);
printf("\n⭐ ČÍM JSOU TY TURNOVERY (akce -> poslední událost):  %d celkem\n", $tSum);
arsort($turnoverPricina);
$vyps = 0;
foreach ($turnoverPricina as $kl => $v) {
    if (++$vyps > 12) break;
    printf("    %-46s %6d  %5.1f %%\n", mb_substr($kl, 0, 46), $v, 100*$v/max(1,$tSum));
}
if ($vyps > 12) {
    printf("    ... a dalších %d druhů\n", count($turnoverPricina) - 12);
}

echo "\n⭐ PŘÍČINA — CO SKÓRER VIDĚL A ZAHODIL (v kolika z těch vad se typ vyskytl):\n";
arsort($typy);
foreach ($typy as $k => $v) {
    printf("    %-14s %6d  %5.1f %% vad     (nejvíc naráz %d)\n",
        $k, $v, 100*$v/max(1,$stat['endturn_vada']), $typyMax[$k] ?? 0);
}
if ($typy === []) {
    echo "    (žádná vada nenastala -- není co rozpadat)\n";
}
