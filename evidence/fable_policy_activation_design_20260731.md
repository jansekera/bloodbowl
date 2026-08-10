# Návrh čistého experimentu: aktivace policy hlavy (31.07.2026)

Autor: Fable 5 (analytik/architekt). POUZE NÁVRH — nic nespuštěno, žádný existující soubor repa nemodifikován.
Kontext: běží no-reset série (PID 1984, `training_noreset_20260731.log`). Vše níže je plán na okno PO jejím
doběhnutí (~2 dny) a po jejím vyhodnocení. Navazuje na: `evidence/fable_pipeline_audit_20260730.md` (N1, N2),
`evidence/fable_policy_fairtest_20260730.md` (A2-1), `evidence/fable_crossera_20260730.md` (část A),
`diag_policy_fairtest_20260730.py` + `_results.json`.

---

## 0. TL;DR — doporučení

1. **První krok = potvrzovací fairtest blendu 0.2 s N=1600 her** (šampion+stash policy @0.2 vs šampion bez).
   Nulová změna pipeline, nulové riziko pro šampiona, jediná proměnná = blend na kandidátní straně. Přesná
   replika A2-1 metodiky, jen jedno rameno a ~11× víc decisive. Cena: **~2,5–3 h** (změřeno: A2-1 běžel
   9,8 her/min @ Pool(6), MCTS=100). Práh úspěchu: decisive WR ≥ 52,65 % (jednostranné α=0,05, σ=0,5/√960).
   Power: 99 % pro skutečný efekt +6 pp, 80 % pro +4 pp.
2. **Teprve při úspěchu kroku 1**: env-gated aktivace blendu v measurement path (gate kandidátní strana +
   benchmark + selection), `BB_GATE_POLICY_BLEND` default 0.0 = bajtově původní chování (vzor BB_NO_RESET,
   3dcb5b2). Součástí MUSÍ být promotion snapshot policy (`weights_best_policy.json`) — jinak gate výsledek
   znehodnotí pozdější drift stash policy pod šampionem.
3. **Aktivace policy v SELF-PLAY inferenci (oprava mrtvého epoch_blend) až PO vyčištění imitačních dat
   (A3-1 league mix)** — blend v tréninku vytváří zpětnovazební smyčku policy→search→visits→imitace a 50 %
   imitačních labelů dnes pochází z anti-random her. Aktivace v INFERENCI (kroky 1–2) na čistá data čekat
   nemusí: HtH test proti frozen je vůči anti-random biasu samo-korigující (bias by se projevil jako neúspěch).
4. **Během běžící série lze připravit vše kromě spuštění**: diag skript (nový soubor), proposal patch pro
   krok 2 (jako text v evidence, NEaplikovat), pre-registrovaný analytický plán (tento dokument), post-série
   checklist zmrazení artefaktů. Série průběžně PŘEPISUJE `weights_policy.json` (`_stash_policy`
   run_iteration.py:548 běží nepodmíněně i v NO_RESET) — testovat se bude post-série stav, snapshot až po
   doběhnutí.

---

## 1. Ověřená mechanika v kódu (citace soubor:řádek, stav k 31.07.)

Vše níže znovu ověřeno čtením zdrojáků dnes (ne jen převzato z auditů):

- **Akumulace policy**: `run_iteration.py:548` `_stash_policy(az_train_path, policy_cache_path)` — nepodmíněně
  po každém tréninku (před selection i gate); `run_iteration.py:510` `_carry_over_policy(...)` ji na začátku
  další iterace vrací do `weights_az_train.json` (definice :347–:383, stash :385–:397). V NO_RESET větvi
  (:505–:508) se carry-over přeskakuje, protože az_train už policy nese — stash na :548 ale běží dál, takže
  **běžící série weights_policy.json každou iteraci přepisuje**.
