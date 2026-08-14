# Team 1 — Výsledky (Opus re-run, 2026-06-10)

> Re-běh kompletního briefu `team1_brief_per_player.md` se 4 specialisty na modelu **Claude Opus 4.8**.
> Každý specialista reálně přečetl C++/Python kód a OVĚŘIL/OPRAVIL předchozí odpovědi (generované slabším modelem).
> Zdrojový brief: `team1_brief_per_player.md` · předchozí nálezy: `team1_findings.md`.

---

## ⭐ Nejdůležitější nové nálezy (napříč specialisty)

1. **Faktická chyba v briefu — výkonnost:** value funkce se v MCTS volá **~100×/tah** (= `MCTS_ITERATIONS=100`, `mcts.cpp:149`), NE 400×. Skutečný hot-spot je **policy** (`computePriors` per akce per uzel). → architektura musí **cachovat sdílený trunk per uzel** a přepočítávat jen action head. (ML/RL)

2. **„NUM_FEATURES je jeden bod změny" — NEPRAVDA.** Reálně ~12 míst. Dva velké přepisy (C++ `extractFeatures` + **paralelní Python `extract_features` v `features.py:9`, který má vlastní `NUM_FEATURES=70`**) + skrytý hardcode `policy_network.cpp:44` `float hidden[64]` se stropem `min(H,64)`. (C++ Analyst)

3. **Pathfinder NErespektuje tacklezones.** `canReachAdjacentTo` (`pathfinder.cpp:20`) má pole `PathNode::dodged` jako **mrtvý kód** (deklarováno, nikdy nenastaveno). `carrierBlitzable` dnes používá pouhý Chebyshev (`feature_extractor.cpp:363`) → nadhodnocené. BFS nutno rozšířit o dodge/TZ. (C++ Analyst, ML/RL)

4. **Dvě pasti BB2016 — engine se LIŠÍ od pravidel** (features musí kopírovat engine, ne pravidla):
   - ~~**Sure Hands NEkontruje Strip Ball** v enginu~~ → **OPRAVENO 2026-06-10** (`block_handler.cpp:460` přidáno `&& !def.hasSkill(SureHands)`, nový test `StripBallNegatedBySureHands`). Engine teď sjednocen s pravidly BB2016: Sure Hands ruší Strip Ball. **Pozor: mění balanc → vyžaduje re-train.**
   - **Guard ZApočítává asistenci i u foulů** (`foul_handler.cpp:19` volá stejné `countAssists`) — STÁLE odchylka od BB2016 pravidel, neopraveno. (Game Domain)

5. **Gating je téměř čistý šum kvůli remízám.** Reálně **200** benchmark her (ne 400; `//2` na `run_iteration.py:171`), **66–72 % remíz** → efektivní vzorek ~34 %. `ANTI_REGRESSION=0.51` leží uvnitř CI (±0.026–0.029). Pro signál +3 % chess je třeba **>1000 her**, nebo přejít na **EMA benchmark + decisive-only winrate**. (Training)

6. **TD(λ) je hotový, ale neaktivní** (`trainer.py:466–530`); pipeline jede `mc_shaped` (`run_iteration.py:158`). Zapnout pro cold start. `stallIncentive` (`feature_extractor.cpp:236`) aktivně odměňuje pasivitu i při remíze → spoluviník nil_nil. (ML/RL, Training)

7. **Doporučení směru: nejdřív inkrementální 70→~150 features** (carrier slot + 5 klíčových hráčů), které zachová warm start přes `_align_features`, jako levný test hypotézy. Plný 492 (cold start od nuly, `warm_start_expand` NEumí změnu počtu vstupů) až po důkazu, že per-player prolomí strop. (Training)

---

## 1) ML/RL Architect

### Q1 — Slot ordering
- **Distance-based** zůstává: slot 0 = ball carrier; 1–10 moji; 11–21 soupeři.
- **ZMĚNA: řadicí klíč musí mít deterministický tie-break přes `player.id`** (roster index), ne podle pozice. `feature_extractor.cpp` dnes řadí Chebyshevem → dva hráči se stejnou vzdáleností přehazují sloty mezi tahy. Nestabilní ordering rozbíjí V(s) **uvnitř jedné MCTS search** (value se počítá na každém listu, `mcts.cpp:149,255-258`).
- **Odmítá hybridní ordering** (slot 1 = ball-blitz kandidát) jako nestabilní — „nejbližší soupeř s Wrestle" skáče mezi hráči.
- **Off-pitch hráči (KO/INJ/DEAD):** NE fixní roster slot (kolize s distance orderingem), ale **na konec své skupiny** + `on_pitch=0` + zero-fill. PRONE/STUNNED jsou stále on-pitch s vlastním stav-flagem.

