# A3-1: Liga starších snapshotů místo random opponent mixu — design (2026-07-31)

**Stav: HOTOVO (design + implementace ve worktree, NEnasazeno, NEpushnuto).**
Autor: Fable 5 (engineer), izolovaný worktree `.claude/worktrees/agent-ae897a8fe179d2dc3`, branch `worktree-agent-ae897a8fe179d2dc3`.
NENASAZOVAT — příprava. Default chování beze změny, vše za env flagem `BB_OPPONENT_LEAGUE`.

POZN. session-continuity: worktree-izolace agenta blokuje zápis do main repa
(i povolená výjimka evidence/ souboru byla harnessem odmítnuta). Soubor proto
vzniká VE WORKTREE pod `evidence/` a je commitován po logických celcích —
při pádu session je vše v git historii worktree branche. Po dokončení zkopírovat
do main `evidence/`.

## 1. Problém (rekapitulace)

- `run_iteration.py:127` `OPPONENT_MIX_RATIO = 0.5` → `--opponent-mix-ratio=0.5`
  do `python/blood_bowl/training_loop.py`.
- `training_loop.py:347-367`: při self-play + mix se každá epocha rozdělí na
  `n_self = games - n_mix` her proti frozen šampionovi (weights_frozen.json)
  a `n_mix = max(1, round(games*0.5))` her proti čistě RANDOM soupeři
  (away_ai='random', bez vah, bez MCTS).
- Polovina tréninkových dat = válcování randomu → nízkoinformativní gradienty;
  podezření, že přispívá k laterálnímu driftu (váhy se hýbou konzistentním
  směrem na "současných datech", ale z poloviny anti-random — viz
  evidence/fable_crossera_20260730.md §A.3). AlphaZero používal ligu starších
  verzí, ne random.

## 2. Mechanismus soupeřových vah (zjištění, do hloubky)

Řetěz: `run_iteration.py` (Step 2) → subprocess `python -m blood_bowl.train_cli`
(`--opponent-mix-ratio=0.5`, `--self-play`, `--use-cpp`) → `training_loop.run_training()`
→ `CPPRunner.simulate()` → per-game child proces (`multiprocessing.Pool`).

Klíčové body:

1. **Frozen mechanismus** (`training_loop.py:168-177`): při self-play bez
   `--opponent-weights` se na ZAČÁTKU tréninku zkopíruje `weights_best.json` →
   `weights_frozen.json` a `frozen_weights_path` se předává každé self hře jako
   `away_weights`. (Pozn.: run_iteration.py Step 1 dělá tutéž kopii ještě před
   spuštěním; training_loop ji jen přepíše identickým obsahem.)
2. **Mix větev** (`training_loop.py:347-367`): dvě sekvenční dávky za epochu —
   `self_batch` (away=macro_mcts + `away_weights=frozen`) a `rand_batch`
   (away='random', `away_weights=None`). Výsledky se slijí do jednoho
   `TournamentResult` s labelem `mixed(self=50%,random=50%)` (ten je vidět v logu).
3. **Per-game dispatch** (`cpp_runner.py:_simulate_parallel`, ř. 240-276): pro
   každou hru se sestaví task-tuple `(seed, …, away_weights, log_dir, game_num, tv)`
   a `Pool.map(_simulate_game_worker, tasks)`. **Každá hra je samostatný child
   proces, který si sám zavolá `bb_engine.simulate_game_logged(...,
   away_weights_path=…)` — C++ engine čte soubor s vahami znovu pro každou hru.**
   Tj. už dnes se `weights_frozen.json` (233 KB) čte ~320× za iteraci; IO náklad
   ligy je identický (jen jiná cesta v tuple), žádné nové kopírování per game.
   "Načíst kotvu jednou" tedy v této architektuře znamená: NEkopírovat soubory,
   jen předat existující cestu; OS page-cache drží soubory horké.