- **Mrtvý blend v self-play**: `run_iteration.py:27` `EPOCHS = 16`, `:119` `IMITATION_EPOCHS = 16` (env
  `BB_IMITATION_EPOCHS`), `:79` `POLICY_BLEND = float(os.environ.get('BB_POLICY_BLEND', 0.0))`, předává se
  do train_cli na :536 `--policy-blend=...`. V `python/blood_bowl/training_loop.py:324–327`:
  `if imitation_epochs > 0 and epoch <= imitation_epochs: epoch_blend = 0.0` — s IMITATION_EPOCHS==EPOCHS
  je podmínka pravdivá VŽDY → `epoch_blend=0` po celý běh, `BB_POLICY_BLEND` je fakticky mrtvý bez současného
  snížení `BB_IMITATION_EPOCHS`.
- **Gate/selection/benchmark**: `_gate_game` (:289–345) i `_benchmark_game` (:255–287) předávají
  `policy_weights_path` (aktivuje heuristické prior floors, `macro_mcts.cpp` gated na `config_.policy !=
  nullptr`), ale NIKDY `policy_blend` → binding default 0.0 → **naučený obsah se nečte**
  (`macro_mcts.cpp:358` čte síť jen při `config_.policyBlend > 0.0f`; blend na :553–:566:
  `prior = (1-b)·heuristic + b·policy`, pak renormalizace).
- **Binding už umí férovou asymetrii** (ea92bd0): `simulate_game_logged(..., policy_blend=, 
  away_policy_weights_path=, away_policy_blend=)`; `away_policy_weights_path=''` = away sdílí home síť →
  prior floors symetricky aktivní na obou stranách, blend nastavitelný per-strana. Přesně tak to používá
  `diag_policy_fairtest_20260730.py:41–48`.
- **Imitační data**: `training_loop.py:347–357` — mix her (self_batch + rand_batch, OPPONENT_MIX_RATIO=0.5
  na `run_iteration.py:127`) loguje do TÉHOŽ `epoch_log_dir`; `training_loop.py:448–462` — policy trénink
  bere `decisions_*.json` z celého adresáře bez rozlišení zdroje → **imitace se učí i z rozhodnutí nad hrami
  vs RANDOM (cca polovina)**. Během imitační fáze navíc `passes=5` (:458).
- **A2-1 výsledek** (diag_policy_fairtest_20260730_results.json, 450 her, MCTS=100, vf_blend=0.15, TV=1200,
  5 ras, side-swap páry, dirichlet 0.3 / exploration_c 0.5 = engine defaults):
  | blend | W | D | L | decisive | dec. WR | Wilson 95 % |
  |---|---|---|---|---|---|---|
  | 0.1 | 45 | 52 | 53 | 98 | 45,9 % | 36,4–55,8 |
  | 0.2 | 49 | 63 | 38 | 87 | **56,3 %** | 45,9–66,3 |
  | 0.3 | 46 | 64 | 40 | 86 | 53,5 % | 43,0–63,7 |
  Draw rate 35–43 % → počítej ~60 % decisive. Hrb na 0.2; CI všech ramen kříží 50 %.
- **Rychlost** (změřeno z `diag_policy_fairtest_20260730.log`): 125 her / 768 s @ Pool(6) → **9,8 her/min**,
  tj. ~590 her/h. To je relevantní kalibr pro stejný tvar experimentu (reference „gate 600 her ~ hodiny" je
  konzervativnější — jiný worker count / MCTS režim).
- **Drift** (fable_crossera část A): L2 vzdálenost stash policy od éry šampiona 0,66→3,46 za měsíc (~29 %
  normy) — akumulace reálně probíhá; policy testovaná v A2-1 už dnes NENÍ tou, která bude po doběhnutí série.

---

## 2. Rozhodovací strom experimentu

### Kandidátní první kroky a proč (ne)

