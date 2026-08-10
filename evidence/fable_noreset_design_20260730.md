# NÁVRH: "No-reset" akumulační experiment (30.07.2026)

**STATUS: ČISTĚ NÁVRH — nic nebylo spuštěno ani implementováno. Rozhodnutí o spuštění je na uživateli.**

Autor: Fable 5 (architekt). Vše níže ověřeno čtením kódu (file:line), ne z paměti.

## Hypotéza k otestování

Gate občas zamítá reálná malá zlepšení a každý REJECTED resetuje trénink na zamrzlého
šampiona: `run_iteration.py:472` kopíruje `weights_best.json → weights_az_train.json` na
startu KAŽDÉ iterace; šampion je beze změny od 29.06 (`champion_backup_91.5pct_20260629`).
Série malých reálných zisků (+1–2 pp/iteraci, jednotlivě pod prahem gate ~52,6 %) se nikdy
nesmí složit → vypadá jako týdny plateau.

**Důležitá výjimka, kterou návrh zohledňuje:** policy hlava se už DNES přenáší napříč
iteracemi (`_carry_over_policy` run_iteration.py:474 vloží policy z minulé iterace do
az_train; `_stash_policy` :512 ji po tréninku nepodmíněně uloží). Reset se tedy týká jen
**value hlavy**. Navíc: policy je při inferenci funkčně inertní (POLICY_BLEND=0.0,
run_iteration.py:73 — v tréninku i v gate; gate ji načítá jen kvůli prior-floorům,
run_iteration.py:88+544, obsah se nevyhodnocuje) a `replay_buffer.pkl` také přežívá mezi
iteracemi (training_loop.py:208–209). **Jediný kanál, kde reset reálně zahazuje naučený
obsah a kde ho gate umí vidět, je value hlava přes GATE_VF_BLEND=0.15
(run_iteration.py:52).** To je přesně to, co experiment měří.

---

## 1. Mechanika

### 1.1 Princip

Trénovat N iterací **bez resetu**: iterace i+1 startuje z kandidáta iterace i (i po
REJECTED). Měřicí kotva = **PŮVODNÍ šampion, bajtově fixní celou dobu**. Klíčový trik:
stávající anti-regression gate (600 her kandidát vs frozen, run_iteration.py:624–657) **JE
přesně to kumulativní HtH měření, které potřebujeme** — stačí (a) místo `weights_best`
zmrazit fixní kotvu a (b) verdikt gate jen zaznamenat, nikdy podle něj neresetovat.
Kumulativní HtH křivka pak padá zadarmo do `gate_history.jsonl` (chess_score + Wilson CI,
run_iteration.py:744–763), žádné extra hry navíc.

Rostoucí trend chess_score přes iterace = důkaz akumulace i pod prahem jednotlivých kroků.

### 1.2 Patch skica (minimální zásah do run_iteration.py, env-gated)

Vše za novým env přepínačem `BB_NO_RESET=1`; s vypnutým flagem je chování bajtově původní.

```python
# (A) ke konstantám u env přepisů (~ř. 70):
NO_RESET = os.environ.get('BB_NO_RESET', '0') == '1'
ANCHOR_PATH = PROJECT_ROOT / 'weights_anchor_noreset.json'

# (B) Step 1 — freeze (ř. 424): frozen pro gate = fixní kotva, ne best
if NO_RESET:
    if not ANCHOR_PATH.exists():                      # 1. iterace experimentu
        shutil.copy2(str(best_path), str(ANCHOR_PATH))  # kotva = šampion
    shutil.copy2(str(ANCHOR_PATH), str(frozen_path))
else:
    shutil.copy2(str(best_path), str(frozen_path))    # původní chování

# (C) reset-on-start (ř. 472–474): v no-reset módu pokračovat z minulého kandidáta
if not (NO_RESET and az_train_path.exists()):
    shutil.copy2(str(best_path), str(az_train_path))
    _carry_over_policy(az_train_path, best_path, policy_cache_path)
# else: az_train už nese value i policy z minulé iterace — nekopírovat nic
# (_carry_over_policy se NESMÍ volat s best jako zdrojem value — přepsala by
#  akumulovanou value hlavu zpět šampionem, viz její tělo ř. 356–369)

# (D) Step 5 — promote/reject (ř. 733–742): weights_best se NIKDY nedotknout
if NO_RESET:
    print(f'[NO-RESET] verdikt {label} pouze zaznamenán; weights_best beze změny')
    if gate_path != az_train_path:                    # selekci vyhrál train_best
        shutil.copy2(str(gate_path), str(az_train_path))  # příští start = vítěz selekce
elif promote:
    ... (původní blok ř. 734–739)
else:
    ... (původní blok ř. 741–742)
```

