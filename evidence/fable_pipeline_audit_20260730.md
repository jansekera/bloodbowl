# Audit tréninkové pipeline — "přerušení učení" (Fable, 30.07.2026)

Metoda: přímé trasování kódu, POUZE čtení. Všechna čísla řádků odpovídají stavu working tree 30.07.2026.
Klíčové soubory: `run_iteration.py`, `python/blood_bowl/training_loop.py`, `python/blood_bowl/trainer.py`,
`python/blood_bowl/replay_buffer.py`, `engine/src/macro_mcts.cpp`, `engine/python/bb_module.cpp`.

Seřazeno podle závažnosti.

---

## N1 — BUG (kritický): Naučený obsah policy hlavy se NIKDE v pipeline nepoužívá; akumuluje se přes REJECTED běhy jako jediný přeživší stav

**Potvrzení centrálního podezření (a) — celý řetězec:**

1. `run_iteration.py:512` — `_stash_policy(az_train_path, policy_cache_path)` běží **nepodmíněně** hned po
   `subprocess.run(train_cli)`, PŘED selection i gate. Uloží policy hlavu finální epochy do `weights_policy.json`.
2. `run_iteration.py:474` — `_carry_over_policy(az_train_path, best_path, policy_cache_path)` na začátku další
   iterace: value hlava se bere z frozen best (`:472` copy best→az_train), ale policy hlava se **přepíše** obsahem
   `weights_policy.json` (`:341-376`). → Policy hlava akumuluje napříč iteracemi bez ohledu na verdikt gate.
3. Gate/selection/benchmark předávají `policy_weights_path=gate_policy_path` (`:544, :549, :579, :624-625`), ale
   **nikdy nepředávají `policy_blend`** → binding default `0.0f` (`bb_module.cpp:574`).
4. `macro_mcts.cpp:358` — naučený obsah policy sítě se čte JEN při `config_.policyBlend > 0.0f`.
   `macro_mcts.cpp:384` — heuristické prior floors se aktivují už přítomností `config_.policy` (nezávisle na obsahu).
   Komentář na `run_iteration.py:540-543` to říká přesně: "policy_blend stays 0 (unset) so the net's learned
   content is never evaluated".

**Rozšíření (b) — trasování VŠECH hodnot policy_blend:**

| Cesta | policy_blend | Naučený obsah policy použit? |
|---|---|---|
| Self-play trénink | `POLICY_BLEND=0.0` default (`run_iteration.py:73`), předáno do train_cli (`:500`); **ALE** `training_loop.py:324-327`: `epoch <= imitation_epochs → epoch_blend=0.0`, a `IMITATION_EPOCHS=16 == EPOCHS=16` → **blend=0 ve VŠECH epochách, i kdyby BB_POLICY_BLEND>0** | NE (past: env proměnná je mrtvá, dokud `BB_IMITATION_EPOCHS < BB_EPOCHS`) |
| Selection H2H | `_gate_game` bez policy_blend → 0.0f | NE (jen prior floors) |
| Anti-regression gate | `_gate_game` bez policy_blend → 0.0f | NE (jen prior floors) |
| Benchmark vs random (run_iteration) | `_benchmark_game` bez policy_blend → 0.0f | NE |
| Interní benchmark training_loop (`:625`) | `policy_blend=epoch_blend` = 0.0 (epocha 16 ≤ 16) | NE |

**Odpověď (c):** 100 % trénovaného signálu policy hlavy je gate-neviditelných — a nejen to: je neviditelný pro
CELOU pipeline (nikdy neovlivní jediné rozhodnutí v žádné hře). Je to strukturálně stejný typ jako GATE_VF_BLEND=0
z 21.07, ale horší: tam se value alespoň používala v self-play cílech; policy hlava se trénuje (policy_loss ~2.0,
top1 ~39-44 % v epoch_metrics.csv), přenáší, akumuluje — a nikdy nehraje. Value hlava je na tom jen o málo lépe:
jediné místo v celé pipeline, kde její naučený obsah ovlivňuje rozhodování, je selection H2H + gate při
`GATE_VF_BLEND=0.15` (15% blend v leaf evaluaci, `macro_mcts.cpp:843-849`). Self-play generování dat: 0 (VF_BLEND=0).
Benchmark: 0 (viz N2).

Navíc: policy imituje MCTS rozhodnutí ze search, který běží s vf_blend=0 a policy_blend=0 → **imituje čistou
heuristiku**, ne naučenou value. I při zapnutí blendu by konvergovala k heuristickému stropu.