**(a) Potvrzovací test blendu 0.2 s velkým N — ZVOLENO jako krok 1.**
- Nula změn v pipeline (nový diag skript, čtení weights, zápis results.json) → nejčistší izolace, jediná
  proměnná = blend kandidátní strany. Nulové riziko pro `weights_best.json`.
- Nejlevnější (~2,5–3 h) a rozhoduje o smyslu VŠECH dalších kroků: pokud obsah policy nemá HtH hodnotu ani
  při 960 decisive, aktivace kdekoli je předčasná a priorita se přesouvá na data (A3-1).
- A2-1 je inconclusive-pozitivní (56,3 %, CI 45,9–66,3) — přesně případ „slibný nález → do fronty jako
  samostatný, pořádně dimenzovaný experiment" (pravidlo jedné změny, feedback 30.07.).

**(b) Aktivace policy v inferenci gate+benchmark+self-play rovnou — ODMÍTNUTO jako první krok.**
- Zaváděla by se změna, jejíž hodnota není potvrzena (CI přes 50 %). Gate s blendem na kandidátní straně
  navíc mění definici „kandidátního systému" — to chceme udělat až s důkazem, a s promotion snapshotem
  policy (jinak nekonzistence frozen-baseline, viz §4.3).
- Kombinovala by ≥2 změny najednou (blend v gate + blend v benchmarku + selection) → nediagnostikovatelné.

**(c) Oprava mrtvého epoch_blend v self-play (BB_IMITATION_EPOCHS<16 + BB_POLICY_BLEND>0) — ODMÍTNUTO
jako první i druhý krok.**
- Mění distribuci tréninkových dat (search se řídí policy priors → jiné hry → jiné value i imitační cíle);
  efekt měřitelný až přes celé iterace gate (~12 h self-play + hodiny gate za JEDNU iteraci) — nejdražší
  a nejhůř izolovatelná varianta.
- Interaguje s data-quality problémem (§4): dokud 50 % imitačních labelů pochází z anti-random her, blend
  v self-play tu kontaminaci zesiluje zpětnou vazbou. Patří až ZA A3-1.

**(d) Nejdřív A3-1 (league mix místo random) a policy až pak — ODMÍTNUTO jako blokátor kroku 1.**
- Krok 1 měří hodnotu JIŽ EXISTUJÍCÍHO obsahu policy proti frozen šampionovi head-to-head. Anti-random bias
  obsahu by se v tomto měření projevil jako neúspěch — měření je vůči špinavým datům poctivé. Čekat na A3-1
  by jen odsunulo levné rozhodnutí o měsíce. A3-1 běží jako paralelní návrh nezávisle.

### Strom

```
KROK 0 (teď, během série): připravit skript + proposal patch + checklist (§6). NIC nespouštět.
KROK 1 (po doběhnutí a vyhodnocení série): potvrzovací fairtest blend 0.2, N=1600.
  ├─ ÚSPĚCH (dec. WR ≥ 52,65 %):
  │    KROK 2: env-gated aktivace v measurement path + promotion snapshot policy (§4.3),
  │            null-test gate (300 her), pak 1 ostrá iterace s BB_GATE_POLICY_BLEND=0.2.
  │      ├─ gate PASS → policy je oficiálně součást hraného systému; KROK 3 (self-play blend)
  │      │              zůstává ve frontě AŽ ZA A3-1.
  │      └─ gate REJECT → policy pomáhá šampionovi, ale kandidátům ne → analýza interakce
  │                       (blend × čerstvá value hlava), nezavádět plošně.
  ├─ INCONCLUSIVE (50 % < WR < 52,65 %): NEaktivovat. Efekt < +4 pp nestojí za komplexitu
  │    před vyčištěním dat → priorita A3-1, re-test po ~4 týdnech akumulace na čistých datech.
  └─ NEÚSPĚCH (WR ≤ 50 %): obsah policy bez HtH hodnoty i po měsíci akumulace → drift je šum
       nebo anti-random artefakt. Priorita A3-1 + zvážit reset stash policy při další éře.
```