4. **Away strana používá jen VALUE váhy.** Binding nemá `away_policy_path`;
   `policy_weights_path` (sdílená weights_policy.json) ovlivňuje jen priory a
   při policy_blend=0 se ve hře stejně nepoužívá. Kotvy tedy potřebují jen
   kompatibilní value síť — přesně to ověřil cross-era census (§A.1): staré
   70-featurové sítě hrají díky `min(numFeatures, inputSize_)` ve
   value_function.cpp.
5. **away_epsilon** se v CPP cestě fakticky nepoužívá (CPPRunner ho přijme a
   ignoruje; worker tuple ho nenese) — away macro_mcts hraje bez epsilonu.
   Pro ligu tedy není co řešit; parametr předáváme kvůli PHP kompatibilitě stejně
   jako self větev (0.0).
6. **Rychlost**: random hra ~0.20 s/MCTS-iter vs self hra ~0.65 s/MCTS-iter
   (odhad v training_loop.py:222-225). Liga → mix hry zdraží ~3,3× (viz Rizika).

## 3. Inventura kandidátních kotev

142 souborů `weights_snap_*.json` (mtime 06.05.–30.07.; 48 z nich má hromadný
mtime 06.05. 13:43 — "cizí linie" květnové éry, cos≈0 k šampionovi). Census
formátů viz fable_crossera_20260730.md §A.1. Turnajové kotvy R6 (chess-score
z 390 her): jul29 55,8 % > jun19 55,0 % > champion 54,2 % > jul21 52,7 % >
jul28 46,9 % > may06 35,4 %.

Kandidátní pool (návrh, zdůvodnění níže):

| kotva | soubor (main root) | éra | pozn. |
|---|---|---|---|
| champion | weights_frozen.json (kopie weights_best.json) | 26.–29.06. | už pokrytý self větví — v lize NENÍ |
| jun16-cert | weights_cert_842c200_20260616.json (99 KB, UNtracked!) | 16.06. | gate-promoted předchůdce; jediná skutečně "gate-ověřená" starší verze |
| jun19 | weights_snap_e8_90pct_+1.3.json (230 KB) | 19.06. | 70f+policy93×64, 55,0 % v turnaji — silná, mírně jiná linie |
| jul21 | weights_snap_e16_94pct_+2.3.json (238 KB) | 21.07. | 73f, 52,7 % — reprezentant červencového driftu |
| jul28 | weights_snap_e16_99pct_+2.5.json | 28.07. | 46,9 % (item1 REJECTED sourozenec) — druhá "driftová" kotva, volitelná |
| may06 | weights_snap_e8_83pct_+1.0.json | ≤06.05. | 35 % — VYŘADIT (viz níže) |

**may06: NEzařazovat.** Důvody: (a) 35 % chess-score = jen o málo lepší cíl než
random, přitom 3,3× dražší hra; (b) cizí linie s cos≈0 — gradienty proti ní
netrénují "porazit své minulé já", ale porazit nesouvisející slabou síť, což je
přesně ten nízkoinformativní režim, který rušíme; (c) AlphaZero liga = starší
verze TÉŽE linie. Pokud chceme "slabého sparring partnera" pro diverzitu, malé
% random je levnější a rozmanitější.

**Stabilita jmen souborů (RIZIKO):** `training_loop.py:641` auto-snapshot
zapisuje `weights_snap_e{epoch}_{wr}pct_{sd}.json` — jméno NENÍ unikátní v čase
a budoucí běh může kotvu PŘEPSAT (např. další e16_94pct_+2.3). Navíc
weights_cert_842c200 je untracked. → Implementace při první aktivaci ligy
jednorázově zkopíruje každou kotvu na stabilní jméno
`weights_league_<name>.json` (bootstrap à la weights_anchor_noreset.json
z commitu 3dcb5b2) a dál používá výhradně stabilní kopie.

## 4. Návrh designu

### 4.1 Env rozhraní (vzor BB_NO_RESET, commit 3dcb5b2 — off by default, bajtově původní chování)

