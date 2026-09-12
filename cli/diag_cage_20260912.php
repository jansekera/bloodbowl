<?php
declare(strict_types=1);

/**
 * KLEC: DRŽÍ TVAR PŘI POSUNU? (12.09.2026)
 *
 * Uživatel: *„pohyb klece — musí se naměřit tak, ať po pohybu nejsou rohy
 * špinavé."* ⇒ Neměří se, jestli AI vyhrává, ale jestli po posunu **zůstanou
 * čtyři rohy obsazené vlastními hráči**.
 *
 * ⛔ ŠPINAVÝ ROH = prázdné pole NEBO pole obsazené soupeřem.
 * ⛔ DVOUROHÁ KLEC U LAJNY SE NEPOČÍTÁ JAKO ÚSPĚCH — uživatel 12.09.:
 *   *„je varianta, kdy je nosič u kraje a jsou jen dva rohy, ale to je
 *   nebezpečné."* Má vlastní koš.
 *
 * Použití: php cli/diag_cage_20260912.php [kouc] [her] [seed] [base|dev]
 */

require_once __DIR__ . '/../vendor/autoload.php';

use App\AI\{AICoachInterface, GreedyAICoach, LearningAICoach, RandomAICoach};
use App\DTO\{GameState, MatchPlayerDTO};
use App\Engine\{ActionResolver, RandomDiceRoller, RulesEngine};
use App\Enum\{ActionType, GamePhase, TeamSide};
use App\ValueObject\Position;

require_once __DIR__ . '/race_rosters.php';
require_once __DIR__ . '/developed_rosters.php';

$coachName  = (string) ($argv[1] ?? 'learning');
$games      = (int) ($argv[2] ?? 20);
$seed       = (int) ($argv[3] ?? 20260912);
$rosterMode = (string) ($argv[4] ?? 'dev');

/** Rohy klece kolem pole -- čtyři diagonály. */
function rohy(Position $p): array
{
    return [
        new Position($p->getX() - 1, $p->getY() - 1),
        new Position($p->getX() + 1, $p->getY() - 1),
        new Position($p->getX() - 1, $p->getY() + 1),
        new Position($p->getX() + 1, $p->getY() + 1),
    ];
}

/**
 * Stav klece kolem nosiče: kolik rohů je NAŠICH, kolik soupeřových,
 * kolik prázdných a kolik jich vůbec leží na hřišti.
 */
function stavKlece(GameState $state, MatchPlayerDTO $carrier): array
{
    $pos = $carrier->getPosition();
    if ($pos === null) {
        return ['nase' => 0, 'souper' => 0, 'prazdne' => 0, 'na_hristi' => 0];
    }
    $side = $carrier->getTeamSide();
    $r = ['nase' => 0, 'souper' => 0, 'prazdne' => 0, 'na_hristi' => 0];
    foreach (rohy($pos) as $roh) {
        if (!$roh->isOnPitch()) {
            continue;
        }
        $r['na_hristi']++;
        $obsazeno = null;
        foreach ($state->getPlayersOnPitch($side) as $p) {
            if ($p->getPosition()?->equals($roh)) { $obsazeno = 'nase'; break; }
        }
        if ($obsazeno === null) {
            foreach ($state->getPlayersOnPitch($side->opponent()) as $p) {
                if ($p->getPosition()?->equals($roh)) { $obsazeno = 'souper'; break; }
            }
        }
        $r[$obsazeno ?? 'prazdne']++;
    }

    return $r;
}

function nosic(GameState $state, TeamSide $side): ?MatchPlayerDTO
{
    $ball = $state->getBall();
    if (!$ball->isHeld() || $ball->getCarrierId() === null) {
        return null;
    }
    $c = $state->getPlayer($ball->getCarrierId());

    return ($c !== null && $c->getTeamSide() === $side) ? $c : null;
}

// ─── POZITIVNÍ KONTROLA MĚŘIDLA — PŘED KORPUSEM ──────────────────────────────
echo "POZITIVNÍ KONTROLA MĚŘIDLA (běží PŘED korpusem):\n";
$ok = static function (string $co, bool $ok, string $detail): void {
    printf("    [%s] %s: %s\n", $ok ? 'OK ' : 'CHYBA', $co, $detail);
    if (!$ok) { fwrite(STDERR, "MERIDLO JE VADNE\n"); exit(9); }
};

$cista = (new \App\Tests\Engine\GameStateBuilder())
    ->addPlayer(TeamSide::HOME, 10, 7, id: 1)
    ->addPlayer(TeamSide::HOME, 9, 6, id: 2)->addPlayer(TeamSide::HOME, 11, 6, id: 3)
    ->addPlayer(TeamSide::HOME, 9, 8, id: 4)->addPlayer(TeamSide::HOME, 11, 8, id: 5)
    ->addPlayer(TeamSide::AWAY, 24, 1, id: 6)
    ->withBallCarried(1)->build();