Pozn.: krok 1 testuje JEDINÝ blend 0.2 (hrb z A2-1). Rozdělení N na víc ramen by zabilo power — ostatní
blendy případně později jako dose-response, až bude existence efektu potvrzena.

---

## 3. Protokol kroku 1 — potvrzovací test blendu 0.2 (pre-registrace)

### 3.1 Design

- **Hypotéza H1**: šampion (weights_best.json) s aktuální stash policy @ blend 0.2 na své straně porazí
  identického šampiona bez blendu s decisive WR > 50 %. H0: WR = 50 %.
- **Uspořádání**: přesná replika `diag_policy_fairtest_20260730.py`, jedno rameno B=0.2.
  Obě strany `weights_path=away_weights_path=weights_best.json`, vf_blend=0.15, MCTS=100, epsilon=0,
  TV=1200, rotace 5 ras (human/orc/skaven/dwarf/wood-elf), `policy_weights_path=<policy snapshot>`,
  `away_policy_weights_path=''` (sdílená síť → prior floors symetricky), blend jen na kandidátní straně,
  side-swap: každý seed hrán 2× (kandidát home i away). Dirichlet 0.3 / exploration_c 0.5 (engine defaults,
  parita s A2-1 — viz 3.6 pro robustnostní rameno).
- **N = 1600 her = 800 seed párů.** `SEED_BASE = 31_000_000` (disjunktní od A2-1, které mělo 30M).
- **Testovaný artefakt**: `weights_policy.json` VE STAVU PO DOBĚHNUTÍ no-reset série, zmrazený kopií
  `cp weights_policy.json evidence/policy_snapshot_postnoreset_<datum>.json` (skript čte snapshot, ne živý
  soubor — série ho do poslední chvíle přepisuje a případné budoucí běhy taky).

### 3.2 Power (σ = 0,5/√decisive)

Očekávaný decisive rate ~60 % (A2-1: 57–65 %). N=1600 → ~960 decisive → σ = 1,61 pp.

| skutečný efekt | potřeba decisive (80 % power, α=0,05 jednostr.) | ≈ her @60 % dec. | power @ N=1600 |
|---|---|---|---|
| +3 pp | ~1716 | ~2860 | ~57 % |
| +4 pp | ~964 | ~1610 | ~80 % |
| +5 pp | ~616 | ~1030 | ~93 % |
| +6 pp (bodový odhad A2-1) | ~427 | ~710 | ~99 % |

Interpretace: N=1600 spolehlivě rozhodne efekty ≥ +4 pp. Efekty +2–3 pp zůstanou šedou zónou — vědomě
akceptováno: efekt < +4 pp před vyčištěním dat nemá aktivační prioritu (viz strom, větev INCONCLUSIVE).

### 3.3 Rozhodovací kritéria (pre-registrovaná)

- **Primární**: decisive WR po 1600 hrách. Úspěch ⇔ WR ≥ 0,5 + 1,645·σ (při 960 dec. = **52,65 %**).
- **Interim pohled po 800 hrách** (~480 dec., σ=2,28 pp), jediný, Haybittle–Peto:
  - stop pro marnost: WR ≤ 50,0 %,
  - stop pro úspěch: z ≥ 2,8 (WR ≥ 56,4 %),
  - jinak pokračovat do plného N (finální práh beze změny — penalizace za interim je při H-P zanedbatelná).
- **Sekundární (reportovat, nerozhodují)**: (a) meta-kombinace s A2-1 ramenem 0.2 (87 dec. @ 56,3 %,
  stratifikovaně); (b) skóre W+0,5D/N — remízy mohou nést signál L→D (audit N6); (c) rozpad per-rasa a
  home/away symetrie; (d) draw rate vs A2-1 (sanity srovnatelnosti).