Poznámky:
- Bod (D) druhý řádek řeší, že selekce (Step 3, ř. 576–612) může vybrat
  `weights_train_best.json` (nejlepší self-play epocha, training_loop.py:502–503) — pak
  musí příští iterace pokračovat z něj, jinak by se větev zahodila.
- `gate_history.jsonl` dostane navíc pole `no_reset: NO_RESET` (1 řádek v dictu ř. 744) —
  ať se experimentální záznamy nedají splést s produkčními.
- Krok 2 (self-play) se nemění: training_loop.py:170–176 si na startu kopíruje
  `weights_best → weights_frozen` jako frozen self-play oponenta — protože `weights_best`
  zůstává celou dobu šampion, je oponent v obou režimech identický (kotva). Experiment tak
  izoluje čistě efekt resetu na inicializaci učícího se modelu, nic jiného se nemění.
- Per-iterační archiv: `epoch_metrics.csv` se při každém tréninku PŘEPISUJE
  (training_loop.py:152, mód 'w') — wrapper skript ho po každé iteraci zkopíruje do
  `evidence/noreset_iter{i}_epoch_metrics.csv` (+ kopii `weights_az_train.json` jako
  `weights_noreset_iter{i}.json` pro pozdější forenzní analýzu/Elo).

Spuštění (wrapper, ilustrativně):
```bash
BB_NO_RESET=1 BB_GATE=900 setsid nohup python3 run_iteration.py --loop 6 --no-push \
  > training_noreset_20260730.log 2>&1 & disown
```

## 2. Bezpečnost

### 2.1 Inventura zápisových cest na weights_best.json (ověřeno v kódu)

| Cesta | Kde | Stav v experimentu |
|---|---|---|
| první vytvoření | run_iteration.py:460–461 | neaktivní (best existuje) |
| abort před tréninkem | run_iteration.py:469 (frozen→best) | obsahově no-op; s patchem (B) frozen=kotva=šampion |
| abort promote | run_iteration.py:680 (frozen→best) | dtto |
| PROMOTED | run_iteration.py:734 (gate_path→best) | **vypnuto patchem (D)** |
| REJECTED | run_iteration.py:741 (frozen→best) | **vypnuto patchem (D)** |
| `_git_push`: `git reset --hard origin/main` + přepis best | run_iteration.py:793, 798–804 | **vypnuto `--no-push`** (guard ř. 766) — proto je `--no-push` POVINNÉ, ne volitelné |
| training_loop "New best!" | training_loop.py:670–674 | **vypnuto standalone guardem** (commit 7d29fc1 ověřen: `standalone = weights_path.resolve() == best_path.resolve()`, ř. 669; trénujeme na weights_az_train → False). Pozn.: `_git_push_weights_best` je beztak no-op (training_loop.py:854–856) |
| training_loop auto-revert | training_loop.py:680–698 | standalone-only + zapisuje jen weights_path (az_train), best nikdy |

Jiné zápisové cesty na weights_best v python/blood_bowl ani v run_iteration nejsou
(benchmark.py jen čte; `_check_regression` jen tiskne, training_loop.py:877+).
S patchem (B)+(D) a `--no-push` je jediný zbývající zápis obsahový no-op (frozen=šampion).

### 2.2 Izolace od produkce — doporučení: oddělený checkout

I když weights_best chráníme, sdílené pracovní soubory (`weights_az_train.json`,
`weights_frozen.json`, `weights_train_best.json`, `epoch_metrics.csv`, `score_log.csv`,
`replay_buffer.pkl`, `weights_snap_*`) by kolidovaly s JAKÝMKOLI souběžným produkčním
během (viz čerstvá zkušenost z item2 smoke). Proto:

1. Experiment běží v **odděleném checkoutu** (např. `~/claude/bloodbowl_noreset/`,
   git worktree + rebuild enginu `./setup.sh`), nikdy v produkčním adresáři.
2. Do checkoutu se zkopíruje startovní stav: `weights_best.json`, `weights_best_meta.json`,
   `weights_policy.json`, `replay_buffer.pkl` (parita s produkčním režimem).
3. **Nesouběžně s jiným tréninkem** — WORKERS=12 (run_iteration.py:127) saturuje stroj;
   dva běhy by si zkreslily wall-clock i per-game timeouty watchdogu.
4. Startovat až po doběhnutí item2 smoke a úklidu dle checklistu (standalone-guard memo).

### 2.3 Rollback plán

- Před startem: `champion_backup_before_noreset_20260730/` s `weights_best.json`,
  `weights_best_meta.json`, `weights_policy.json` + zaznamenat `sha256sum` všech tří.
- Po každé iteraci i po konci experimentu: ověřit sha256 produkčního `weights_best.json`
  proti záznamu (mělo by být triviálně shodné — experiment běží jinde; kontrola je levná
  pojistka proti lidské chybě).
- Vzor už existuje: `champion_backup_91.5pct_20260629`, `..._before_item1_...`,
  `..._before_item2_...` (obsah: weights_best.json + meta, ověřeno).
- Experiment nic nepushuje; případný nový šampion z experimentu se smí do produkce dostat
  JEDINĚ standardním plným gate během proti aktuálnímu weights_best (viz §5), nikdy ruční
  kopií.

### 2.4 Kritéria předčasného ukončení (reálná regrese vs šum)

σ chess_score při ~150 decisive (gate 600) je 4,1 pp; při ~225 decisive (gate 900) 3,3 pp.
Kvantifikovaná pravidla:

- **STOP (regrese):** kumulativní HtH vs kotva < 45 % při ≥200 decisive ve **dvou po sobě
  jdoucích iteracích** (jedna iterace <45 % je ~1,4σ událost, ~8 % falešný poplach;
  dvě za sebou <1 %), NEBO jediná iterace < 40 % (≈2,8σ) — kandidát reálně degraduje,
  ukončit a analyzovat (výsledek (c), §5).
- **STOP (kolaps value):** benchmark vs random < BM_FLOOR 77 % (run_iteration.py:56) ve
  dvou po sobě jdoucích iteracích, nebo NaN/exploze `weight_norm_change` v epoch_metrics.
- **STOP (technické):** abort_promote větev (visící engine, nekompletní gate,
  run_iteration.py:667–681) ve dvou iteracích za sebou.
- Plochý průběh NENÍ důvod k ukončení — je to informativní výsledek (b).

## 3. Měření

### 3.1 Primární: kumulativní HtH vs kotva + power úvaha

Cíl: detekovat trend +1–2 pp/iteraci přes 4–6+ iterací.

- Gate 600 her → ~75 % remíz (run_iteration.py:53) → ~150 decisive → σ ≈ 4,1 pp/bod.
- **Doporučení: BB_GATE=900** → ~225 decisive → σ ≈ 3,3 pp/bod, za ~+25 min/iteraci.
- Lineární regrese chess_score na pořadí iterace, SE směrnice = σ/√Σ(x−x̄)²:
  - N=6, gate 900: SE ≈ 0,79 pp/iter → +1,5–2 pp/iter detekovatelné (z ≈ 1,9–2,5); +1 pp
    marginální (z ≈ 1,3).
  - N=8, gate 900: SE ≈ 0,51 pp/iter → i +1 pp/iter na z ≈ 2.
  - N=4, gate 600: jen kvalitativní čtení / efekty ≥ 2,5 pp/iter.
- Doplňkový pooled kontrast (robustní k nelinearitě): iterace (N−1,N) vs (1,2), každý pool
  ~450 decisive → SE rozdílu ≈ 3,3 pp; očekávaný Δ při +1,5 pp/iter a N=6 je ~6 pp (z≈1,8).
- Experiment je **prodloužitelný bez ztráty**: stav žije v souborech, další `--loop K`
  pokračuje tam, kde skončil — začít levně a při náznaku trendu prodloužit je legitimní.