- `BB_OPPONENT_LEAGUE=1` — zapne ligu. Vypnuto (default) → run_iteration
  nepřidá žádný nový CLI argument → training_loop jede identickou větví jako dnes.
- `BB_LEAGUE_ANCHORS` — volitelný override poolu: `name=path[:weight],…`
  (default = kurátorovaný seznam v run_iteration.py: jun16-cert 1.0,
  jun19 1.0, jul21 1.0; jul28 zatím ne — držet pool malý, 1 změna najednou).
- `BB_LEAGUE_RANDOM_SHARE` — podíl mix her, které ZŮSTANOU random
  (default 0.1, tj. 5 % všech her). Důvod: úplné odstranění random z tréninkových
  dat je druhá souběžná změna distribuce; malé reziduum drží kontinuitu
  a levnou diverzitu. Nastavením 0.0 lze vypnout.

### 4.2 Výběr soupeře: deterministický, recency-vážený? → UNIFORM s vahami v spec

Rozhodnutí: **uniform přes kotvy (výchozí váhy 1.0), s možností per-anchor vah
ve spec stringu**. Recency-weighting neřešit algoritmicky: (a) pool je malý a
kurátorovaný, váhy lze zapsat ručně; (b) self větev (50 % her, frozen šampion)
UŽ JE maximální recency — liga má dodávat právě to starší/diverzní; (c) méně
kódu = menší riziko. Plánovač je deterministický: seed = f(epocha) →
reprodukovatelné složení epochy, largest-remainder kvóty (každá kotva dostane
⌊podíl⌋ her, zbytek rozdělí seedovaný RNG) → žádná epocha není celá proti
jedné kotvě omylem.

### 4.3 Složení epochy (40 her, mix_ratio 0.5, random_share 0.1)

- 20 her self vs frozen šampion (beze změny),
- 2 hry vs random (reziduum),
- 18 her vs liga: ~6 na kotvu (3 kotvy, kvóty deterministicky).

Log label: `mixed(self=50%, league=45%, random=5%)`.

### 4.4 Per-game podstrčení vah (nejlevnější cesta)

`CPPRunner._simulate_parallel` i serial cesta dostanou `away_weights` nově i
jako `list[str]` délky `matches` (str = dnešní chování, beze změny). Task tuple
i-té hry nese `away_weights[i]`. Jedna dávka, jeden Pool, plná paralelizace
(žádné per-anchor mini-dávky, které by 12-worker pool vyhladověly na 6 hrách).
Engine si soubor přečte per game jako dnes frozen — nulový nový IO/memory
náklad.

## 5. Přesná místa změn

1. `run_iteration.py` (~ř. 75 za NO_RESET blok): parse `BB_OPPONENT_LEAGUE`,
   `BB_LEAGUE_ANCHORS`, `BB_LEAGUE_RANDOM_SHARE`; default kurátorovaný pool;
   bootstrap stabilních kopií `weights_league_<name>.json` (jen pokud chybí);
   při zapnuté lize append `--league-opponents=…` a `--league-random-share=…`
   do cmd (ř. ~544). Flag off → cmd bajtově identický.
2. `python/blood_bowl/train_cli.py`: `--league-opponents` (spec string),
   `--league-random-share` (float, default 0.0) → kwargs run_training.
3. `python/blood_bowl/training_loop.py`: `run_training(..., league_opponents=None,
   league_random_share=0.0)`; v mix větvi (ř. 347) při zadané lize rozdělit
   n_mix na n_rand/n_league, `LeagueOpponentPool.schedule(n_league, seed=epoch)`
   → per-game seznam cest, jedna league dávka away=macro_mcts,
   away_weights=list; úprava effective_opponent labelu a odhadu času
   (league hry počítat jako self_secs, ne rand_secs).
4. `python/blood_bowl/cpp_runner.py`: `away_weights: str | list[str]` v
   `simulate`/`_simulate_parallel` + serial smyčce.
