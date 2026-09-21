<?php

declare(strict_types=1);

/**
 * MĚŘIDLO K OPRAVĚ „Dodge 1× za kolo" (21.09.2026).
 *
 * `rules_bb2016.txt` r. 960-962 a 8089-8090: "the player may only re-roll one
 * failed Dodge roll per turn".
 *
 * ⭐ Histogram událostí (`diag_event_histogram_20260911.php`) tuhle změnu NEMĚŘÍ --
 *   sčítá `reroll` dohromady bez zdroje, takže by ukázal totéž před opravou i po ní.
 *   Tohle měřidlo čte přímo to, co se změnilo:
 *     - `hodu_na_uhyb` / `uhybu`    = hody na úhyb celkem / z toho nové pokusy (bez přehozených hodů)
 *     - `dodge_prehozu_celkem`      = kolikrát vůbec Dodge přehodil úhyb (pozitivní kontrola mechaniky)
 *     - `hrac_kolo_s_2plus`         = kolikrát týž hráč přehodil Dodgem dvakrát v jednom kole (MUSÍ být 0)
 *     - `limit_sepnul`              = kolikrát hráč s Dodge NOVĚ (tj. ne přehozeným hodem) neúspěšně
 *                                     uhýbal, ačkoli už Dodge v tom kole použil -- přesně ty hody,
 *                                     které starý kód ještě jednou přehazoval
 *
 * ⛔ 21.09. (1): první verze měřidla počítala jako „nový úhyb" i VÝSLEDEK PŘEHOZU (přehoz vydá další
 *   událost `dodge`), takže `limit_sepnul` hlásilo 22 místo skutečného počtu. Každá událost `reroll`
 *   proto označí následující hod téhož hráče jako přehozený a ten se do pokusů nepočítá.
 * ⛔ 21.09. (2): a ta oprava měla vlastní vadu -- `reroll` vydávají i pickup (Sure Hands), chytání,
 *   přihrávka, GFI (Sure Feet), blok a TTM. Příznak „další hod je přehoz" tedy visel na hráči
 *   klidně přes několik akcí a zhasl až u příštího úhybu, který tím vypadl z počtu pokusů.
 *   ⇒ příznak se spotřebuje u NEJBLIŽŠÍ další události téhož hráče, ať je jakákoli.
 *
 * Použití: php cli/diag_dodge_once_per_turn_20260921.php [kouc] [her] [seed] [base|dev] [rasaH] [rasaA]
 */

require_once __DIR__ . '/../vendor/autoload.php';

use App\AI\{AICoachInterface, GreedyAICoach, LearningAICoach, RandomAICoach};
use App\DTO\GameState;
use App\Engine\{ActionResolver, FixedDiceRoller, RandomDiceRoller, RulesEngine};
use App\Enum\{ActionType, GamePhase, SkillName, TeamSide};

require_once __DIR__ . '/race_rosters.php';
require_once __DIR__ . '/developed_rosters.php';

$coachName  = (string) ($argv[1] ?? 'greedy');
$games      = (int) ($argv[2] ?? 12);
$seed       = (int) ($argv[3] ?? 20260921);
$rosterMode = (string) ($argv[4] ?? 'dev');
$fixHome    = $argv[5] ?? null;
$fixAway    = $argv[6] ?? $fixHome;

$makeCoach = static function () use ($coachName): AICoachInterface {
    return match ($coachName) {
        'greedy'   => new GreedyAICoach(),
        'learning' => new LearningAICoach(__DIR__ . '/../weights.json', 0.0),
        'random'   => new RandomAICoach(),
        default    => throw new \InvalidArgumentException("neznamy kouc: {$coachName}"),
    };
};

// ================= POZITIVNÍ KONTROLA MĚŘIDLA =================
// Bez ní je každá nula níž bezcenná: měřidlo musí na známé fixtuře najít
// jedničku (Dodge přehodil) i nulu (podruhé už ne).
$kontrola = static function (string $co, bool $ok, string $detail): void {
    echo ($ok ? '  ✅ ' : '  ⛔ ') . $co . ' -- ' . $detail . "\n";
    if (!$ok) { fwrite(STDERR, "MERIDLO JE VADNE, koncim\n"); exit(9); }
};

echo "KONTROLA MĚŘIDLA (fixtura ze stejné geometrie jako test):\n";
$s = (new \App\Tests\Engine\GameStateBuilder())
    ->addPlayer(TeamSide::HOME, 5, 5, movement: 6, skills: [SkillName::Dodge], id: 1)
    ->addPlayer(TeamSide::AWAY, 5, 4, id: 2)
    ->addPlayer(TeamSide::AWAY, 5, 6, id: 3)
    ->build();
$s = $s->withTeamState(TeamSide::HOME, $s->getTeamState(TeamSide::HOME)->withRerolls(0));
$r = (new ActionResolver(new FixedDiceRoller([3, 5, 2, 6, 1, 1, 1, 1])))
        ->resolve($s, ActionType::MOVE, ['playerId' => 1, 'x' => 7, 'y' => 5]);

