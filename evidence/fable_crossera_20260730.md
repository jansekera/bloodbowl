# Cross-era analýza: weight-space telemetrie + HtH turnaj snapshotů (30.07.2026, Fable)

Centrální otázka: **učíme se v dlouhém oblouku?** — měřeno nezávisle na gate.
Skript: `diag_crossera_20260730.py` (subcommandy `telemetry` / `compat` / `tournament` / `report`).
Data: `diag_crossera_20260730_telemetry.json`, `diag_crossera_20260730_results.json`.
Nic produkčního nebylo změněno (weights_best.json ověřen bajt-po-bajtu identický se zálohou 29.06).

---

## Část A — Weight-space telemetrie (145 souborů: 142 snapshotů + 3 champion backupy)

### A.1 Kompatibilita formátů (kompletní census)

| Formát | Počet | Poznámka |
|---|---|---|
| `alphazero_neural`, 73 featur, policy 96×64 (aktuální) | 20 snapshotů + 3 backupy | plně srovnatelné se šampionem (červen 25+ a celý červenec) |
| legacy `neural` (klíče W1/b1/W2/b2), 70 featur, bez policy | 76 | value srovnatelná přes překryv prvních 70 řádků W1 (featury 70–72 byly APPENDOVÁNY — feature_extractor.h fix #3, ověřeno v kódu) |
| `alphazero_neural`, 70 featur, policy 85×32 / 85×64 / 93×64 / linear85 | 18 | value srovnatelná přes překryv; policy část nesrovnatelná (jiná dimenze akčních featur) |
| `alphazero_linear` / plain array (60–99 KB, resp. ~1,6 KB) | 24 | **vyřazeno** — lineární value, žádný neuronový prostor ke srovnání |
| `neural` hidden_size=128 (`weights_snap_e25_70pct_+0.8.json`) | 1 | **vyřazeno** — jiná architektura skryté vrstvy |

Engine POZN.: `value_function.cpp` `evaluate()` používá `min(numFeatures, inputSize_)` →
staré 70-featurové sítě jsou hratelné i na dnešním enginu (jen „nevidí" 3 nejnovější
loose-ball featury, na kterých ani nebyly trénované). Všech 6 turnajových kotev prošlo
1-game load testem.

### A.2 Vzdálenost od šampiona v čase (medián po měsících; value70 = překryvový prostor)

| Éra | n | value L2 od šampiona (medián) | value cosine | policy L2 (jen plný formát) |
|---|---|---|---|---|
| květen | 64 | **12,13** (47 snapů „cizí linie", cos ≈ 0,00) / 2,8–3,0 (17 snapů příbuzných, cos ≈ 0,95) | 0,018 | — |
| červen | 39 | **0,44** (od 17.06. stabilně 0,42–0,47) | 0,9988 | 0,81 |
| červenec | 14 | **0,33** (e16 kandidáti 0,21–0,40; e2 smoke jen 0,06) | 0,9993 | **2,71 (roste monotónně 1,5→3,5)** |

Pro kontext: ‖value šampiona‖ = 8,88, ‖policy šampiona‖ = 11,88.

Klíčová čtení:
- **Květnová éra je jiná linie.** 47 z 64 květnových snapů má cos ≈ 0 vůči šampionovi —
  prakticky nesouvisející síť (jiný init/lineage). Většina květnových souborů má navíc
  identický mtime 06.05. 13:43 (hromadné kopírování) → mtime je u nich jen přibližná éra.
- **Červen = konvergence k dnešnímu šampionovi**: 2,0 (27.05.) → 0,4 (polovina června).
  Šampion je obsahově identický s `weights_snap_e16_91pct_+2.2.json` (26.06.) — to je jeho zdroj.
- **Červenec = value se skoro nehýbe, policy driftuje kumulativně.** Value část 16-epochových
  kandidátů končí 0,21–0,40 od šampiona (relativně ~4 % normy). Policy část ale roste
  MONOTÓNNĚ s časem: 0,66 (29.06.) → 1,48 (08.07.) → 2,06 (13.07.) → 2,50 (21.07.) →
  3,29 (23.07.) → 3,46 (29.07.), tj. ~29 % normy. **Policy hlava se skutečně přenáší přes
  iterace a kumulativně se vzdaluje — přesně dle očekávání.** (Value se resetuje na šampiona
  každou iteraci, proto se nevzdaluje.)

### A.3 Driftují kandidáti jedním směrem, nebo náhodně kolem šampiona?

**Jedním směrem.** Párové kosiny rozdílových vektorů (snap − šampion) mezi 10 červencovými
e16 kandidáty:

| část | mean cos | medián cos | min | max |
|---|---|---|---|---|
| value | 0,65 | **0,89** | −0,42 | 0,96 |
| policy | 0,91 | **0,93** | 0,72 | 1,00 |

cos(diff 08.07., diff 28.07.) = 0,86 (value), 0,72 (policy). Náhodný rozptyl v ~4700/6300-dim
prostoru by dával cos ≈ 0. **Trénink táhne kandidáty konzistentně do stejného místa a gate je
opakovaně vrací zpět** — konzistentní s hypotézou „zahazujeme malé zisky" (29.07.), ale sama
telemetrie nerozhodne, jestli ten směr je zlepšení, nebo systematický self-play bias. To
rozhoduje turnaj (část B).

### A.4 Ověření „weight_norm_change ~0.0003/epocha"

`epoch_metrics.csv` má jen 2 řádky (item2 smoke run, epochy 1–2): −0,0004 a +0,0003.
V kódu (`training_loop.py:404`) je to `‖W‖_after − ‖W‖_before`, tedy **změna normy, ne velikost posunu**.
Proti souborům:
- e2 snapshoty (2 epochy): |Δnormy| vůči šampionovi 0,0001 a 0,0027 → ~0,0001–0,0014/epocha — **řádově sedí**.
- e16 kandidáti: |Δnormy| = 0,005–0,037 (~0,0003–0,0023/epocha) — sedí, ALE jejich **L2 posun je 0,21–0,40,
  tj. ~10–15× větší než změna normy**. Váhy se tedy hlavně OTÁČEJÍ po slupce konstantní normy.
  → Metrika `weight_norm_change` je pravdivá, ale jako indikátor učení silně podhodnocuje reálný pohyb;
  správná metrika by byla ‖ΔW‖ (L2 update normy), ~0,013–0,025/epocha.

---

## Část B — Cross-era HtH turnaj

Kotvy (kompatibilita každé ověřena 1 hrou):

| kotva | soubor | éra obsahu | formát |
|---|---|---|---|
| may06 | weights_snap_e8_83pct_+1.0.json | ≤06.05. | 70f value-only, „cizí linie" (cos ≈ 0 k šampionovi) |
| jun19 | weights_snap_e8_90pct_+1.3.json | 19.06. | 70f + policy 93×64 |
| champion | weights_best.json | 26.–29.06. | 73f (aktuální šampion) |
| jul21 | weights_snap_e16_94pct_+2.3.json | 21.07. | 73f |
| jul28 | weights_snap_e16_99pct_+2.5.json | 28.07. (item1 e16 sourozenec) | 73f |
| jul29 | weights_snap_e2_100pct_+2.9.json | 29.07. (item2 smoke e2) | 73f |

Režim = produkční gate: MCTS=100, vf_blend=0,15, epsilon=0, dirichlet_alpha=0,0,
exploration_c=1,0, TV 1200, rotace 5 ras, sdílená `weights_policy.json` (prior-floor režim;
policy_blend=0 → **turnaj porovnává VALUE sítě**, stejně jako gate — naučené policy hlavy
jednotlivých kotev se ve hře nepoužívají, engine je z weights_path nečte).
15 párů × 13 seedů × 2 orientace (home/away swap na stejném seedu) = 390 her.

### B.1 Matice výsledků (řádek vs sloupec: V-R-P, decisive win rate [95% Wilson CI])

*(doplněno po doběhnutí turnaje)*

### B.2 Bradley-Terry žebříček

*(doplněno po doběhnutí turnaje)*

### B.3 Odpověď na hlavní otázku

*(doplněno po doběhnutí turnaje)*

## B.1–B.3 Turnajové výsledky (doplněno lokálně po session-limit pádu agenta; zdroj diag_crossera_20260730_results.json, 390 her)

Chess-score žebříček (podíl bodů, remíza=0,5; 130 her/kotva):
jul29 (item2 e2, MCTS400) 55,8 % > jun19 55,0 % > champion 54,2 %
> jul21 52,7 % > jul28 (item1 e16) 46,9 % > may06 35,4 %.

Klíčové páry (decisive % prvního): may06 vs champion 18 % | may06 vs
jul21 13 % | jun19 vs champion 73 % (11 dec.) | champion vs jul29 43 %
| champion vs jul21 65 %.

Závěry: (1) květen→červen = velké reálné zlepšení — pipeline se
prokazatelně učila; (2) od poloviny června všechny kotvy v pásmu
47–56 % ≈ šum → plateau je REÁLNÉ, ne artefakt měření; (3) item2 e2
na špici a porazil šampiona 8-6 — slabý, šumový plus pro MCTS=400;
(4) item1 e16 nejslabší po-květnová kotva (konzistentní s REJECTED).
Kompletní matice: viz results JSON + evidence/fable_synthesis_20260730.md.