### Q2 — Architektura pro ~492 vstupů
- **MLP, NE transformer.** Topologie: `492 → 256 → 128 → {value head →1 tanh, policy head →logit}`, **sdílený trunk**.
- **Vynucené C++ změny PŘED tréninkem** (jinak to nepojede):
  1. Policy: zrušit `float hidden[64]` strop (`policy_network.cpp:44`), parametrizovat šířku.
  2. Value: `W1_` flat row-major místo `vector<vector<float>>`; `hidden` buffer jako member (dnes heap-alloc per `evaluate()`, `value_function.cpp:36`).
  3. **Cache sdíleného trunku per MCTS uzel** — dnešní `evaluateAction` přepočítává celý vstup pro každou akci (`policy_network.cpp:47-62`), u 492 vstupů neúnosné. Policy head dostává jen `[128 trunk ⊕ 15 action features]`.
  4. Sloučit dnešní 2 oddělené sítě (policy + value) do **jeden trunk, dvě hlavy**, jeden JSON.
- float32 inference stačí; Xavier init zachovat. Odhad: low jednotky ms/tah.

### Q3 — Warm start
- **W1 náhodná inicializace (Xavier), ŽÁDNÝ přenos** — mapuje jiný vstupní prostor.
- **ZMĚNA: žádný přenos output biasu** (předchozí návrh `mean(old_weights)` špatně definovaný pro tanh value); `b2=0`.
- **Past:** `_align_features` (`trainer.py:236`) tiše zarovná starou 70-síť místo selhání → nutné vytvořit čerstvý trainer s `n_features=492`.
- **LR schedule:** loop 1–4 = 5e-4 · loop 5–12 = 3e-4 · loop 13+ cosine decay → 1e-4. (492→256 má ~126k vah ve W1, 28× víc parametrů než dnešní 70×64.)

### Q4 — Interaction features (3 → **5 explicitních**)
Předpočítat to, co vyžaduje pathfinding/asistenční pravidla nebo je čistě poziční nelinearita:
1. `dist_to_ball` (triviální).
2. `in_carrier_diagonal` (cage roh; XOR-like, síť odvozuje draze).
3. `can_blitz_carrier[j]` — **BFS flood-fill, NE Chebyshev** (`feature_extractor.cpp:363`).
4. **`net_st_for_block[i]` per hráč** (NOVÉ) — engine už má `countAssists`+`getBlockDiceInfo`; asistenční logika je nelineární, síť ji z raw statů spolehlivě neodvodí.
5. `enemy_tz_count` per hráč (čí TZ, ne jen kolik).

Síti nechat: Wrestle/Block/StripBall combo výsledky, Frenzy trap, pass-lane jemnosti.

### Q5 — nil_nil
- **ZMĚNA — snižuje optimismus:** per-player samy nesrazí 44 %→10–15 %, realisticky **44 %→~30 %**. Důvod: extractor už dnes nese 14 strategických agregátů (`feature_extractor.cpp:56-69`), informační mezera je menší než brief tvrdil.
- nil_nil je hlavně **reward/credit problém.** Priorita fixů: **TD(λ=0.9)** → **draw penalty** (dnes draw=0, `trainer.py:82-89`; nastavit −0.1…−0.2) → **osekat `stallIncentive`** (`feature_extractor.cpp:236-242` odměňuje stall i při remíze).
- Kombinací cíl ~15 % nil_nil.

### Q6 — Layout slotu (21 hodnot × 22 + 30 globál = 492)
| Skupina | Hodnoty |
|---|---|
| **Identita/stav (5)** | valid, on_pitch, is_standing, is_prone_or_stunned, has_ball |
| **Pozice (3)** | x_norm, y_norm, dist_to_endzone |
| **Staty (4)** | MA/9, ST/5, AG/5, AV/10 |
| **Skills (5)** | Block, Dodge, Guard, Wrestle, SureHands_or_StripBall |
| **Taktika předpoč. (4)** | enemy_tz_count, can_be_blitzed (BFS), in_carrier_diagonal, net_st_for_block |