$dodgeRerolly = 0; $neuspesneUhyby = 0;
foreach ($r->getEvents() as $ev) {
    if ($ev->getType() === 'reroll' && ($ev->getData()['source'] ?? '') === 'Dodge') { $dodgeRerolly++; }
    if ($ev->getType() === 'dodge' && ($ev->getData()['success'] ?? true) === false) { $neuspesneUhyby++; }
}
$kontrola('umí najít použitý Dodge přehoz', $dodgeRerolly === 1, "dodge přehozů = {$dodgeRerolly}");
$kontrola('umí najít neúspěšný úhyb', $neuspesneUhyby === 2, "neúspěšných úhybů = {$neuspesneUhyby}");
$kontrola('umí říct „podruhé už ne"', $r->isTurnover(), 'druhý úhyb skončil turnoverem');
echo "\n";

mt_srand($seed);
$races = array_keys(RACE_ROSTERS);

$stat = [
    'hry' => 0, 'kola' => 0, 'hodu_na_uhyb' => 0, 'uhybu' => 0, 'neuspesnych_uhybu' => 0,
    'dodge_prehozu_celkem' => 0, 'hrac_kolo_s_2plus' => 0, 'limit_sepnul' => 0,
];
$vyjimky = 0;

for ($g = 0; $g < $games; $g++) {
    $homeRace = $fixHome ?? $races[mt_rand(0, count($races) - 1)];
    $awayRace = $fixAway ?? $races[mt_rand(0, count($races) - 1)];

    $dice   = new RandomDiceRoller();
    $rules  = new RulesEngine();
    $homeAi = $makeCoach();
    $awayAi = $makeCoach();

    $players = $rosterMode === 'base'
        ? getRaceRoster(TeamSide::HOME, $homeRace) + getRaceRoster(TeamSide::AWAY, $awayRace)
        : getDevelopedRaceRoster(TeamSide::HOME, $homeRace) + getDevelopedRaceRoster(TeamSide::AWAY, $awayRace);

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
    $stat['hry']++;

    /** @var array<string, int> $prehozyVKole  klíč "půle/strana/kolo/hráč" -> počet Dodge přehozů */
    $prehozyVKole = [];
    /** @var array<int, bool> $dalsiUhybJePrehoz  hráč -> další událost `dodge` je výsledek přehozu */
    $dalsiUhybJePrehoz = [];

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

        $novyKlic = $state->getHalf() . '/' . $side->value . '/' . $state->getTeamState($side)->getTurnNumber();
        if ($klic !== null && $novyKlic !== $klic) { $stat['kola']++; }
        $klic = $novyKlic;

        $decision = $ai->decideAction($state, $rules);
        $total++; $turnActions++;
        if ($turnActions > 50) { $decision = ['action' => ActionType::END_TURN, 'params' => []]; $turnActions = 0; }

        $predStavem = $state;
        try {
            $result = $resolver->resolve($state, $decision['action'], $decision['params']);
        } catch (\Exception $e) {
            $vyjimky++;
            $result = $resolver->resolve($state, ActionType::END_TURN, []);
        }

        foreach ($result->getEvents() as $ev) {
            $typ  = $ev->getType();
            $data = $ev->getData();
            $pid  = (int) ($data['playerId'] ?? 0);
            $k    = $klic . '/' . $pid;

            // Příznak „tenhle hod je výsledek přehozu" platí jen pro NEJBLIŽŠÍ událost hráče.
            $jePrehoz = $dalsiUhybJePrehoz[$pid] ?? false;
            if ($typ !== 'reroll' && $pid !== 0) { $dalsiUhybJePrehoz[$pid] = false; }

            if ($typ === 'dodge') {
                $stat['hodu_na_uhyb']++;
                if (!$jePrehoz) { $stat['uhybu']++; }
                if (($data['success'] ?? true) === false) {
                    $stat['neuspesnych_uhybu']++;
                    // Sepnul limit? NOVÝ úhyb hráče, který má Dodge, ale už ho v tom kole použil.
                    $hrac = $predStavem->getPlayer($pid);
                    if (!$jePrehoz && $hrac !== null && $hrac->hasSkill(SkillName::Dodge)
                        && ($prehozyVKole[$k] ?? 0) >= 1) {
                        $stat['limit_sepnul']++;
                    }
                }
            }

            if ($typ === 'reroll') {
                $dalsiUhybJePrehoz[$pid] = true;
                if (($data['source'] ?? '') === 'Dodge') {
                    $stat['dodge_prehozu_celkem']++;
                    $prehozyVKole[$k] = ($prehozyVKole[$k] ?? 0) + 1;
                    if ($prehozyVKole[$k] === 2) { $stat['hrac_kolo_s_2plus']++; }
                }
            }
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

echo "BĚH: {$stat['hry']} her, {$stat['kola']} kol, výjimek: {$vyjimky}\n";
echo "  hodu_na_uhyb              {$stat['hodu_na_uhyb']}   (vcetne prehozenych)\n";
echo "  uhybu                     {$stat['uhybu']}   (pokusu, bez prehozu)\n";
echo "  neuspesnych_uhybu         {$stat['neuspesnych_uhybu']}\n";
echo "  dodge_prehozu_celkem      {$stat['dodge_prehozu_celkem']}\n";
echo "  limit_sepnul              {$stat['limit_sepnul']}\n";
echo "  hrac_kolo_s_2plus         {$stat['hrac_kolo_s_2plus']}   <= podle r. 960-962 musí být 0\n";