**Detaily navíc:**
- `weights_policy.json` není v gitu (žádný commit) ani v `_git_push` files (`run_iteration.py:812-816`) — jediný
  akumulátor učení napříč iteracemi existuje jen lokálně, bez zálohy a bez verze.
- Nekonzistence: při promote `train_best` dostane šampion policy hlavu z prostřední epochy, ale příští iterace ji
  přepíše stashem z **az_train finální epochy** (`:512` stashuje vždy az_train) — nezáleží, dokud blend=0.

**Odpověď (d) — co by vyžadoval férový test neural policy:**
1. `BB_IMITATION_EPOCHS < BB_EPOCHS`, aby aspoň část epoch běžela s `epoch_blend = POLICY_BLEND > 0` (self-play).
2. Pro měření (gate/H2H): `_gate_game` musí předat `policy_blend > 0` — dnes parametr vůbec neposílá.
3. **Blokátor v bindingu:** `simulate_game_logged` má jen JEDEN `policy_weights_path` — `bb_module.cpp:484-487`
   načte jediný `policyNet` a `makePolicy` (`:509-511`) ho dá OBĚMA stranám. `away_weights_path` existuje jen pro
   value. → H2H "kandidát s natrénovanou policy vs frozen bez ní / s jinou" je dnes **technicky nemožný** bez
   rozšíření bindingu o `away_policy_weights_path` (+ per-side blend).
4. Side-swap + dost decisive her (stejná mechanika jako gate) + předregistrovaný práh.

**Návrh fixu:** (i) rozšířit binding o per-side policy path/blend; (ii) přidat `GATE_POLICY_BLEND` analogicky ke
`GATE_VF_BLEND`; (iii) stash policy až PO verdiktu, nebo verzovat `weights_policy.json` s metadaty (z jaké iterace,
verdikt); (iv) do té doby explicitně zdokumentovat, že "policy learning" je dead-end větev bez odbytu.

---

## N2 — BUG (kritický, nový nález): Benchmark vs random je dodnes null-test — kandidátovy váhy vůbec nečte

`run_iteration.py:548` — `_run_benchmark` předává `VF_BLEND` (=0.0, `:39`), nikoli `GATE_VF_BLEND`. Komentář
`:47-48` to dělá vědomě ("_run_benchmark zůstávají beze změny (0.0)"). Při `vfBlend<=0` engine value síť přeskočí
(`macro_mcts.cpp:843`), `policy_blend`=0 (N1) → **benchmark skóre je zcela nezávislé na obsahu měřených vah**.
Je to tentýž mechanismus, který byl 21.07 opraven v gate (`diag_null_weights.py`), ale v benchmarku žije dál.

**Dopad na měření:** `new_bm` je šum + efekt MCTS budgetu, a přitom řídí:
- tier volbu k (1.0/1.5/2.0σ) pro HtH práh (`:695-705`) — šum benchmarku ohýbá laťku gate,
- HARD-REJECT (`:707-710`) a BM_FLOOR (`:719-721`),
- selection reporting a `all_time_best_bm`.
Gate_history to potvrzuje: az_train vs train_best benchmarky 0.96/0.96, 0.985/0.995, 0.98/1.0 — rozdíly uvnitř
šumu identických heuristik. ("Benchmark 91.5→99.5 %" u item1 byl výjimka — item1 běžel s BB_VF_BLEND>0.)

**Návrh fixu:** benchmark buď (a) měřit s GATE_VF_BLEND (stejná parita jako gate), nebo (b) přiznat mu roli
"sanity heuristického enginu" a vyřadit ho z tier logiky (k fixní), protože dnes tam vnáší jen šum.

---

## N3 — BUG (nový nález, "časová kapsle"): git push každou iteraci resetuje replay_buffer.pkl na snapshot z ÚNORA 2026

Mechanismus:
1. `replay_buffer.pkl` je trackovaný od **Initial commitu 4ff6f6d (20.02.2026)** a od té doby nikdy necommitnutý
   znovu (`git log -- replay_buffer.pkl`). Committed verze: **5000 transakcí, 70-featurové, mc_return=None**
   (ověřeno unpicklem `HEAD:replay_buffer.pkl`).
2. `_git_push` dělá `git reset --hard origin/main` (`run_iteration.py:793`) a re-apply provádí jen pro váhy, meta a
   epoch_metrics.csv (`:795-810, :812-816`) — replay_buffer.pkl **není** v seznamu → po každém push se vrátí na
   únorovou verzi.
3. `training_loop.py:209-211` ho na začátku dalšího běhu načte → prvních ~8 epoch se 64 replay vzorků/epochu bere
   z ~80 % z her z února (deque 10000 se čerstvými ~640 transakcemi/epochu promývá pomalu).