`21×22 = 462` + **30 globálních** (score/diff, turn progress, rerolls, weather, possession, KO/INJ počty, oneTurnTDVuln, screen, bias…) = **492**. Číslo je orientační, ne dogma (7 skillů/slot → ~506).

---

## 2) C++ Engine Analyst

### Q1 — Skills enum (OVĚŘENO)
- `SkillName : uint8_t`, hodnoty 0–73, `SKILL_COUNT=74` (`enums.h:139`). Uloženo v **`std::bitset<128>`** (`player.h:11`). `p.hasSkill(X)` **funguje přímo, bez změn v C++.**
- Ověřené indexy: Block=0, Catch=1, Dodge=2, Frenzy=3, Guard=4, MightyBlow=5, Pass=6, SideStep=7, StandFirm=8, StripBall=9, SureHands=10, Tackle=11, SureFeet=12, Dauntless=18, JumpUp=29, Sprint=30, DirtyPlayer=32, Wrestle=36, Claw=37, Leap=42. Všech 20 dotazovaných **existuje** — brief je měl správně.

### Q2 — BFS flood-fill (EXISTUJE, s výhradou)
- `canReachAdjacentTo` (`pathfinder.cpp:20`) je plnohodnotný BFS (26×15=390 polí), respektuje MA+GFI (Sprint→3), stand-up cost (PRONE −3, JumpUp zdarma), obsazená pole.
- **KRITICKÁ VÝHRADA: TZ/dodge se NEzapočítává** — uniformní cena 1/pole, `PathNode::dodged` je mrtvý kód. Pro „reach respektující TZ" nutné rozšířit: nová funkce `canReachSquare()` vracející `{reachable, reachableRisky, minCost, dodgeCount}` (Dijkstra-lite, preferuj cesty bez dodge).
- **Jeden flood-fill na hráče (22 celkem), sdílený přes featury:** reach_carrier ✓, reach_loose_ball ✓, escape_routes (vedlejší produkt: pole v BFS s dodgeCount==0, dist 1) ✓, can_score (test, zda dosažené pole leží v endzóně) ✓. **NE jeden BFS na featuru.**

### Q3 — KO/injured (OVĚŘENO)
- `PlayerState`: STANDING=0, PRONE=1, STUNNED=2, KO=3, INJURED=4, DEAD=5, EJECTED=6, OFF_PITCH=7 (`enums.h:15`). `isOnPitch` = STANDING|PRONE|STUNNED.
- Fixní slot dle indexu 0–21 (`game_state.h:19`: 0–10 home, 11–21 away). `on_pitch` flag jako první feature + **zvlášť `is_ko` flag** (KO se vrací o poločase, INJURED ne) + zero-fill geometrie; statické atributy (MA/ST/AG/AV/skills) ponechat.

### Q4 — Rozsah změn 70→492 (~12 míst, NE jedno)
- **Povinná logika:** `feature_extractor.cpp:62-605` `extractFeatures()` kompletní přepis; `feature_extractor.h:8` konstanta.
- **Automaticky dědí (rekompilace):** `game_simulator.h:40`, `policies.h:25`, `policy_network.h:11`, `mcts.cpp:207,256,304`, `macro_mcts.cpp:190,497,580`, `policies.cpp:148`, `bb_module.cpp:171,181,314,316`.
- **Skrytý hardcode:** `policy_network.cpp:44` `float hidden[64]` + `:45` `min(H,64)` — hidden napevno max 64.
- **Python — druhá nezávislá definice:** `features.py:9` `NUM_FEATURES=70` + celá `extract_features()` (ř. 12–420) musí zrcadlit C++ → jinak train/inference mismatch. **Paritní test C++↔Python je kritický gate.**
- **Váhy:** `weights_*.json` formát je velikostně agnostický (`n_features` + odvození z `W1.size()`, `value_function.cpp:67`) → **formát měnit netřeba**, ale staré 70-váhy nekompatibilní (`evaluate()` ořezává na `min`, `value_function.cpp:37`) → nutný re-train/warm-start expanze W1.

### Q5 — Výkon
- Hotspoty: `replayToNode` (`mcts.cpp:316`, přehrává všechny akce na cestě — dominantní cena dnes), `extractFeatures`, `countTacklezones` (`helpers.cpp:6`).
- Bez optimalizace by per-player+BFS zpomalil MCTS **3–10×** (desítky–stovky ms/tah navíc).
- **Precompute:** (1) `int occupancy[390]` na začátku extrakce → `getPlayerAtPosition` O(1); (2) jeden flood-fill/hráč; (3) `int8_t tzMap[390]` → `countTacklezones` O(1); (4) cache featur per MCTS uzel; (5) early-cutoff v BFS (už je).

