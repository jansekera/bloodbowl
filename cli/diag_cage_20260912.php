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
 * Stav klece kolem nosiče -- ČTYŘI TŘÍDY, podle upřesnění uživatele 12.09.:
 *   `souper`   ⛔⛔⛔ roh obsadil soupeř -- UŽ STOJÍ VEDLE MÍČE.
 *              Uživatel 12.09.: "soupeř stojící vedle míče je nejhorší --
 *              block vs blitz." ⇒ Odtud může nosiče BLOKOVAT, a blok je
 *              neomezený. Na prázdný roh se teprve musí dostat, a na to
 *              potřebuje BLITZ, který má jen JEDEN za kolo.
 *   `prazdny`  ⛔⛔ na rohu nikdo nestojí -- díra, ale stojí je blitz
 *   `spinavy`  ⚠️ náš hráč tam stojí, ALE má vedle sebe soupeře
 *              (uživatel: "špinavý roh = soused se soupeřem")
 *   `cisty`    ✅ náš hráč a v okolí žádný stojící soupeř
 *
 * ⭐ Počítají se jen STOJÍCÍ soupeři -- ležící nemají zónu zachycení,
 *   takže roh nešpiní.
 */
function stavKlece(GameState $state, MatchPlayerDTO $carrier): array
{
    $pos = $carrier->getPosition();
    if ($pos === null) {
        return ['nase' => 0, 'souper' => 0, 'prazdne' => 0, 'na_hristi' => 0];
    }
    $side = $carrier->getTeamSide();
    $r = ['cisty' => 0, 'spinavy' => 0, 'souper' => 0, 'prazdny' => 0, 'na_hristi' => 0];
    foreach (rohy($pos) as $roh) {
        if (!$roh->isOnPitch()) {
            continue;
        }
        $r['na_hristi']++;

        $nas = null;
        foreach ($state->getPlayersOnPitch($side) as $p) {
            if ($p->getPosition()?->equals($roh)) { $nas = $p; break; }
        }
        if ($nas === null) {
            $obsazenSouperem = false;
            foreach ($state->getPlayersOnPitch($side->opponent()) as $p) {
                if ($p->getPosition()?->equals($roh)) { $obsazenSouperem = true; break; }
            }
            $r[$obsazenSouperem ? 'souper' : 'prazdny']++;
            continue;
        }

        // Nas hrac na rohu stoji -- ma vedle sebe STOJICIHO soupere?
        $spinavy = false;
        foreach ($state->getPlayersOnPitch($side->opponent()) as $p) {
            if ($p->getState() !== \App\Enum\PlayerState::STANDING) { continue; }
            $sp = $p->getPosition();
            if ($sp !== null && $sp->distanceTo($roh) === 1) { $spinavy = true; break; }
        }
        $r[$spinavy ? 'spinavy' : 'cisty']++;
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
$ok('umí přečíst ČISTOU klec (4 čisté rohy)', $s1['cisty'] === 4, json_encode($s1));

$spinava = (new \App\Tests\Engine\GameStateBuilder())
    ->addPlayer(TeamSide::HOME, 10, 7, id: 1)
    ->addPlayer(TeamSide::HOME, 9, 6, id: 2)->addPlayer(TeamSide::HOME, 11, 6, id: 3)
    ->addPlayer(TeamSide::AWAY, 9, 8, id: 4)      // roh drží SOUPEŘ
    ->addPlayer(TeamSide::AWAY, 24, 1, id: 6)
    ->withBallCarried(1)->build();
$s2 = stavKlece($spinava, $spinava->getPlayer(1));
$ok('rozliší roh vzatý soupeřem od prázdného',
    $s2['souper'] === 1 && $s2['prazdny'] === 1, json_encode($s2));

// ⭐ A TOHLE JE TA DEFINICE, NA KTEROU UŽIVATEL 12.09. TRVAL:
//   roh drží náš hráč, ale stojí vedle něj soupeř => ŠPINAVÝ.
$oblicena = (new \App\Tests\Engine\GameStateBuilder())
    ->addPlayer(TeamSide::HOME, 10, 7, id: 1)
    ->addPlayer(TeamSide::HOME, 9, 6, id: 2)->addPlayer(TeamSide::HOME, 11, 6, id: 3)
    ->addPlayer(TeamSide::HOME, 9, 8, id: 4)->addPlayer(TeamSide::HOME, 11, 8, id: 5)
    ->addPlayer(TeamSide::AWAY, 8, 5, id: 6)      // soused rohu (9,6)
    ->withBallCarried(1)->build();
$s2b = stavKlece($oblicena, $oblicena->getPlayer(1));
$ok('najde ŠPINAVÝ roh (náš hráč, ale soupeř vedle)',
    $s2b['spinavy'] === 1 && $s2b['cisty'] === 3, json_encode($s2b));

$uKraje = (new \App\Tests\Engine\GameStateBuilder())
    ->addPlayer(TeamSide::HOME, 10, 0, id: 1)
    ->addPlayer(TeamSide::HOME, 9, 1, id: 2)->addPlayer(TeamSide::HOME, 11, 1, id: 3)
    ->addPlayer(TeamSide::AWAY, 24, 5, id: 6)
    ->withBallCarried(1)->build();
$s3 = stavKlece($uKraje, $uKraje->getPlayer(1));
$ok('u lajny vidí, že rohy na hřišti jsou jen DVA', $s3['na_hristi'] === 2, json_encode($s3));
echo "  ⇒ měřidlo rozliší čistý, špinavý, soupeřův i prázdný roh a případ u lajny.\n\n";

// ─── KORPUS ──────────────────────────────────────────────────────────────────
mt_srand($seed);
$races = array_keys(RACE_ROSTERS);
$st = [
    'kola'            => 0,
    'bez_nosice'      => 0,   // rozpad nize -- viz tri kose pod tim
    'mic_u_soupere'   => 0,   // ⭐ NORMALNI STAV: mic drzi druhy tym
    'mic_volny'       => 0,   // ⛔ lezi na hristi a nikdo ho nezvedl
    'mic_mimo_hru'    => 0,   // po TD / pred vykopem
    'nosic_bez_klece' => 0,   // start: méně než 4 naše rohy
    'u_lajny'         => 0,   // start: na hřišti nejsou 4 rohy
    'klec_na_startu'  => 0,
    'klec_prezila'    => 0,   // a na konci kola zase 4 naše
    'klec_spinava'    => 0,   // na konci chybí aspoň jeden
];
$posun = [];      // o kolik se nosič posunul v kolech s klecí
$spinavost = ['prazdny' => 0, 'souper' => 0, 'spinavy' => 0];

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
        if ($konec['cisty'] === 4) {
            $st['klec_prezila']++;
        } else {
            $st['klec_spinava']++;
            $spinavost['prazdny'] += $konec['prazdny'];
            $spinavost['souper']  += $konec['souper'];
            $spinavost['spinavy'] += $konec['spinavy'];
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
                // ⭐ 12.09.: "bez nosice" byl slepy kos -- vetsina z nej je
                //   uplne normalni stav, kdy mic proste drzi DRUHY tym.
                //   Bez tohohle rozpadu vypadalo 22 z 30 kol jako problem.
                $ball = $state->getBall();
                if ($ball->isHeld()) {
                    $st['mic_u_soupere']++;
                } elseif ($ball->isOnPitch()) {
                    $st['mic_volny']++;
                } else {
                    $st['mic_mimo_hru']++;
                }
                $start = ['typ' => 'bez_nosice', 'side' => $side, 'pos' => null];
            } else {
                $k = stavKlece($state, $c);
                $typ = $k['na_hristi'] < 4
                ? 'u_lajny'
                : (($k['cisty'] + $k['spinavy']) === 4 ? 'klec' : 'nosic_bez_klece');
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
printf("     z toho míč drží SOUPEŘ              %6d   (normální stav, ne vada)\n", $st['mic_u_soupere']);
printf("     ⛔ z toho míč VOLNÝ na hřišti        %6d   (nikdo ho nezvedl)\n", $st['mic_volny']);
printf("     míč mimo hru (po TD / před výkopem) %6d\n", $st['mic_mimo_hru']);
printf("  nosič bez klece na startu              %6d\n", $st['nosic_bez_klece']);
printf("  ⚠️ nosič u lajny (rohy nejsou 4)        %6d   <- NEPOČÍTÁ SE ZA ÚSPĚCH\n", $st['u_lajny']);
printf("  ⭐ klec na startu (4 naše rohy)         %6d\n", $st['klec_na_startu']);
$zbytek = $st['kola'] - $st['bez_nosice'] - $st['nosic_bez_klece'] - $st['u_lajny'] - $st['klec_na_startu'];
printf("  ZBYTEK (musí být 0)                    %6d\n\n", $zbytek);

if ($st['klec_na_startu'] > 0) {
    printf("Z KOL, KTERÁ ZAČALA S KLECÍ (%d):\n", $st['klec_na_startu']);
    printf("  ✅ všechny čtyři rohy ČISTÉ            %6d   %5.1f %%\n",
        $st['klec_prezila'], 100 * $st['klec_prezila'] / $st['klec_na_startu']);
    printf("  ⛔ aspoň jeden roh není čistý          %6d   %5.1f %%\n",
        $st['klec_spinava'], 100 * $st['klec_spinava'] / $st['klec_na_startu']);
        if ($posun !== []) {
        printf("  posun nosiče: průměr %.2f pole, maximum %d\n",
            array_sum($posun) / count($posun), max($posun));
    }
} else {
    echo "⛔ ANI JEDNO KOLO NEZAČALO S KLECÍ -- nulu nelze číst jako nález o posunu,\n";
    echo "   měřilo by se něco, co vůbec nenastalo.\n";
}