4. Dimenzní nesoulad 70 vs 73 featur se tiše zamaskuje zero-paddingem (`trainer.py:416-424`); únorové transakce
   navíc nesou starou reward sémantiku (mc_return=None → fallback na winner-only reward, `training_loop.py:819-821`
   / `:835`).

Ověřeno na živém souboru: aktuální `replay_buffer.pkl` = 5000 únorových + 1286 čerstvých transakcí.

**Odpovědi na auditní otázky #2:** ANO, mísí éry (únor 2026 engine — před VŠEMI opravami včetně ball-stuck,
item14, item7 — s aktuálním); reset se neděje NIKDY (naopak se stará data cyklicky re-injektují); vzorkování je
uniform random 64 transakcí/epochu (`training_loop.py:393-397`, `replay_buffer.py:120-123`).

**Dopad:** omezený objemem (64 vs tisíce updatů z fresh logů ≈ ~2-3 % updatů/epochu), ale je to permanentní
kontaminace value tréninku 5 měsíců starými hrami jiného enginu. **Fix:** `git rm --cached replay_buffer.pkl`
(+ .gitignore), a resetovat buffer při změně enginu/featur (verzovací pole v pkl).

---

## N4 — BUG/PODEZŘENÍ (champion lineage): současný frozen šampion NENÍ gate-promoted model

- Poslední **promoted** commit šampiona: `842c200` (16.06., n_features=70).
- Obsah dnešního `weights_best.json` == commit `99a0d1c` (22.07., popisek **"rejected"**), kde se šampion změnil na
  **úplně jiný 73-featurový net** — n_features, value_W1/b1/W2/b2 i policy_* vše jiné; NENÍ to zero-padding
  migrace (ověřeno numericky: prvních 70 sloupců W1 se neshoduje).
- NUM_FEATURES 70→73 se změnilo 25.06. (`8658768`); někde mezi 25.06. a 22.07. byl šampion přepsán mimo gate
  (vzor = standalone-guard korupce známá z 29.07.: trénink/skript píšící přímo do weights_best.json) a `_git_push`
  ho pak v rejected commitu zafixoval jako novou baseline.

**Dopad na měření:** všechny gate verdikty od 22.07. (43-50 % HtH, 4/4 REJECTED, item1…) měří kandidáty proti
anchoru, který nikdy neprošel gate certifikací. Nevíme, jestli je to silný soupeř, nebo jen jiný. **Fix:** dohledat
původ 73-featurového netu (proces z ~29.06.?), buď ho zpětně certifikovat (benchmark+HtH proti poslednímu
certifikovanému, s vědomím feature nekompatibility), nebo vědomě prohlásit za novou baseline se záznamem.
Standalone guard (už v working tree, `training_loop.py:666-674`) konečně commitnout.

---

## N5 — Strukturální (potvrzeno): reset-on-reject zahazuje veškerý value pokrok; přežívá jen nepoužívaná policy

- `run_iteration.py:472` — každá iterace: copy best→az_train → **value hlava startuje vždy ze stejného frozen
  šampiona** (beze změny obsahu od 22.07., viz N4).
- `run_iteration.py:741` — reject: copy frozen→best. Nic z 16 epoch se nezachová…
- …s jedinou výjimkou: policy hlava (`:474+512`) — která ale nehraje (N1).

Šestnáct epoch value tréninku musí od nuly vyprodukovat ≥52.6 % decisive HtH (372 dec.), jinak se zisk zahodí.
Pokud reálné zlepšení za 16 epoch je ~1-2 pp, gate ho správně-statisticky, ale strukturálně-fatálně zamítne
pokaždé → hypotéza "zahazujeme malé reálné zisky" má v kódu přesně tento mechanismus. **Návrh:** akumulační
experiment — N iterací bez resetu (az_train pokračuje), gate až kumulativně; nebo práh per-iterace snížit a
jistit dlouhodobým trendem (gate_history už existuje).

## N6 — PODEZŘENÍ: gate ignoruje remízy (decisive-only) při draw rate 29-59 %

`run_iteration.py:653-654`: `chess_score = wins / decisive`; remízy zahozeny (`:650-652` zdůvodnění).
Gate_history: draws 39 %, 38 %, 29 %. Zlepšení směru "prohry→remízy" (typické pro malý pokrok v obraně) je
gate-neviditelné; σ z decisive (`:693`) → vysoký draw rate zmenšuje N a zvyšuje práh. Alternativa: skórovat
W+0.5D/N s prahem odvozeným z permutačního testu, nebo aspoň reportovat L→D posun (dnes viditelný jen v logu).