### Q6 — Náročnost (~11–14 člověko-dnů)
| # | Krok | Odhad |
|---|---|---|
| 1 | Precompute (`occupancy[]`, `tzMap[]`) | 1 d |
| 2 | Pathfinder `canReachSquare` + TZ/dodge + testy | 2 d |
| 3 | Per-player slot layout (bez BFS featur) | 2–3 d |
| 4 | Reachability featury přes jeden flood-fill | 2 d |
| 5 | Zrcadlit do Pythonu + paritní test C++↔Python | 2–3 d |
| 6 | Síť: dynamický hidden buffer + warm-start W1 | 1 d |
| 7 | Výkonové ladění MCTS | 1–2 d |
| 8 | Re-train + benchmark vs 86.7% | (běh) |

Pořadí pro průběžné testování: 1→2→3→**5 (paritní gate)**→4→6→7→8.

---

## 3) Training Loop Expert

### Reálné hodnoty (ověřeno)
`EPOCHS=16`, `GAMES_PER_EPOCH=40`, `MCTS_ITERATIONS=100`, `LR=0.0003`, `VF_BLEND=0.0`, `BENCHMARK_MATCHES=400`, `GATING_MATCHES=400`, `BM_DROP_LIMIT=0.05`, `BM_FLOOR=0.77`, `ANTI_REGRESSION=0.51`, `HIDDEN_SIZE=64`, `WORKERS=12`, `mc_shaped`. **Jeden loop ≈ 2.5–3 h** (16 epoch self-play = 103 min, ~9.7 s/hra).

### Q1 — Cold start hyperparametry
| Parametr | Běžně | Cold start | Vrátit |
|---|---|---|---|
| LR | 0.0003 | **0.0008** (1–4), **0.0005** (5–10) | 0.0003 (~loop 10) |
| BM_FLOOR | 0.77 | **0.0** (1–6), pak 0.55 | 0.77 |
| ANTI_REGRESSION | 0.51 | **vypnout gate** (1–3) | EMA (viz Q4) |
| GAMES_PER_EPOCH | 40 | **80** (1–8) | 40 |
| HIDDEN_SIZE | 64 | **128** (expanze na 256 přes warm_start) | — |
| EPSILON_START | 0.35 | **0.45** | 0.35 |

`BM_FLOOR=0.77` je tvrdá zeď — čerstvá síť by NIKDY nepromovala, nutno vypnout.

### Q2 — Konvergence
- ~492 spolehlivě >60 % vs random: **8–14 loopů (~3–6 dní)**. Plné dohnání ~85 %: **30–50 loopů (~2–4 týdny)**.
- První loopy budou vypadat jako selhání (nil_nil >0.6). **Nesledovat jen win_rate** — sledovat `mean_abs_vf` a avg_score_diff jako včasnější signál učení.

### Q3 — TD(λ): implementovaný, NEAKTIVNÍ
- `train_td_lambda` existuje (`trainer.py:183-225` lin., `466-530` neural), dispatch `training_loop.py:764`, ale `run_iteration.py:158` natvrdo `mc_shaped`.
- **Doporučení: lambda=0.85** (ne 0.9 — cold V(s) je hlučná, vyšší λ = vyšší variance). **Nuance: první 2–3 cold loopy nechat `mc_shaped`**, pak přepnout. Lambda se s velikostí sítě nemění.
- **Konkrétní edit:** `run_iteration.py:158` `mc_shaped` → `td_lambda` + `--lambda=0.85`.

### Q4 — Gating: téměř čistý šum
- Reálně **200 benchmark her** (`//2`, `run_iteration.py:171`), 400 gate (chess).
- 4 reálné iterace: chess 0.485–0.515, **95% CI ±0.026–0.029**, **66–72 % remíz** → jen ~130/400 decisive. `ANTI_REGRESSION=0.51` je statisticky nerozlišitelné od 0.50 → promote/reject je hod mincí.
- Pro +3 % chess signál: **>1000 her** (oprava: předchozí "≥600" podhodnoceno).
- **Robustnější kritérium:** EMA benchmark (`0.7*prev+0.3*new`) jako primární promote signál + chess-gate jen jako decisive-only winrate `W/(W+L)` s prahem 0.45.

