# Item 13 MVP — staged safe-then-PICKUP celo-tahový plánovač (2026-07-31)

**Stav: HOTOVO (implementace + testy + harness); PLNÁ validace PŘIPRAVENA, NESPUŠTĚNA**
(worktree `agent-a978afb18db853696`, NEpushnuto — commity jen ve worktree)

Zadání: [project_bloodbowl_item13_wholeturn_pickup_planner_20260728] — MVP scope
POTVRZENÝ uživatelem: jeden tah, bez multi-turn lookahead. Safe akce (backup
positioning k volnému míči) deterministicky NAPŘED, pak JEDINÝ stochastický
branch = PICKUP (success → pokračování pod ADVANCE cílem, fail → stop + fixní
penalizace −0.10). Hodnota plánu = P·V(success) + (1−P)·V(fail), obě V přes
existující statický `simulate()` heuristik. Žádný BLITZ/LOS/FOUL/multi-turn.

## Commity (worktree, nepushnuto)

- `9e63d43` — feat(engine): StagedTurnPlanner + config gate + integrace do
  MacroMCTSPolicy + 15 unit testů
- `aabd4c1` — feat(diag): harness + miner + 12 mined stavů

## Design rozhodnutí

1. **Cíl tahu (`classifyTurnGoal`)** — explicitní zrcadlo aritmetiky, kterou
   `simulate()` už kóduje implicitně (macro_mcts.cpp, offensive scoringBonus):
   míč volný na hřišti → `PICKUP_BALL`; náš carrier a endzóna v dosahu
   `movementRemaining+2` NEBO urgence (`turnsLeft<=2` a dist ≤ MA+2) →
   `SCORE_BALL`; jinak `ADVANCE_BALL`; soupeřův míč / mimo PLAY → `NONE`.
   MVP staví plán JEN pro `PICKUP_BALL` — ostatní cíle vrací `valid=false`
   a policy jede dál přes search() jako dnes.

2. **Safe stage** — kandidáti jsou výhradně REPOSITION makra (jediný
   dice-free typ; s volným míčem generátor cílí na volná pole VEDLE míče —
   item11 fix f03ef2a — takže „přines backup" vzniká uspořádáním, ne novým
   makrem). Každé makro se před přijetím MC-sonduje (PROBE_K=48, práh
   SAFE_PTO=0.02 + ne-no-op ≥0.5 akce — stejné prahy, které validoval item10
   Q-guard) na **vyvíjející se projekci**, ne na kořenovém stavu.
   **Kandidáti se po každém přijetí REGENERUJÍ z projekce** — jednorázová
   sklizeň z kořene dávala všem spoluhráčům STEJNÉ „nejbližší volné pole u
   míče" (pozorováno na g0000: 6 maker, 1 příchod); regenerace přepočítá cíl
   dalšího backupu proti aktuální obsazenosti. Terminace: přijatý hráč má
   hasMoved (mizí z poolu), odmítnutí hráči se pamatují — pool striktně
   klesá. Picker je ze safe stage vyloučen (hraje jen jednou, v branchi).
   Řazení: cíl nejblíž míči první.