- **Sanity guardy**: pokud decisive rate < 45 % nebo > 75 % (mimo A2-1 pásmo), prošetřit před interpretací;
  pokud interim WR obou orientací (home/away) diverguje > 15 pp, podezření na asymetrii setupu → stop a audit.

### 3.4 Ochrana šampiona a hygiena

- Skript POUZE ČTE `weights_best.json` + policy snapshot; zapisuje jen
  `diag_policy_confirm_<datum>_results.json` (inkrementálně po každé hře — crash-resume vzor A2-1).
- Před startem: `md5sum weights_best.json weights_best_meta.json` zaznamenat do results.json; po doběhnutí
  ověřit beze změny. Žádné git operace, žádný zápis do weights_*, žádný gate/promote.
- Watchdog: `timeout=300` s per hra (standard pipeline); Pool s maxtasksperchild netřeba (A2-1 běžel čistě).

### 3.5 Cena

- 1600 her @ 9,8 her/min (Pool 6) ≈ **2,7 h**; s 10 workery (post-série je CPU volné) odhad ~1,7–2 h.
  Doporučuji ale držet Pool(6) kvůli paritě s A2-1 měřením (stejný kontencí režim) — rozdíl ceny ~1 h
  nestojí za novou proměnnou. Analýza + zápis evidence ~0,5 h. **Celkem ~3–3,5 h.**

### 3.6 Volitelné robustnostní rameno B (sekundární, až po primárním)

N=400 her @ blend 0.2 v **eval-paritě** (dirichlet_alpha=0.0, exploration_c=1.0 — režim gate po fixu
2026-07-10, run_iteration.py:96–117). Otázka: přežije efekt bez explorace v rootu? Cena ~40–50 min.
Nerozhoduje o kroku 2 (primární je replika A2-1), ale předpoví, jak se blend projeví v ostrém gate.

---

## 4. Krok 2 — env-gated aktivace v measurement path (připravit, implementovat až po úspěchu kroku 1)

### 4.1 Přesné změny (vzor BB_NO_RESET, 3dcb5b2: default = bajtově identické chování)

Vše v `run_iteration.py`, jediný nový env `BB_GATE_POLICY_BLEND` (default 0.0 = dnešní stav):

1. **:94–95 (u GATE_USE_POLICY_PRIORS)**: `GATE_POLICY_BLEND = float(os.environ.get('BB_GATE_POLICY_BLEND', '0.0'))`
   + komentář odkazující na tento dokument a výsledek kroku 1.
2. **`_gate_game` (:289–345)**: rozšířit tuple o 11. prvek `cand_policy_blend` (vzor postupného rozšiřování
   7→8→9→10 už v kódu je, zpětně kompatibilní). Ve volání `simulate_game_logged` doplnit:
   `policy_blend=(blend if not cand_is_away else 0.0)`, `away_policy_blend=(blend if cand_is_away else 0.0)`,
   `away_policy_weights_path=''` (sdílená síť, floors symetricky — dnešní chování zachováno). Frozen strana
   hraje blend 0 = přesně konfigurace, se kterou byla promotnuta → férové „systém vs systém".
3. **`_benchmark_game` (:255–287)**: analogicky — kandidátní macro_mcts strana dostane blend (obě orientace
   side-swapu), random soupeř policy nemá.
4. **Selection H2H (:~620–630)**: az_train vs train_best sdílejí TUTÉŽ stash policy → blend na OBĚ strany
   stejně (srovnává se value hlava za identického policy režimu).
5. **Promotion snapshot (Step 5/6, PASS větev)**: `shutil.copy2(weights_policy.json → weights_best_policy.json)`
   + do `weights_best_meta.json` zapsat `policy_blend` a md5 policy. **Bez tohoto je krok 2 nevalidní**: gate
   by schválil kombinaci (value, policy@t), ale stash policy dál driftuje (L2 +29 %/měsíc) a šampion by za
   týden hrál s jinou policy, než s jakou prošel. Budoucí frozen strana gate pak čte `weights_best_policy.json`
   (svou zmrazenou policy) místo sdílené sítě — od první promoce s blendem.