### Q5 — Shaping weights pro ~492
Ponechat jen **globální/terminální** signály, odebrat vše vázané na pozici/hráče (duplikoval by per-player gradient):
- **PONECHAT:** my_score (3.0), opp_score (−3.0), having_ball (0.5), ball_on_ground (−0.8), carrier_can_score (0.8, zpočátku).
- **PŘEKLOPIT do slotů:** carrier_dist_to_td, carrier_near_endzone, opp_scoring_threat, carrier_tz_count, carrier_blitzable.
- **ODEBRAT:** loose_ball_proximity, my/opp injured, stall_incentive.
- Startovní sada: **{my_score, opp_score, having_ball, ball_on_ground, carrier_can_score}**.

### Q6 — Riziko cold startu vs inkrementální
- Rizika 492: delší konvergence (měsíc bez modelu), ztráta zralého 86 % modelu (`warm_start_expand` NEumí změnu počtu vstupů — opravdu cold od nuly), zpomalení loopu 3 h→~5 h, gate šum.
- **VERDIKT: nejdřív inkrementální 70→~150** (carrier slot + 5 klíčových hráčů, ~16 features/hráč) — zachová warm start přes `_align_features` (ř. 532–541), riziko regrese nízké, konvergence ve dnech. Pokud prolomí 86 % strop, validuje hypotézu levně. Plný 492 až po důkazu — jinak měsíc cold startu riskuje, že problém byl v gate šumu/nil_nil, ne ve features.
- Mitigace 492: MCTS_ITERATIONS dočasně 60, paralelně držet starý best jako fallback.

---

## 4) Game Domain Expert

### Q1 — Priorita skills (8 bitů + agregát)
- **MUST (6):** Block, Dodge, Guard, **Tackle**, Wrestle, SureHands. *(ZMĚNA: Tackle z HIGH do MUST — engine ho používá 2× ve `scoreFace`/Stumble, bez něj nelze ocenit blok na Dodge nosiče.)*
- **HIGH (6):** StripBall, MightyBlow, StandFirm, SideStep, Frenzy, Dauntless.
- **NICE:** Claw (↓ — sám nic nedělá, jen s MightyBlow vs vysoké AV), Leap, Pass, Catch.
- **LOW (vynechat/agregát):** JumpUp, Sprint, SureFeet, BreakTackle… → Sprint+SureFeet+BreakTackle nekódovat jako bity, ale jako odvozený **`effective_move` skalár** (ušetří 3 bity).

### Q2 — Taktické per-player signály
| Koncept | Feature(y) |
|---|---|
| Cage corner? | `in_carrier_diagonal` (Chebyshev==1 & Δx≠0 & Δy≠0) |
| Nosič chráněn? | `cage_corners_filled` (0–4), `carrier_blitzable` |
| Cage breakdown | per roh: `corner_eff_st`, `corner_has_guard`, `corner_has_standfirm` + `min_dist_my_blitzer_to_weakest_corner` |
| Ball blitz→pickup | `can_reach_carrier` (BFS), `has_wrestle`; `nearest_free_surehands_dist_to_ball`, `surehands_collector_free` |
| L-pin / sideline | `adjacent_to_sideline` (y∈{0,14}), `escape_routes_count`, `my_adjacent_to_pinned_target` |
| Poziční riziko | `n_opponents_can_block_me`, `worst_block_dice_against_me`; prone/stunned se nepočítá **kromě JumpUp** |
| Pass/handoff target | `is_free_receiver` (tz==0), `dist_to_endzone`, `has_clear_pass_lane` (+ PassBlock do 3 čtverců) |

### Q3 — Asistence / block dice (algoritmus briefu OVĚŘEN správný)
- `countAssists` (`helpers.cpp:125-149`): Guard vždy asistuje (i v TZ), non-Guard jen při `countTacklezones==0`. Block dice prahy `attST > 2*defST`→3, `>defST`→2, rovnost→1 (`helpers.cpp:151`) — BB2016 ✓.
- **ZMĚNA: `net_st_for_block` musí být per pár (útočník→obránce)**, ne per hráč, a výstup rovnou jako **block-dice kategorie ∈ {−3…+3}** (znaménko=kdo vybírá), ne raw net — kvůli nelineárnímu prahu 2×ST.
- Pozor: Guard v TZ (častá chyba), Dauntless (D6 test na raw ST, `block_handler.cpp:249`) — počítat ve worst-case bez Dauntlessu. StandFirm/SideStep → cage/L-pin signály, ne net_st.