$s1 = stavKlece($cista, $cista->getPlayer(1));
$ok('umí přečíst ČISTOU klec (4 naše rohy)', $s1['nase'] === 4, json_encode($s1));

$spinava = (new \App\Tests\Engine\GameStateBuilder())
    ->addPlayer(TeamSide::HOME, 10, 7, id: 1)
    ->addPlayer(TeamSide::HOME, 9, 6, id: 2)->addPlayer(TeamSide::HOME, 11, 6, id: 3)
    ->addPlayer(TeamSide::AWAY, 9, 8, id: 4)      // roh drží SOUPEŘ
    ->addPlayer(TeamSide::AWAY, 24, 1, id: 6)
    ->withBallCarried(1)->build();
$s2 = stavKlece($spinava, $spinava->getPlayer(1));
$ok('umí najít ŠPINAVÝ roh (soupeř + prázdno)',
    $s2['nase'] === 2 && $s2['souper'] === 1 && $s2['prazdne'] === 1, json_encode($s2));

$uKraje = (new \App\Tests\Engine\GameStateBuilder())
    ->addPlayer(TeamSide::HOME, 10, 0, id: 1)
    ->addPlayer(TeamSide::HOME, 9, 1, id: 2)->addPlayer(TeamSide::HOME, 11, 1, id: 3)
    ->addPlayer(TeamSide::AWAY, 24, 5, id: 6)
    ->withBallCarried(1)->build();
$s3 = stavKlece($uKraje, $uKraje->getPlayer(1));
$ok('u lajny vidí, že rohy na hřišti jsou jen DVA', $s3['na_hristi'] === 2, json_encode($s3));
echo "  ⇒ měřidlo umí najít čistou klec, špinavý roh i případ u lajny.\n\n";

// ─── KORPUS ──────────────────────────────────────────────────────────────────
mt_srand($seed);
$races = array_keys(RACE_ROSTERS);
$st = [
    'kola'            => 0,
    'bez_nosice'      => 0,
    'nosic_bez_klece' => 0,   // start: méně než 4 naše rohy
    'u_lajny'         => 0,   // start: na hřišti nejsou 4 rohy
    'klec_na_startu'  => 0,
    'klec_prezila'    => 0,   // a na konci kola zase 4 naše
    'klec_spinava'    => 0,   // na konci chybí aspoň jeden
];
$posun = [];      // o kolik se nosič posunul v kolech s klecí
$spinavost = ['prazdne' => 0, 'souper' => 0];

