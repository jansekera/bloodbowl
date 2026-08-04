# F0 + F1 + A/B harness — cage advance balík (2026-08-03, Fable)

**Stav: IMPLEMENTOVÁNO + testy zelené (487/487); plná validace PŘIPRAVENA, NESPUŠTĚNA**
(worktree `agent-ac2fc491e08aef722`, NEpushnuto, NEmergovnuto — commity jen ve worktree branchi)

## Exec summary

1. **F0 hotovo:** `python/blood_bowl/fairtest_schedule.py` — plný round-robin rozvrh včetně mirrorů
   (náhrada period-5 cyklu) + párové per-race vyhodnocení within-matchup (§3b vzor); 18 engine-free testů.
2. **F1 hotovo:** `MCTSConfig::cageAdvance` (DEFAULT OFF) — celotahový staged plán „posuň klec o 1–2 pole"
   (rohy napřed, carrier poslední), vzor stagedPickupPlanner; **obchází prior floory úplně** (žádná nová
   rodina priorů — lekce item7 se neopakuje). Všechny 3 závazné constrainty uživatele implementovány
   (tempo = výpočet; výběr rohů spolehlivost > Guard/StandFirm, nega-traity genericky přes skilly;
   role-rezervační API). 16 nových C++ testů, celá suita 487/487.
3. **Harness hotov + mini-smoke proběhl:** párový head-to-head A/B (cand = cageAdvance ON) na
   dwarf–skaven / dwarf–we / dwarf mirror / orc–skaven kontrole, s **attrition metrikou KO/INJ/DEAD
   per tým z finálního GameState** (vlastní kopie simulateGame smyčky — result objekt to nevydává).
4. Balík je stacked na item13 commity (cherry-pick z worktree agent-a978afb18db853696) — CAGE_ADVANCE
   trigger používá `classifyTurnGoal` a sdílí staged-plán mašinerii.
5. Pre-registrované prahy (neměnit po spuštění): párový per-race Δ chess (cand-as-dwarf vs base-as-dwarf)
   ≥ **+3 pp** na každém dwarf matchupu; orc–skaven kontrola **|Δ| do 2 SE od 0**.

## Commity (worktree branch `worktree-agent-ac2fc491e08aef722`)

- `b8b2288`..`eac8b17` — cherry-pick item13 (staged planner + harness + cap + support gate; závislost)
- `0f87492` — F0: fairtest_schedule.py + 18 testů
- `f615e65` — F1: cage_advance.{h,cpp} + config gate + integrace MacroMCTSPolicy + 16 testů
- `5053cc0` — F1 harness diag_f1_cage_advance_harness.cpp
- (další viz `git log`, smoke + report)

## ČÁST 1 — F0 metodika

`python/blood_bowl/fairtest_schedule.py`:

- `matchup_for_seed(seed_idx, races)` — deterministický plný round-robin: každý blok N² po sobě jdoucích
  seedů pokryje všech N² uspořádaných dvojic přesně jednou (mirrory včetně, orientace vyvážené).
  Drop-in náhrada za `ra=RACES[i%5]; rb=RACES[(i+1)%5]` (diag_policy_confirm_20260731.py:48-49).
- `recommended_n_pairs(target)` — zaokrouhlí N na celé bloky (800 pro 5 ras vychází přesně, 32 bloků).
- `per_race_paired(rows)` — per-race čísla PÁROVĚ within-matchup proti baseline téže rasy: pro pár
  (cand_home/cand_away téhož seedu) dostane každá rasa matchupu jeden vzorek
  (chess cand-as-X, chess base-as-X); výstup per rasa × soupeř: cand/base decisive WR, Δ pp,
  párové t na chess skóre. Řádkový kontrakt = výstup game() ze stávajících fairtestů.