3. **Branch** — `sampleBranch` (BRANCH_K=64 vzorků `greedyExpandMacro(PICKUP)`
   na projektovaném stavu): success = makro končí s míčem v našem držení.
   Vědomá MVP simplifikace: riziko approach cesty (dodge/GFI po cestě k
   míči) je PŘIBALENO do jediného branche — drží přesně jeden branch point a
   přitom oceňuje riziko, které exekutor reálně podstoupí. V(success) přes
   `expandPickup` zahrnuje i jeho vestavěný stall-aware ADVANCE po sebrání
   (= „pokračování pod ADVANCE cílem" bez nové mašinerie). V(fail) = střední
   leaf eval fail vzorků + fixní FAIL_PENALTY −0.10 (styl
   greedyLookaheadBonus); žádný bounce/recovery strom.
   Leaf eval = **existující** `simulate()` přes nový 1-řádkový public
   wrapper `MacroMCTSSearch::evaluateLeaf` (žádná nová value funkce).

4. **Top-2 PICKUP (item7)** — plán se staví pro oba emitované PICKUP
   kandidáty a vybere se vyšší planValue (levné, řeší interakci s item7
   zmíněnou ve specu).

5. **Integrace do policy** — `MacroMCTSPolicy::nextStagedMacro`: max 1
   stavba plánu na team-turn (hranice = team/turnNumber/half); plán dodává
   makra místo search(). **Deviace** = sémantická revalidace
   (`stagedMacroStillValid`) selže → zbytek plánu se zahodí a zbytek tahu
   jede existující per-macro search() re-planning. Revalidace je záměrně
   sémantická (hráč stojí+neodehrál; REPOSITION cíl volný a ≠ pole volného
   míče; PICKUP: míč pořád volný na stejném poli a v dosahu), NE „vygeneruj
   znovu a porovnej" — cíle REPOSITION se při každé generaci přepočítávají,
   exact-match by zdravý plán označil za deviaci hned po prvním kroku.
   Extra pojistka: pokud plánované makro expanduje naprázdno (drift, který
   validátor nevidí), policy NEukončí tah (starý fallthrough vracel
   END_TURN), ale zahodí plán a spustí search. Decision logging se pro
   staged makra přeskakuje (lastChildVisits by byl stale).

6. **Config gate** — `MCTSConfig::stagedPickupPlanner`, default `false`
   (vzor `riskDeferral`). Produkční aktivace až po plné validaci.

## Soubory

- `engine/include/bb/turn_planner.h` — API + konstanty (SAFE_PTO=0.02,
  PROBE_K=48, BRANCH_K=64, FAIL_PENALTY=−0.10)
- `engine/src/turn_planner.cpp` — classifyTurnGoal, stagedMacroStillValid,
  StagedTurnPlanner
- `engine/include/bb/mcts.h` — +`stagedPickupPlanner`
- `engine/include/bb/macro_mcts.h` — +`evaluateLeaf` wrapper, +staged členy
  MacroMCTSPolicy
- `engine/src/macro_mcts.cpp` — integrace do `operator()`
- `engine/tests/test_turn_planner.cpp` — 15 testů: goal klasifikace ×5,
  stavba plánu ×4 (safe-first + backup-vedle-míče-ne-na-něm = item11,
  2-branch konzistence, AG-monotonie pSuccess, žádný plán mimo PICKUP cíl),
  validátor ×1 (7 pod-případů), config default OFF ×1, policy ×4
  (backup jedná před pickerem; celý tah — picker není první aktivace;
  deviace → fallback na search; gate OFF ⇒ chování identické se search)
- `diag_item13_mine_states.py` + `diag_item13_states.json` — 12 mined
  PICKUP-goal stavů (preference mid-game, 4 s reálným TO/nezotavením;
  zdroj: main repo `diag_replay_mine_20260730_data/`, jen čtení)
- `diag_item13_staged_planner_harness.cpp` — STAGE P (predikce plánu),
  STAGE AB (párované seedy A=produkce vs B=staged), kalibrace planValue vs
  realizovaná hodnota, item11 metrika „backups adjacent při pickupu"

## Testy

Celá suita: **468 testů, 467 zelených + 1 pre-existing wall-clock flake**
(`MCTS.TimeBudgetRespected` — padá jen pod plnou zátěží stroje s tréninkem,
izolovaně 3× po sobě zelený; s item13 nesouvisí). Všech 15 nových zelených.

## Mini-smoke harnessu (jediné spuštěné běhy; nPairs=4, ~1 min)

- Stav 0 (g0000 h1t4): plán 6 safe + PICKUP p22, pSuccess=0.83;
  **item11 metrika: B=1.00 vs A=0.00 adjacent backupů při pickup hodu**;
  hodnota delta +0.00 (N=4 — bez výpovědi, jen sanity).
- Stav 8 (g0008 h2t6, reálný TO): planner korektně ocenil **pSuccess=0.000**
  (míč tento tah nedosažitelný — obě ramena shodně nezajistí míč);
  item11 metrika opět B=1.00 vs A=0.00.

## Jak spustit plnou validaci (po víkendu, až skončí trénink)

```bash
cd /home/jan/claude/bloodbowl/.claude/worktrees/agent-a978afb18db853696
# harness už je zkompilovaný (./diag_item13_staged_planner); rebuild kdyžtak:
#   g++ -O2 -std=c++20 -Iengine/include -Iengine/third_party \
#       diag_item13_staged_planner_harness.cpp -Lengine/build -lbb_engine \
#       -Wl,-rpath,$PWD/engine/build -o diag_item13_staged_planner
# plný běh: 12 stavů × 2×200 celých tahů, search iters=100 na makro;
# odhad ~2–3 h na 1 jádře — pustit niced, klidně přes noc:
nice -n 19 ./diag_item13_staged_planner . diag_item13_states.json 200 \
    > diag_item13_full_$(date +%Y%m%d).log 2>&1 &
```

Vyhodnocení: (a) STAGE AB paired delta hodnoty + turnover/ball-secured/TD;
(b) kalibrace planValue vs realizovaná B střední hodnota (predikuje 2-branch
model realitu?); (c) item11 metrika backups-adjacent B >> A napříč stavy.
Práh smysluplnosti: konzistentní kladná delta / nižší TO rate přes stavy;
šumové dno per-stav ~±8–11 pp (viz feedback_draw_rate_noise_floor).

Pozn.: harness čte `weights_best.json`/`weights_policy.json` z argv[1]
(worktree má lokální kopie z main repa k 31.07.; pro validaci proti novějším
main vahám dát `/home/jan/claude/bloodbowl` jako repoRoot — JEN čtení).

## Otevřené otázky

- pSuccess z MC zahrnuje i approach riziko (dodge/GFI cestou k míči) —
  oceňuje realitu, ale „P(pickup)" v reportech je P(celé makro uspěje).
  Čistá pravděpodobnost hodu: `calculatePickupTarget` (helpers.h).
- pSuccess=0 stavy (g0008): plán pickup přesto zkusí (jako search; hráč
  aspoň dojde k míči). Otázka pro post-validaci: má plán v takovém případě
  raději jen stavět backup a pickup nechat na příští tah? (multi-turn úvaha
  → mimo MVP scope.)
- Konvergující kolona: když první backup nedorazí na cílové pole, další
  regenerovaní kandidáti dostanou totéž pole (stále volné) — chová se jako
  kolona k míči; ne chyba, ale plná validace ukáže, zda to proti produkci
  neplýtvá aktivacemi.
- Plán se staví max 1× za team-turn; po deviaci zbytek tahu řeší search
  (nový plán až příští tah) — vědomé MVP zjednodušení.
- Náklad stavby plánu: ~((#přijatých+odmítnutých) × 48 + 2×64) expanzí makra
  (typicky < 1000 ≈ srovnatelné s jedním search() callem) — jen když je
  gate zapnutý a míč volný.
- Rerolly: fixtures/harness používají konvenci item10 (2 rerolly; v replay
  logu nejsou). Branch vzorkování je zahrnuje přes attemptRoll.