for ($g = 0; $g < $games; $g++) {
    $homeRace = $races[mt_rand(0, count($races) - 1)];
    $awayRace = $races[mt_rand(0, count($races) - 1)];
    $dice = new RandomDiceRoller(); $rules = new RulesEngine();
    $mk = static fn(): AICoachInterface => match ($coachName) {
        'greedy' => new GreedyAICoach(),
        'learning' => new LearningAICoach(__DIR__ . '/../weights.json', 0.0),
        'random' => new RandomAICoach(),
        default => throw new \InvalidArgumentException("neznamy kouc: {$coachName}"),
    };
    $homeAi = $mk(); $awayAi = $mk();
    $players = $rosterMode === 'base'
        ? getRaceRoster(TeamSide::HOME, $homeRace) + getRaceRoster(TeamSide::AWAY, $awayRace)
        : getDevelopedRaceRoster(TeamSide::HOME, $homeRace) + getDevelopedRaceRoster(TeamSide::AWAY, $awayRace);
    $state = GameState::create(1,
        \App\DTO\TeamStateDTO::create(1, 'H', $homeRace, TeamSide::HOME, 3),
        \App\DTO\TeamStateDTO::create(2, 'A', $awayRace, TeamSide::AWAY, 3),
        $players, TeamSide::HOME);
    $state = $homeAi->setupFormation($state, TeamSide::HOME);
    $state = $awayAi->setupFormation($state, TeamSide::AWAY);
    $resolver = new ActionResolver($dice);
    $state = $resolver->resolve($state, ActionType::END_SETUP, [])->getNewState();
    $gameFlow = $resolver->getGameFlowResolver();

    $total = 0; $turnActions = 0; $klic = null; $start = null;

    $uzavri = static function (?array $start, GameState $state) use (&$st, &$posun, &$spinavost): void {
        if ($start === null) { return; }
        $st['kola']++;
        if ($start['typ'] !== 'klec') { $st[$start['typ']]++; return; }
        $st['klec_na_startu']++;
        $c = nosic($state, $start['side']);
        if ($c === null) { $st['klec_spinava']++; return; }
        $konec = stavKlece($state, $c);
        if ($konec['nase'] === 4) {
            $st['klec_prezila']++;
        } else {
            $st['klec_spinava']++;
            $spinavost['prazdne'] += $konec['prazdne'];
            $spinavost['souper']  += $konec['souper'];
        }
        $posun[] = $c->getPosition() !== null && $start['pos'] !== null
            ? max(abs($c->getPosition()->getX() - $start['pos']->getX()),
                  abs($c->getPosition()->getY() - $start['pos']->getY()))
            : 0;
    };

    while ($state->getPhase() !== GamePhase::GAME_OVER && $total < 2000) {
        if (!$state->getPhase()->isPlayable()) {
            if ($state->getPhase()->isSetup()) {
                $side = $state->getActiveTeam();
                $ai = $side === TeamSide::HOME ? $homeAi : $awayAi;
                if (count($state->getPlayersOnPitch($side)) < 11) { $state = $ai->setupFormation($state, $side); }
                $state = $resolver->resolve($state, ActionType::END_SETUP, [])->getNewState();
                $total++;
            } elseif ($state->getPhase() === GamePhase::HALF_TIME) {
                $state = $gameFlow->resolveHalfTime($state)['state']; $total++;
            } else { break; }
            continue;
        }
        $side = $state->getActiveTeam();
        $ai = $side === TeamSide::HOME ? $homeAi : $awayAi;
        $novyKlic = $state->getHalf() . '/' . $side->value . '/' . $state->getTeamState($side)->getTurnNumber();
        if ($klic !== $novyKlic) {
            $uzavri($start, $state);
            $c = nosic($state, $side);
            if ($c === null) {
                $start = ['typ' => 'bez_nosice', 'side' => $side, 'pos' => null];
            } else {
                $k = stavKlece($state, $c);
                $typ = $k['na_hristi'] < 4 ? 'u_lajny' : ($k['nase'] === 4 ? 'klec' : 'nosic_bez_klece');
                $start = ['typ' => $typ, 'side' => $side, 'pos' => $c->getPosition()];
            }
            $klic = $novyKlic; $turnActions = 0;
        }

        $decision = $ai->decideAction($state, $rules);
        $total++; $turnActions++;
        if ($turnActions > 50) { $decision = ['action' => ActionType::END_TURN, 'params' => []]; }
        try { $result = $resolver->resolve($state, $decision['action'], $decision['params']); }
        catch (\Exception $e) { $result = $resolver->resolve($state, ActionType::END_TURN, []); }
        $state = $result->getNewState();
        if ($result->isTurnover()) { $state = $resolver->resolve($state, ActionType::END_TURN, [])->getNewState(); }
        $sc = $gameFlow->checkTouchdown($state);
        if ($sc !== null) {
            $state = $gameFlow->resolveTouchdown($state, $sc)['state'];
            $state = $gameFlow->resolvePostTouchdown($state)['state'];
        }
    }
    $uzavri($start, $state); $start = null; $klic = null;
}

printf("KOUČ: %s   KORPUS: %d her, seed %d, rostery: %s\n\n", $coachName, $games, $seed, $rosterMode);
printf("KOLA CELKEM (jmenovatel)                 %6d\n", $st['kola']);
printf("  bez nosiče                             %6d\n", $st['bez_nosice']);
printf("  nosič bez klece na startu              %6d\n", $st['nosic_bez_klece']);
printf("  ⚠️ nosič u lajny (rohy nejsou 4)        %6d   <- NEPOČÍTÁ SE ZA ÚSPĚCH\n", $st['u_lajny']);
printf("  ⭐ klec na startu (4 naše rohy)         %6d\n", $st['klec_na_startu']);
$zbytek = $st['kola'] - $st['bez_nosice'] - $st['nosic_bez_klece'] - $st['u_lajny'] - $st['klec_na_startu'];
printf("  ZBYTEK (musí být 0)                    %6d\n\n", $zbytek);

if ($st['klec_na_startu'] > 0) {
    printf("Z KOL, KTERÁ ZAČALA S KLECÍ (%d):\n", $st['klec_na_startu']);
    printf("  ✅ klec přežila kolo                   %6d   %5.1f %%\n",
        $st['klec_prezila'], 100 * $st['klec_prezila'] / $st['klec_na_startu']);
    printf("  ⛔ rohy po kole ŠPINAVÉ                %6d   %5.1f %%\n",
        $st['klec_spinava'], 100 * $st['klec_spinava'] / $st['klec_na_startu']);
    printf("     z toho prázdných rohů %d, obsazených soupeřem %d\n",
        $spinavost['prazdne'], $spinavost['souper']);
    if ($posun !== []) {
        printf("  posun nosiče: průměr %.2f pole, maximum %d\n",
            array_sum($posun) / count($posun), max($posun));
    }
} else {
    echo "⛔ ANI JEDNO KOLO NEZAČALO S KLECÍ -- nulu nelze číst jako nález o posunu,\n";
    echo "   měřilo by se něco, co vůbec nenastalo.\n";
}