### 3.2 Sekundární metriky (per iterace, z archivovaných epoch_metrics.csv)

Sloupce ověřeny: `epoch,nil_nil_rate,mean_abs_vf,weight_norm_change,grad_norm,policy_loss,policy_top1_agreement,mcts_visit_entropy,pre_td_ramp`.

- `weight_norm_change` — má zůstat ohraničené; monotónní růst bez růstu HtH = drift, ne učení.
- `policy_loss` + `policy_top1_agreement` — policy už dnes akumuluje; srovnat trajektorii
  s historickými produkčními iteracemi (mění kontinuita value něco na učení policy?).
- `mcts_visit_entropy` — klesající = ostřejší search (viz MCTS-budget proba 21.07).
- `nil_nil_rate`, `mean_abs_vf` — sanity value signálu.
- **Decision-churn nástroj (vzniká paralelně, Track-2):** po každé iteraci spustit na
  kandidáta vs kotvu na fixní sadě situací → % změněných rozhodnutí/iteraci. Rozliší
  „váhy se hýbou, rozhodnutí stejná" (drift) od reálné behaviorální změny; zároveň plní
  metodiku „konkrétní situace > agregáty" — archivovat situace, kde se rozhodnutí změnilo.
- `gate_history.jsonl` navíc zdarma nese benchmark_vs_random, selekční H2H a side-audit.

## 4. Cena a doporučená konfigurace

Reference: plná iterace MCTS=100 (16 ep × 40 her + benchmark 400 + selekce 150 + gate 600)
≈ **7 h**; item2 smoke MCTS=400 (2 ep × 20 her + gate/benchmark) ≈ 8 h → eval hry škálují
~lineárně s MCTS budgetem (run_iteration.py:134–139), plná iterace při MCTS=400 odhadem
**25–30 h**.

| Varianta | Konfigurace | Cena | Co umí |
|---|---|---|---|
| **Levná (go/no-go)** | MCTS=100, N=4, BB_GATE=600, jinak produkční defaulty | 4×7 h ≈ **28 h** | jen hrubý trend (≥2,5 pp/iter) nebo jasná regrese; rozhodne o prodloužení |
| **Plná (doporučeno)** | MCTS=100, **N=6–8**, **BB_GATE=900**, jinak defaulty | 6×7,5 ≈ **45 h** / 8×7,5 ≈ **60 h** | +1,5–2 pp/iter na z≈2 (N=6), +1 pp/iter (N=8) |
| MCTS=400 | nedoporučeno pro tento experiment | ~25–30 h/iter → 8–10 dní na N=8 | hypotéza se týká reset mechaniky, ne search budgetu; MCTS=400 až jako follow-up na vítězi |

**Doporučení: plná varianta N=6 @ MCTS=100, BB_GATE=900, s explicitní opcí prodloužit na
N=8, pokud po 6 iteracích bude směrnice v pásmu +0,5–1,5 pp/iter (sugestivní, ne
průkazné).** Odůvodnění: (i) měříme dynamiku akumulace, ne absolutní sílu — MCTS=100 je
režim, ve kterém plateau vzniklo, tedy správný režim pro test; (ii) 2 dny wall-clock je
srovnatelné s jedním item2-stylem experimentem; (iii) prodloužení je zadarmo (stav v
souborech).

## 5. Interpretační matice (předem)