6. **Práh gate beze změny** (0,5 + k·σ na decisive) — mění se jen definice kandidátního systému, ne metr.

### 4.2 Validace před ostrým použitím

- **Null-test**: frozen vs frozen, blend 0.2 na OBOU stranách, N=300 → očekávané WR ~50 % (analog
  diag_null_weights.py z 21.07.). Odchylka > 2σ → bug v plumbing.
- Pak 1 ostrá iterace s `BB_GATE_POLICY_BLEND=0.2` a standardním vyhodnocením.

### 4.3 Cena

Implementace + review ~2–4 h; null-test ~30–40 min; ostrá iterace = běžná cena iterace (self-play ~12 h +
gate hodiny) — ale ta by běžela tak jako tak, marginální náklad ≈ 0.

---

## 5. Krok 3 — aktivace v self-play (mrtvý epoch_blend) — AŽ PO A3-1

Mechanicky triviální (`BB_IMITATION_EPOCHS=12` + `BB_POLICY_BLEND=0.1–0.2` → epochy 13–16 s blendem;
training_loop.py:324–327), ale záměrně poslední:
- mění tréninkovou distribuci (search řízený policy priors → jiné hry → jiné value/imitační cíle) — efekt
  měřitelný jen přes celé iterace, nejdražší validace;
- POZOR na skrytou druhou změnu: `passes` klesá z 5 na 1 mimo imitační fázi (training_loop.py:458) —
  snížení IMITATION_EPOCHS tedy mění i intenzitu imitačního tréninku, do návrhu experimentu započítat;
- hlavně: zpětná vazba na špinavá data, viz §6.

---

## 6. Data-quality: aktivovat před, nebo po vyčištění imitačních dat?

**Závěr: kroky 1–2 (inference) PŘED vyčištěním; krok 3 (trénink) až PO A3-1 (nebo po levnějším filtru).**

Proč inference nemusí čekat:
- Obsah stash policy už JE natrénovaný na špinavém mixu — krok 1 měří jeho reálnou hodnotu proti frozen
  šampionovi HtH (jediný uznávaný signál projektu). Pokud se policy naučila hlavně anti-random triky
  (rozhodnutí optimální jen proti pasivnímu soupeři), proti frozen macro_mcts to nepomůže a test skončí
  ≤ 50 % → měření je vůči kontaminaci samo-korigující. A2-1 hrb 56,3 % na 0.2 naznačuje, že i přes 50%
  kontaminaci malý čistý pozitivní obsah existuje — po vyčištění dat by měl být větší, ne menší.
- Blend 0.2 navíc drží 80 % váhy na heuristických priorech — mechanismus omezuje škodu miscalibrace.

Proč trénink čekat musí — rizika imitace z anti-random her (kvantifikace, kde jde):
1. **Distribuční posun stavů**: random soupeř nestaví klece, nemarkuje, nechává míč — polovina imitačních
   labelů je podmíněna stavy, které proti reálnému soupeři skoro nenastávají. `policy_top1_agreement`
   0,39–0,44 je agregát přes oba zdroje; per-zdroj rozpad neznáme (levné doměřit — viz §7 přípravy).
2. **Kvalita labelů**: MCTS visits proti random jsou ostřejší (snadné pozice, široké value marže) → v
   imitační loss (passes=5!) snadná anti-random rozhodnutí převáží těžká self-play rozhodnutí.
3. **Zpětná smyčka pouze při kroku 3**: blend v self-play → policy priors tvarují search → search generuje
   visits → visits jsou imitační labely → bias se amplifikuje a zužuje exploraci směrem k chování proti
   slabým soupeřům (root Dirichlet 0.3 to tlumí jen u kořene). V inferenci (kroky 1–2) tato smyčka
   neexistuje — policy se tam jen ČTE.