### Q4 — Rasa jako feature?
- **One-hot rasy: NE** — odvoditelné z per-player MA/ST/AG/AV+skills; riskuje overfitting na 4 trénované rasy.
- **`opp_roster_speed` (SLOW/MIXED/FAST): ANO** *(ZMĚNA z „nekritické")* — engine ji **už počítá** (`classifyRosterSpeed`, `roster.cpp:471-491`), 1 feature zdarma, globální strategický prior pro cold-start konvergenci. Patří do globálního kontextu.
- **`is_fast_runner` (MA≥7+Dodge): ANO**, jako odvozený bit ve slotu — rychlí runneři se chovají opačně (schovávají se pro sprint).

### Q5 — nil_nil herně
- Skórovat = 4–8 krokové sekvence, kde mezikroky mají lokálně ≤0 hodnotu, „nehýbat se" = nulový risk. Agregát skrývá konkrétní příležitost.
- Features rozbíjející equilibrium: `is_free_receiver`+`dist_to_endzone`+`has_clear_pass_lane`; `corner_eff_st`+blitzer dist; `can_reach_carrier`+`block_dice_vs_carrier`; `carrier_blitzable`.
- **Nutné i mimo features:** draw penalty (remíza je „bezpečná"), TD(λ≈0.9), potential-based progress shaping. Features samy → ~10–15 %, ne nula.

### Q6 — BB2016 pasti (ověřeno proti enginu)
| Tvrzení | Verdikt |
|---|---|
| Passing přes AG, žádná PA | ✓ `pass_handler.cpp:196` `7-agility` |
| Intercept −2, +TZ, žádný Deflect | ✓ `pass_handler.cpp:68,73` |
| **Sure Hands kontruje Strip Ball** | ✗→✓ **PŮVODNĚ neplatilo, OPRAVENO 2026-06-10** — přidána kontrola `!def.hasSkill(SureHands)` (`block_handler.cpp:460`) + test `StripBallNegatedBySureHands`. Engine nyní v souladu s BB2016. Feature `surehands_counters_stripball` je teď validní. |
| **Guard u foulů** | ✓ **engine ZApočítává** (`foul_handler.cpp:19` volá stejné `countAssists`) — opačně než BB2016 pravidla |
| MightyBlow+Claw jen útok | ✓ `block_handler.cpp:481` |
| Wrestle>Block (Both Down) | ✓ s nuancí: hráč s Block+Wrestle použije Block; Wrestle = oba prone, no turnover |
| Tackle ruší Dodge i na Stumble | ✓ `block_handler.cpp:15,401` |

**Klíč:** features musí kopírovat **engine, ne pravidla BB2016** — Sure Hands vs StripBall už OPRAVENO (2026-06-10), zbývá jedna odchylka: Guard u foulů.

---

## Konsolidované doporučení k implementaci

**Pořadí (jedna změna naráz, ověřit efekt — viz uživatelská preference):**

1. **Rychlé výhry bez per-player (lze hned, testovatelné samostatně):**
   - Zapnout **TD(λ=0.85)** — `run_iteration.py:158`.
   - Přidat **draw penalty** −0.1…−0.2 — `trainer.py:82-89`.
   - Osekat **stallIncentive** při remíze — `feature_extractor.cpp:236-242`.
   - Přejít na **EMA + decisive-only** gating kritérium.
2. **Levný test hypotézy: inkrementální 70→~150** (carrier slot + 5 klíčových hráčů), warm start zachován.
3. **Pokud (2) prolomí 86 %:** plný per-player ~492 dle plánu C++ Analysta (11–14 d), s precompute infrastrukturou a paritním C++↔Python testem jako gate.

**Pozor před implementací:**
- BFS musí respektovat TZ/dodge (dnešní `dodged` je mrtvý kód).
- Python `features.py` má vlastní `NUM_FEATURES` — synchronizovat.
- `policy_network.cpp:44` hidden[64] strop — rozšířit.
- Features kopírují engine, ne pravidla. Sure Hands/StripBall už sjednoceno (2026-06-10); zbývá Guard u foulů (engine započítává, pravidla ne).

---

*Generováno re-během Team 1 na Claude Opus 4.8. Agent IDs pro pokračování: ML/RL `a72bc845d70b95c1c`, C++ `a29a5f3a25651d301`, Training `aa1b7b1b845c36eae`, Domain `a6a003a0b231a2edb`.*