## N7 — PODEZŘENÍ: BB_MCTS=400 — konzistentní uvnitř běhu, nekomparabilní napříč běhy (latentní trvalý HARD-REJECT)

Uvnitř běhu OK: `MCTS_ITERATIONS` jde shodně do self-play (`:488`), benchmarku (`:548`), selection (`:579`) i gate
(`:624`), watchdog škáluje timeout (`:222`). ALE cross-run: meta ukládá `benchmark_mcts_iterations` (`:737`), gate
ho při čtení **ignoruje** (`:429-438`) — poslední gate_history záznam: mcts=400, bm=1.0 vs all_time_best 0.915@100
→ "benchmark zlepšen" (zde shodou okolností mírnější laťka). Nebezpečí: promote při 400 zapíše all_time_best=1.0
→ všechny budoucí runy na 100 (bm~0.92 < 1.0−0.05) skončí **trvalým HARD-REJECT** (`:707`). `training_loop.py:679`
tuto ochranu (mcts_mismatch) má, `run_iteration.py` ne. Fix: při čtení meta porovnat mcts a při nesouladu
resetovat benchmark baseline (obdoba TV resetu `:440-446`).

## N8 — Metrika: weight_norm_change neměří pohyb vah (odpověď na #7)

`training_loop.py:380, 403-404` + `_weight_norm` (`:913-921`): metrika je **‖W‖_after − ‖W‖_before** — rozdíl
skalárních norem, NE ‖ΔW‖. Přitom `grad_norm` ~3.7-4.3 na update (`trainer.py:549-556`, mean per-update) × LR
0.0003 → krok ~0.0012/update × tisíce updatů/epochu → váhy urazí řádově jednotky. Δ‖W‖≈±0.0003-0.0004 tedy
znamená "norma se nemění", ne "váhy se nehýbou" — pohyb je z velké části vzájemně se rušící nebo tangenciální
(po sféře konstantní normy). Metrika neumí odlišit plateau od velkého kruhového driftu. **Fix:** logovat
`np.linalg.norm(W_after − W_before)` (per-epoch snapshot), ideálně i cos-similarity update směrů.

## N9 — PODEZŘENÍ: selection krok je mince mezi dvěma náhodně určenými kandidáty (odpověď na #3)

- H2H 150 her (`:577-601`), ~60 remíz → σ≈5 pp na decisive; gate_history: 49:46, 44:47, 41:46 — vše uvnitř šumu.
  Tie-break: `tb_w > az_w` (`:603`) → při rovnosti vyhrává az_train. Poražený kandidát se zahazuje celý.
- Horší: sám výběr `weights_train_best` (`training_loop.py:494-503`) stojí na self-play win rate proti frozen — ale
  s VF_BLEND=0 engine váhy stran vůbec nečte (obě strany = identická heuristika, liší se jen epsilonem home strany)
  → per-epoch WR je šum → train_best je fakticky náhodně vybraná epocha.
**Fix:** buď selection zrušit (kandidát = finální epocha) a ušetřených 150+400 her přesunout do gate N, nebo
vybírat podle interního H2H s vf_blend>0.

## N10 — Kontext (známé, doplněna file:line): self-play generuje data bez kontrastu

`training_loop.py:311-318` + `cpp_runner.py` → `simulate_game_logged` s vf_blend=0, policy_blend=0: obě strany
self-play (az_train vs frozen best) hrají TOUTÉŽ heuristiku; "frozen opponent" mechanismus (`:167-176`) je
fakticky mirror se dvěma epsilony. Trénovaná data tedy nenesou informaci o síle aktuálních vah — známý nález
(value-blind self-play), zde jen ukotven přesnými řádky pro souvislost s N1/N2.

---

## Souhrnný obraz

Jediné místo v celé pipeline, kde naučený obsah JAKÉKOLI sítě ovlivňuje rozhodování, je selection H2H + gate
s GATE_VF_BLEND=0.15 (15% leaf blend value hlavy). Policy hlava nehraje nikde (N1), benchmark neměří váhy (N2),
self-play data jsou value-blind (N10), value pokrok se při rejectu resetuje (N5) a remízy se nepočítají (N6).
Anchor, proti němuž se vše měří, není gate-certifikovaný (N4), a value trénink je průběžně kontaminován únorovými
daty (N3). Otázka "učíme se?" je za těchto podmínek měřitelná jen skrz 15% blend v HtH — vše ostatní je šum
nebo mrtvá větev.