5. NOVÝ `python/blood_bowl/league.py`: parse spec, validace existence souborů,
   `ensure_stable_copies(root)` (bootstrap), `schedule(n, seed)` —
   deterministický largest-remainder plánovač.
6. NOVÝ `python/tests/test_league_pool.py`: bez enginu — determinismus
   plánovače, kvóty, parse, bootstrap v tmpdir, default-off (run_training
   signatura default = None ⇒ stará větev).

## 6. Rizika

- **Draw-rate nahoru.** Random hry byly ~jisté decisive výhry; liga kotvy
  47–55 % chess-score → víc remíz (šumové dno draw-rate ±8–11pp známé z HtH).
  MC-shaped trénink z remíz dostává slabší terminální signál. Mitigace: self
  polovina beze změny, random reziduum 5 %, sledovat draw-rate v epoch_metrics.
- **Replay buffer kontaminace/split.** `replay_buffer.pkl` PŘEŽÍVÁ mezi
  iteracemi — po zapnutí ligy v něm zůstávají anti-random transitions ze
  starých běhů. Doporučení pro experiment: začít s čistým bufferem (zálohovat
  a smazat), jinak se distribuce mění postupně a nečitelně.
- **Pomalejší epocha.** Mix hry zdraží z ~0.20 na ~0.65 s/MCTS-iter →
  při MCTS=100 epocha 40 her ~ z ~34 min na ~52 min (worker-paralelismus
  to dělí ~12×, reálně +~50 % wall času tréninkové fáze). Validace na CPU
  ceně viz §7.
- **Win-rate metrika epochy se rozředí.** epoch win_rate (kill-condition,
  benchmark_interval logika) dnes zahrnuje snadné random výhry; liga ji sníží
  ~o 10-20 pp bez reálného zhoršení. Kill-condition je relativní (5 epoch bez
  zlepšení), takže OK, ale číst logy s tímto vědomím. Benchmark vs random
  (BM_FLOOR, new_bm škála) je ODDĚLENÁ větev (benchmark.py) — beze změny.
- **Kotva = zamrzlá minulá verze s policy sítí jiné dimenze** (jun19 93×64):
  ve hře se nepoužívá (policy_blend=0, away nemá policy path) — ověřeno
  crossera load testy; kdyby se někdy zapínal policy_blend>0, kotvy zůstanou
  value-only a je to OK (priory z sdílené weights_policy.json).
- **Šampion není ohrožen:** liga mění jen složení TRÉNINKOVÝCH dat; gate,
  selekce, weights_best tok jsou nedotčené. Flag off = bajtově identické cmd.
- **--no-push kompatibilita:** změny se netýkají push větve; bootstrap kopie
  jsou lokální soubory mimo git (untracked), nic se nepushuje.

## 7. Validační experiment (až PO no-reset sérii; NEZAPÍNAT souběžně s ní)

Jedna změna najednou: no-reset série doběhne a vyhodnotí se PRVNÍ. Pak:

- **Návrh:** zopakovat identický no-reset protokol (stejný počet iterací,
  stejná kotva gate) s `BB_NO_RESET=1 BB_OPPONENT_LEAGUE=1` a čistým replay
  bufferem. Primární metriky: (a) trajektorie HtH vs fixní kotva přes iterace
  (srovnání s no-reset baseline sérií), (b) draw-rate, (c) weight-space
  telemetrie diag_crossera (směr driftu — přestane být "anti-random
  laterální"?).
- **Cena:** stejný počet iterací jako baseline série; wall čas +~50 %
  tréninkové fáze (viz §6) → při 16 epochách×40 her ~8-9 h/iterace místo ~6 h
  (hrubý odhad, kalibrovat z reálného no-reset běhu). Gate/benchmark fáze beze
  změny.
- Rozhodnutí o nasazení do produkčního OPPONENT_MIX až podle obou sérií.

## 8. Implementace (worktree, commit)

- Worktree: `/home/jan/claude/bloodbowl/.claude/worktrees/agent-ae897a8fe179d2dc3`,
  branch `worktree-agent-ae897a8fe179d2dc3` (base = main repo HEAD 80fe7c3).
- Commity (NEpushnuto):
  - `713aa49` — docs: tento design dokument (draft).
  - `69c9ee5` — feat(pipeline): BB_OPPONENT_LEAGUE implementace + testy.
- Změněné soubory:
  - `python/blood_bowl/league.py` (NOVÝ) — `LeagueOpponentPool`: parse spec
    `name=path[:weight],…`, `ensure_stable_copies()` (idempotentní bootstrap
    `weights_league_<name>.json`, nikdy nepřepisuje existující zamrzlou kopii),
    `schedule(n, seed)` (largest-remainder kvóty + seedovaný shuffle,
    deterministické), `to_spec()`.
  - `python/blood_bowl/cpp_runner.py` — `away_weights` nově i `list[str]`
    per game (délka validována proti `matches`); serial i parallel cesta;
    str = bajtově původní chování.
  - `python/blood_bowl/cli_runner.py` — PHP cesta: list → jasný
    `NotImplementedError` (liga vyžaduje --use-cpp).
  - `python/blood_bowl/training_loop.py` — `run_training(...,
    league_opponents=None, league_random_share=0.0)`; mix větev dělí n_mix na
    n_league/n_rand, league dávka away=macro_mcts s per-game rozvrhem
    (`seed=epoch`); label `mixed(self=…, league=…, random=…)`; odhad času
    počítá league hry jako self_secs; `_run_simulation_batch` krájí per-game
    seznam přes race sub-dávky (kontiguální řezy, offsety ověřeny testem).
  - `python/blood_bowl/train_cli.py` — `--league-opponents`,
    `--league-random-share` (defaulty vypnuto).
  - `run_iteration.py` — env blok `BB_OPPONENT_LEAGUE` / `BB_LEAGUE_ANCHORS` /
    `BB_LEAGUE_RANDOM_SHARE` (vzor BB_NO_RESET 3dcb5b2), kurátorovaný default
    pool, bootstrap + append CLI argů JEN při zapnutém flagu.
  - `python/tests/test_league_pool.py` (NOVÝ) — 17 testů bez enginu: parse,
    determinismus a kvóty rozvrhu, idempotence bootstrapu (přepsaný zdrojový
    snapshot NEZMĚNÍ zamrzlou kotvu), krájení dávek přes fake runner
    (kontiguita + pokrytí + game_offsety), off-by-default kontrakt.
- Ověření: `pytest tests/test_league_pool.py` 17/17 pass; sousední
  `test_training_loop.py` + `test_cli_runner.py` 17/17 pass; py_compile všech
  dotčených souborů OK; import-smoke run_iteration s flagem i bez něj OK.
  Žádné hry enginu nespuštěny (CPU patří běžícímu tréninku).
- Default-off důkaz: flag off → run_iteration nepřidá žádný CLI arg (blok je
  celý pod `if OPPONENT_LEAGUE:`), `league_opponents=None` v training_loop
  vede na `n_rand=n_mix, n_league=0` → identické dvě dávky jako dosud;
  `away_weights` jako str prochází všude beze změny typu.

## 9. Poznámka k nasazení (pro budoucí session)

1. Merge worktree branche (nebo cherry-pick 69c9ee5) do main — až NEBĚŽÍ trénink.
2. Před prvním league během: zálohovat a vyčistit `replay_buffer.pkl` (§6).
3. Spustit až PO vyhodnocení no-reset série; kombinace s BB_NO_RESET je
   technicky nezávislá (dotýkají se různých kroků run_iteration), ale
   metodicky = 2 změny → pořadí dle §7.
4. `weights_cert_842c200_20260616.json` je untracked — před aktivací ověřit,
   že v main rootu stále existuje (bootstrap by jinak spadl s jasnou chybou).