- Levnější mezikrok než celé A3-1: **filtrovat decisions podle zdroje hry** (imitovat jen self-play
  rozhodnutí; `training_loop.py:449–454` dnes bere vše z epoch_log_dir) — menší zásah než league mix,
  vhodný jako samostatný experiment ve frontě, NE přibalovat ke krokům 1–3 (jedna změna najednou).

---

## 7. Interakce s běžící no-reset sérií + co připravit předem

**Tvrdá pravidla**: experiment startuje až (a) PID 1984 doběhne, (b) série je vyhodnocena (master pointer
31.07.), (c) nic jiného neběží (feedback_verify_no_interference). Série navíc průběžně přepisuje
`weights_policy.json` i `weights_az_train.json` — jakýkoli snapshot před doběhnutím by byl mid-flight.

**Připravit lze hned (nové soubory, žádné spouštění, žádná modifikace):**
1. **Diag skript** `diag_policy_confirm_20260802.py` (nový soubor v rootu, NEspouštět): kopie A2-1 skriptu
   s jedním ramenem B=0.2, N_PAIRS=800, SEED_BASE=31_000_000, interim checkpoint po 800 hrách, md5 guard
   na weights_best, čtení policy SNAPSHOTU (cesta parametr). — připraveno k napsání, viz 3.1–3.4.
2. **Proposal patch kroku 2** jako textový návrh (tento dokument §4.1; případně samostatný
   `evidence/proposal_gate_policy_blend_20260731.md` s přesným diffem) — NEaplikovat do repa.
3. **Pre-registrace** = §3 tohoto dokumentu (N, prahy, interim pravidla, sekundární analýzy, seedy).
4. **Post-série checklist**:
   - ověřit exit PID 1984 + `pgrep -f run_iteration` prázdný (pozor na pkill self-kill gotchu),
   - vyhodnotit sérii (samostatný úkol, má přednost — její výsledek může změnit prioritu; kdyby no-reset
     kumulativní kandidát porazil kotvu, krok 1 přesto běží proti weights_best — baseline nemění),
   - `md5sum weights_best.json` vs poslední známý stav (série s BB_NO_RESET best nikdy nezapisuje —
     ověřit, ne věřit),
   - `cp weights_policy.json evidence/policy_snapshot_postnoreset_<datum>.json` + md5 do evidence,
   - spustit krok 1 (setsid+nohup+disown, log do rootu, inkrementální results.json).
5. **Per-zdroj agreement diag** (podklad §6): skript, který nad existujícími decisions_*.json z posledních
   epoch spočítá policy_top1_agreement zvlášť pro self-play a vs-random hry (rozlišitelné přes game_offset /
   pořadí v epochě, `training_loop.py:348–357`). Levné (sekundy CPU), ale i tak až po doběhnutí série —
   čte adresáře, do kterých série právě zapisuje.

**Časová osa po doběhnutí (orientačně):** vyhodnocení série (0,5–2 h) → snapshot + krok 1 (~3–3,5 h) →
při úspěchu implementace kroku 2 (2–4 h) + null-test (40 min) → ostrá iterace s blendem v dalším okně.

---

## 8. Checklist ochrany šampiona (souhrn)

- [ ] Krok 1 nečte-nezapisuje nic než: read weights_best.json + policy snapshot; write results.json.
- [ ] md5 weights_best.json před/po každém běhu, zaznamenáno v results.
- [ ] Žádné git operace v celém kroku 1; krok 2 jde do repa až po úspěchu kroku 1, commit s testy, env
      default off = bajtově původní chování.
- [ ] Gate práh nezměněn; promotion snapshot policy (weights_best_policy.json) je NOVÝ soubor, best nikdy
      nepřepisován mimo standardní PASS větev.
- [ ] Jedna změna najednou: krok 1 (měření) → krok 2 (measurement path) → [A3-1 / decision filter] →
      krok 3 (self-play). Nikdy dvě naráz.