- Testy: `python/tests/test_fairtest_schedule.py` (18, engine-free; pokrytí bloku, mirrory,
  dwarf-potká-všechny, vyváženost, konfound test „globální WR neprosakuje", párové t, unpaired skip).

Spuštění: `cd python && /home/jan/claude/bloodbowl/venv/bin/python -m pytest tests/test_fairtest_schedule.py -q`

## ČÁST 2 — F1 CAGE_ADVANCE

Soubory: `engine/include/bb/cage_advance.h`, `engine/src/cage_advance.cpp`,
`engine/tests/test_cage_advance.cpp`; gate `MCTSConfig::cageAdvance=false` (mcts.h);
integrace `MacroMCTSPolicy::nextStagedMacro` (macro_mcts.cpp — goal ADVANCE_BALL větev
vedle item13 PICKUP_BALL větve, max 1 staged plán per team-turn, sdílená deviace→search fallback).

Design (závazné constrainty):

1. **Tempo = výpočet.** `requiredPace = dist(carrier→EZ) / (min(turnsLeft,8) − 1 rezerva)`;
   turnsLeft = 9 − turnNumber = táž aritmetika jako `idealDist = turnsLeft*MA` v simulate()
   (žádný druhý pacing mechanismus, jen per-turn čtení téhož rozvrhu).
   `achievablePace = role-limit (největší step ∈{2,1}, pro který reálný corner assignment vyjde —
   reformace roh až step+2 polí, carrier bez GFI) − odpor (stojící soupeři v koridoru x+1..x+4,
   |Δy|≤2: 1–2 soupeři → −1 pole tempa, 3+ → −2)`. `required > achievable` ⇒ verdikt
   **TEMPO_INSUFFICIENT — žádný slepý posun**, tah řeší search jako dnes (může blitznout/pasnout).
   finalStep = clamp(ceil(requiredPace), 1, achievable) — nikdy nepředbíhat rozvrh (rezerva drží).
2. **Výběr rohů.** Sort klíč per slot: bez-GFI příchod > vzdálenost (Chebyshev) > **Manhattan tiebreak**
   (rovná trasa; nález ze smoke testů — cross-cage kandidát při stejné Chebyshev vzdálenosti reálně
   nedošel a rušil plán) > Guard > StandFirm. Tvrdá vyloučení GENERICKY přes skilly (nikdy jména
   ras/pozic): BoneHead/ReallyStupid/WildAnimal/TakeRoot (spolehlivost aktivace), SecretWeapon
   (Deathroller-typ — po drivu vyloučen), BallAndChain. NoHands ani Loner samy o sobě nevylučují
   (roh ruce nepotřebuje; Loner daní jen rerolly). V TV1200 to vylučuje human Ogra (BoneHead)
   a we Treemana (TakeRoot) navzdory jeho Guard+StandFirm — přesně testováno.
   GFI: všechny rohy bez GFI; max 1 roh na dist MA+1 — protože REPOSITION je zásadně dice-free
   (expandReposition nikdy neGFIuje), tenhle roh dojde o pole vedle a dovře příští tah =
   allowance je hodnotící pravidlo assignmentu, ne reálný GFI risk. Víc ⇒ otevřený roh;
   feasibility: filled ≥ 2 a filled ≥ min(built, 3) (nedegradovat stojící klec).
3. **Role-aware rozpočet.** `build(state, reservedPlayerIds)` — rezervovaní hráči nikdy draftováni
   (stojící rezervovaný na slotu se počítá jako tělo, ale nehýbe se). Kolize s item13 PICKUP plánem
   je strukturálně vyloučená (goal PICKUP_BALL vs ADVANCE_BALL + max 1 staged plán/turn); parametr
   je připravené API pro budoucí rezervace blitzer/asistenti/fauler (zatím prázdný seznam).

Bezpečnost: každé makro MC-probe na vyvíjející se projekci (PROBE_K=48, SAFE_PTO=0.02 — prahy
validované item10/item13); marked rohy (dodge při odchodu) ⇒ verdikt DICEY ⇒ fallback na search.
Prior floory: staged makra jdou mimo expand() → žádný zásah do floor rozpočtu.

Testy (16): trigger ×3 (default off, <2 rohy, volný míč), tempo ×5 (výpočet s rezervou,
pozadu za rozvrhem, poslední kolo, screen 3 soupeřů ⇒ insufficient, 1 marker ⇒ step 2→1),
plán ×1 (celá klec se posune, rohy napřed, carrier poslední, exekuce end-to-end 4/4 rohy),
výběr rohů ×4 (Guard tiebreak, Treeman nedraftován, generická eligibilita, GFI budget max 1),
rezervace ×1, policy integrace ×2 (rohy jednají před carrierem + gate-off identita se search).

Build + testy:
```bash
cd /home/jan/claude/bloodbowl/.claude/worktrees/agent-ac2fc491e08aef722
nice -n 15 cmake --build engine/build -j2
cd engine/build && ./bb_tests            # 487/487
./bb_tests --gtest_filter='CageAdvance*' # 16/16
```

## ČÁST 3 — párový A/B harness s attrition metrikou

`diag_f1_cage_advance_harness.cpp` (zkompilovaný `./diag_f1_cage_advance`):

- Head-to-head fairtest vzor (diag_policy_confirm): cand = produkční config + cageAdvance=true,
  base = produkční config; side-swapped páry, SEED_BASE=34M (disjunktní od 30M/31M).
  Config parity s šampion fairtestem: MCTS-100, vfBlend=0.15, policy net načtená s blend 0
  ⇒ prior floory AKTIVNÍ.
- Matchupy: 0 dwarf–skaven, 1 dwarf–wood-elf, 2 dwarf–dwarf mirror, 3 orc–skaven (kontrola).
- **Attrition/survival:** end-of-game KO/INJURED/DEAD/EJECTED per tým z finálního GameState
  (vlastní kopie simulateGame smyčky — veřejné API stav nevrací); agregace per rasa × „soupeř měl
  gate ON/OFF" ⇒ přímo odpovídá na otázku „kolik skavenů přežije zápas do dalšího kola turnaje"
  (survivors = 11 − INJ − DEAD; KO se do dalšího zápasu probírá).
- Per-game JSONL řádky (`diag_f1_cage_advance_rows.jsonl`) v F0 formátu ⇒ reanalyzovatelné
  `fairtest_schedule.per_race_paired`.

**Pre-registrované prahy (zapsáno PŘED spuštěním, neměnit):**

| Matchup | Metrika | Práh |
|---|---|---|
| dwarf–skaven, dwarf–we, dwarf mirror | párový Δ chess cand-as-dwarf (§3b) | **≥ +3 pp** (+0.03) |
| orc–skaven (kontrola) | párový Δ chess cand-as-orc | **|Δ| ≤ 2 SE** (žádná regrese) |
| attrition | KO/INJ/DEAD per rasa × opp-gate | deskriptivní, bez gate prahu |

Plný běh (AŽ PO SKONČENÍ TRÉNINKU, nikdy souběžně):
```bash
cd /home/jan/claude/bloodbowl/.claude/worktrees/agent-ac2fc491e08aef722
for m in 0 1 2 3; do
  nice -n 19 ./diag_f1_cage_advance . 400 $m > diag_f1_m${m}_$(date +%Y%m%d).log 2>&1 &
done
# 400 párů × 4 matchupy = 3200 her MCTS-100; řádově ~10-20 h/matchup na 1 jádře.
# SE při 400 párech ≈ 1.8-2.5 pp → práh +3 pp rozlišitelný.
```

### Mini-smoke (jediné spuštěné hry; 2 páry = 4 hry dwarf–skaven, nice -19)

```
=== matchup 0: dwarf (home race) vs skaven ===
SUMMARY matchup 0 (dwarf vs skaven), 2 pairs (4 games):
  cand overall: W3 D0 L1 decisiveWR=0.750 chess=0.7500
  PAIRED delta chess as dwarf: +0.5000 +- 0.3536 SE (~1.4 SE)

ATTRITION (per rasa, split podle gate soupeře):
race       opp gate        games   KO/g  INJ/g  DEAD/g  EJ/g  surv/11
dwarf      off                 2   0.00   0.00    0.00  0.00    11.00
dwarf      cageAdvance ON      2   0.00   0.50    0.00  0.50    10.50
skaven     off                 2   2.00   0.50    0.00  0.00    10.50
skaven     cageAdvance ON      2   0.50   0.00    0.00  0.00    11.00
```

N=4 = čistě sanity (šumové dno ±8–11 pp na N=150!): harness běží end-to-end, JSONL řádky
se zapisují, attrition tabulka se plní. ŽÁDNÝ závěr o efektu z tohohle čísla nedělat.
Pozn.: smoke běžel před přidáním počítadla „cage plans adopted/game" — plný běh ho už reportuje.

## Otevřené otázky

1. **Carrier po splnění plánu:** zbytek tahu patří search(), který může carrierem s zbylým MA ještě
   popojet (pozorováno v policy testu: plán 12→14, search dotáhl na 15 a rozvolnil diagonály).
   Vědomé MVP — zda to škodí, ukáže harness (metrika: konec-of-turn cage integrita by šla přidat).
2. **1-GFI roh dojde o pole vedle** (REPOSITION je dice-free) — dovře příští tah; pokud harness ukáže
   časté „skoro-rohy", zvážit reálné GFI v expandu (změna globálního REPOSITION chování = samostatná změna).
3. Rotace vs translace rohů vzniká emergentně z greedy assignmentu (dist+Manhattan) — bez explicitního
   „který roh je vpředu" plánování; DICEY fallback kryje selhání tras.
4. Resistance penalta (1–2 soupeři → −1, 3+ → −2) je heuristika bez kalibrace — kandidát na
   post-harness tuning, NE teď (jedna změna najednou).
5. Stacked na item13 (nemergované commity z agent-a978afb18db853696) — při merge řešit společně;
   item13 plná validace stále nespuštěná.

## Jak navázat

1. Po skončení tréninkové iterace spustit plný harness (viz výše), vyhodnotit proti pre-registrovaným
   prahům; při splnění → aktivace gate v produkčním configu (env / run_iteration) jako SAMOSTATNÝ krok.
2. Příští fairtest / gate per-race čtení překlopit na `fairtest_schedule` (F0) — drop-in.