| Výsledek | Co říká o hypotéze | Další krok |
|---|---|---|
| **(a) Rostoucí** kumulativní HtH (směrnice >0, z≥2; typicky přes ~55 % v iteraci 4–6) | Hypotéza POTVRZENA: malé zisky jsou reálné a skládají se; reset-on-reject je zahazoval. Plateau bylo artefaktem promotion politiky. | Změnit promotion politiku (viz §6: nejdřív periodická re-kotva / delší akumulační okna, ne nutně soft-promote). Finálního kandidáta protáhnout standardním plným gate vs aktuální weights_best + benchmark floor; při průchodu promotnout normální cestou. |
| **(b) Plochý ~50 %** (směrnice ≈0, CI přes nulu) | Hypotéza (v testovatelné části) VYVRÁCENA: resety nezahazují měřitelné value zisky; plateau je reálné (konzistentní s diagnózou policy-plateau 17.07 a value-blind self-play 24.07). **Caveat:** gate je obsahově slepý k policy hlavě (policy_blend=0 i v gate) — pokud paralelní audit „policy_blend=0 v gate" potvrdí, že učení žije v policy, ploché HtH akumulaci v policy NEvyvrací. Rozhodne decision-churn + policy_loss/top1: pokud i ty stojí, plateau je skutečné. | Pozornost přesměrovat na policy_blend bring-up (krok 5 z AZ plánu), featury/kapacitu value sítě, případně MCTS budget (pozitivní proba 21.07). |
| **(c) Klesající** (splní stop kritéria §2.4) | Hypotéza vyvrácena a navíc: bez gate disciplíny model driftuje/degeneruje (self-play overfitting sám na sebe, value drift — známý samostatný problém, viz project_neural_policy_rootcause). Resety fakticky chrání. | Gate ponechat; z epoch_metrics + decision-churn identifikovat mechanismus driftu (weight_norm vs policy_loss); zvážit regularizaci / kotvení tréninku místo změn gate. |

Vazba na paralelní audit „problém je policy_blend=0 v gate": oba testy jsou komplementární
— tento experiment testuje *reset kanál value hlavy*, audit testuje *slepotu měření na
policy*. Matice: (a)+audit-negativní → reset byl hlavní problém; (b)+audit-pozitivní →
problém je měřicí slepota, ne reset; (b)+audit-negativní → plateau je reálné;
(a)+audit-pozitivní → oba problémy naráz (nejdřív opravit měření, pak promotion).

## 6. Alternativy — srovnání

1. **Elo / kontinuální tracking místo binárního gate.** Udržovat žebříček všech
   iteračních kandidátů + kotvy (archiv `weights_noreset_iter{i}.json` z §1.2 to
   umožňuje zpětně!), řídké párování. Výhoda: trend viditelný průběžně, robustní vůči
   šumu jednotlivých gate. Nevýhoda: víc infrastruktury a her za stejnou statistickou
   sílu; neodpovídá přímo na kauzální otázku „škodí reset?". Verdikt: dobrý **nástupce**
   gate, pokud vyjde (a); ne náhrada tohoto experimentu.
2. **Soft promote** (promote při chess_score>50 % bez marže, delší okno). V produkci by
   promotoval šum ~50 % času → random walk šampiona; kotva se hýbe, takže se z toho trend
   měří hůř než z tohoto experimentu. Fakticky jde o „no-reset s přepisováním
   weights_best" — striktně horší jako *měření*, potenciálně zajímavé jako *produkční
   politika* až PO potvrzení (a), a i pak bezpečnější s periodickou regret-kontrolou vs
   stará kotva.
3. **Periodický re-gate proti staré kotvě** (každých K iterací velké HtH vs K-iterací
   stará kotva, jinak vše beze změny). Pozor: **hypotézu nemůže potvrdit** — při dnešním
   reset-on-reject je šampion mezi promoty bajtově konstantní, takže re-gate „šampion vs
   starý šampion" měří 50 % z konstrukce; akumulaci, kterou resety zahazují, z principu
   nevidí. Verdikt: levný trvalý *monitoring pro dobu po změně politiky*, ne test.

Tento experiment (no-reset + fixní kotva) je jediná z variant, která hypotézu testuje
přímo a kauzálně, za cenu ~2 dnů wall-clock a s nulovým rizikem pro šampiona.

---

## Prerekvizity před spuštěním (checklist)

1. Doběhlý item2 smoke + úklid dle standalone-guard checklistu (paměť 29.07).
2. Oddělený checkout + rebuild enginu; zkopírovat startovní soubory (§2.2 bod 2).
3. `champion_backup_before_noreset_20260730/` + sha256 záznam (§2.3).
4. Patch dle §1.2 (env-gated, s BB_NO_RESET=0 bajtově původní chování) + commit PŘED
   spuštěním (politika „commit před tréninkem").
5. Wrapper na archivaci `epoch_metrics.csv` a `weights_noreset_iter{i}.json` po iteraci.
6. Decision-churn nástroj zapojit, jakmile bude z Track-2 k dispozici (není blokující).
