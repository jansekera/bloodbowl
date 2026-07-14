# Item 10 — defenzivní prior-floor rebalance: patch + re-derivovaný validační plán

**Datum:** 2026-07-14 · **Autor:** Fable 5 (read-only agent)
**Nahrazuje:** ztracený 7-counter plán z transcriptu mrtvé session (agent `a671c901b4af36636`,
2026-07-10). Čísla patche (0.05→0.08 + FOUL cap 0.08) přežila v master listu
(`project_bloodbowl_fable_queue_priority_20260710`, item 10); plán níže je znovu
odvozen proti aktuálnímu kódu, ne rekonstrukce po paměti.

> ⚠ **Provozní kontext vzniku:** při psaní běží trénink (PID 2474). NIC z tohoto
> dokumentu se dnes nespouští — žádný rebuild, žádné simulace. Vše níže je návrh
> k provedení až (a) trénink doběhne, (b) screen fixy 2a/2b (items 8+9) budou
> aplikované A ZMĚŘENÉ. Sekvence je závazná, viz §5.

---

## 0. Stav repa a kotev (ověřeno proti HEAD `251d21b`, 2026-07-14)

- `engine/src/macro_actions.cpp`: item 5 (stall-guard) = `a88f5e2`, item 6
  (blitz follow-up isOnPitch guard) = `ca297c9` — **oba už aplikované**. Master
  list z 07-10 je v tomhle zastaralý (u 5 i 6 má „NEAPLIKOVÁNO").
- Items 8 (patch 2a) a 9 (patch 2b) v HEAD **zatím nejsou** (ověřeno obsahem:
  `expandReposition` má stále hardcap 4, Strategy 0.5 v obranném řetězu chybí).
  Tento plán **předpokládá, že 2a+2b budou aplikované dřív** (dnes se aplikují
  a měří dle `proposals_screen_fixes_20260713.md`).
- **Klíčové pro kotvy:** items 2a/2b i H2-kickoff fix se týkají jiných souborů
  (`macro_actions.cpp`, `game_simulator.cpp`/`kickoff_handler.cpp`). Item 10 sedí
  celý v `engine/src/macro_mcts.cpp`, kterého se z celé fronty dotýká už jen
  item 16b (nRollouts zero-fill, řádky ~196-203 — jiný region, žádný překryv).
  **Diff v §1 proto platí beze změn ať už 2a/2b/H2 aplikované budou, nebo ne.**
  Kotvy ověřeny v HEAD: switch s floor/cap pravidly na `macro_mcts.cpp:311-382`,
  `case MacroType::REPOSITION` na `:371-373`, FOUL dnes padá do `default:` na
  `:380-381`.

### Mechanika, na které vše stojí (ověřeno čtením `expand()`, `macro_mcts.cpp:242-425`)

1. Priory startují **uniformně `1/n`** (`:262`), pak per-makro `minPrior` floor /
   `maxPrior` cap (`:308-391`), pak **jeden společný renorm** (`:392-398`).
   Žádný softmax. Z toho plynou přesné binding prahy:
   - starý REPOSITION floor 0.05 binduje až při `1/n < 0.05` ⇒ **n ≥ 21** — na
     reálných uzlech skoro nikdy ⇒ byl to near no-op;
   - nový floor 0.08 binduje při **n ≥ 13** (velké obranné uzly) a je záměrný
     no-op při n ≤ 12;
   - FOUL cap 0.08 je zrcadlo: binduje jen při `1/n > 0.08` ⇒ **n ≤ 12**
     (řídké uzly). Dnes FOUL bez capu zdědí plný uniform — při n=6 to je ~0.17,
     víc než BLOCK **floor** 0.12 (pozn. přesně: BLOCK má na řídkém uzlu taky
     raw 1/n, floor jen zdola; pointa je, že niche akce, která se nemůže
     dotknout míče, nemá mít prior ≥ BLOCK a 2× BLITZ-per-candidate bez
     jakéhokoli stropu).
2. **Celý floor/cap blok běží jen při `config_.policy != nullptr`**
   (`:292`). Ověřeno pro produkční cesty: self-play policy vždy nahrává;
   gate/benchmark ji od `run_iteration.py:435` (`GATE_USE_POLICY_PRIORS`,
   default 1) dostávají taky; `diag_utils.py` docstring i
   `diag_fresh_baseline_20260710.py` posílají `weights_policy.json`.
   **Past na měření:** diag skript, který `policy_weights_path` NEpošle, změří
   garantovaný no-op. Každý A/B skript itemu 10 MUSÍ posílat
   `policy_weights_path="weights_policy.json"` v obou ramenech (checklist §3).
3. `onDef` v `macro_mcts.cpp:306` = `state.ball.isHeld && !activeHasBall` —
   tj. **loose-ball stavy (míč na zemi) NEJSOU „onDef"**. Důsledek pro rozsah
   patche viz §1.3.

---

## 1. Přesný patch v1

### 1.1 Engine diff (`engine/src/macro_mcts.cpp`, proti HEAD `251d21b`; region nedotčen 2a/2b)

```diff
--- a/engine/src/macro_mcts.cpp
+++ b/engine/src/macro_mcts.cpp
@@ -368,13 +368,32 @@
                 case MacroType::PICKUP:
                     minPrior = 0.20f;
                     if (scoreDiff < 0) minPrior = 0.30f;
                     if (turnsRemaining <= 3) minPrior = 0.35f;
                     break;
                 case MacroType::REPOSITION:
-                    if (onDef) minPrior = 0.05f;
+                    // 2026-07-14 (master-list item 10): priors start uniform
+                    // at 1/n, so the old 0.05 floor only ever bound at n >= 21
+                    // candidates -- a near no-op. 0.08 binds at n >= 13 (large
+                    // defensive nodes, where the fixed-Y screen/safety spots
+                    // plus the 2b intercept lane live) and stays a deliberate
+                    // no-op at n <= 12. Rebalanced only AFTER 2a (real
+                    // movement budget) and 2b (intercept-lane targets) made
+                    // defensive REPOSITION actually arrive somewhere useful.
+                    if (onDef) minPrior = 0.08f;
                     break;
+                case MacroType::FOUL:
+                    // 2026-07-14 (item 10): FOUL used to fall through to
+                    // default: -- uncapped, inheriting the full uniform 1/n
+                    // (up to ~0.17 at sparse n=6 nodes), i.e. more prior than
+                    // BLOCK's 0.12 floor guarantees, for a niche action that
+                    // cannot touch the ball. Defensive cap mirrors the
+                    // REPOSITION floor boundary: binds only at n <= 12.
+                    // Deliberately onDef-gated: loose-ball FOUL overuse is
+                    // item 7 (pickup candidates) territory, not rebalanced
+                    // here (attribution -- see the validation doc).
+                    if (onDef) maxPrior = 0.08f;
+                    break;
                 case MacroType::END_TURN:
                     if (priors[i] > 0.10f && n > 2) {
                         priors[i] = 0.10f;
                         needsRenorm = true;
                     }
                     break;
```

(Kontext je unikátní; kdyby se čísla řádků posunula, `git apply --recount` /
`patch -F3` diff usadí obsahem.)

### 1.2 Testovací accessor (podmínka testovatelnosti, čistá addice)

`expand()` je privátní a **žádný existující gtest priory z `expand()` nepokrývá**
(testy v `test_macro_mcts.cpp` si priory nastavují ručně; CAGE fix `3fd9285` je
jeden z 8/11 fixů bez C++ regresního testu — nález auditu, item 15). Bez tohohle
accessoru nejde splnit konvenci negativní kontroly:

```diff
--- a/engine/include/bb/macro_mcts.h
+++ b/engine/include/bb/macro_mcts.h
@@ (public sekce MacroMCTSSearch, za lastChildVisits())
     const std::vector<MacroChildVisitInfo>& lastChildVisits() const { return lastChildVisits_; }
+
+    // Test-only: expand a fresh root for `state` and return each child's
+    // (macro, prior) after floor/cap + renorm. Pure wrapper over the private
+    // expand(); exists so the prior floor/cap regime is pinnable by gtest
+    // (audit 2026-07-10: 8/11 shipped fixes had zero C++ regression tests).
+    std::vector<std::pair<Macro, float>> expandRootPriorsForTest(const GameState& state);
```

```cpp
// engine/src/macro_mcts.cpp (kamkoli za expand())
std::vector<std::pair<Macro, float>> MacroMCTSSearch::expandRootPriorsForTest(
        const GameState& state) {
    MacroMCTSNode root;
    expand(&root, state);
    std::vector<std::pair<Macro, float>> out;
    out.reserve(root.children.size());
    for (auto& c : root.children) out.emplace_back(c.macro, c.prior);
    return out;
}
```

Pozn.: root Dirichlet noise se aplikuje až v `search()` (`:131-146`), ne
v `expand()` — accessor je deterministický i bez ohledu na `dirichletAlpha`.

### 1.3 Vědomé hranice rozsahu v1 (zapsat, neaplikovat spekulativně)

- **FOUL cap je onDef-gated** (dle master listu). Tím **nepokrývá** loose-ball
  FOUL overuse z `evidence/fable_replay_mining_findings.md` (FOUL zvolen 853×
  místo PICKUP při míči na zemi) — tam je `onDef=false` (míč nikdo nedrží).
  To je záměr: loose-ball scramble je doména itemu 7 (PICKUP top-2/3) a
  rozšíření capu mimo onDef by slilo atribuci obou itemů. Counter C5 níže to
  hlídá jako null-check.
- Drobná známá asymetrie: `onDef` v `macro_mcts.cpp` vs `onDefense`
  v `macro_actions.cpp` se liší jen ve stavu „míč mimo pitch a nedržen"
  (přechodný stav uvnitř resolv­ování). Bez akce, jen pro vědomí.

---

## 2. Regresní testy (gtest) + protokol negativní kontroly

Do `engine/tests/test_macro_mcts.cpp`. Společný setup: `PolicyNetwork` je
default-constructible s nulovými váhami (`policy_network.h:30`) — přesně
aktivuje heuristický floor režim bez učeného obsahu (produkční regime
self-play při `policy_blend=0`):

```cpp
PolicyNetwork dummyPolicy;            // zero weights, linear
MCTSConfig cfg;
cfg.policy = &dummyPolicy;            // aktivuje floor/cap větev (:292)
cfg.policyBlend = 0.0f;               // žádný blend -- čistě heuristické priory
MacroMCTSSearch search(nullptr, cfg, 42);
auto priors = search.expandRootPriorsForTest(state);
```

Klíčový trik: **assertovat poměry, ne absolutní hodnoty** — poměr dvou priorů
přežije společný renorm beze změny (floor 0.08 / floor 0.20 = 0.4 ať je suma
jakákoli). A **vždy nejdřív ASSERTnout počet kandidátů n** (z `priors.size()`),
protože binding prahy na n závisí — test se tak sám ohlídá proti driftu
generace kandidátů (vč. budoucího itemu 7, který n změní a test to LOUDLY
odhalí, místo aby tiše testoval jiný režim).

### T1 `DefensiveRepositionFloorBindsAtLargeNodes` — negativní kontrola MUSÍ selhat pre-patch

Stav: AWAY nosič drží míč; HOME (aktivní) má ≥12 volných stojících hráčů
(každý = 1 REPOSITION), nosič v dosahu blitzu ⇒ obrana emituje až 2 BLITZ
(`macro_actions.cpp:369`), + END_TURN ⇒ **n ≥ 15 ⇒ ASSERT n ≥ 13 a n ≤ 20**.
- BLITZ floor 0.20 binduje (1/n < 0.20 od n≥6), REPOSITION floor binduje až
  post-patch.
- `EXPECT_NEAR(prior(REPOSITION)/prior(BLITZ), 0.08f/0.20f, 0.02)` → post-patch
  0.400.
- **Pre-patch hodnota (pro negativní kontrolu):** REPOSITION má raw `1/n`
  (0.05 floor při 13≤n≤20 nebinduje!), tj. poměr `1/(0.20·n)` — např. 0.333
  při n=15 → test selže. Při n≥21 by byl 0.25 → taky selže.

### T2 `DefensiveFoulCapBindsAtSparseNodes` — negativní kontrola MUSÍ selhat pre-patch

Stav: řídký obranný uzel, **ASSERT 6 ≤ n ≤ 12**: AWAY nosič + jeden AWAY hráč
PRONE adjacentně k HOME hráči (⇒ 1 FOUL, `macro_actions.cpp:492-509`; prone
soupeř neblokuje „free" check pro REPOSITION — ten počítá jen STANDING,
`:554`), pár volných HOME hráčů, nosič v dosahu blitzu.
- `EXPECT_NEAR(prior(FOUL)/prior(BLITZ), 0.08f/0.20f, 0.02)` → post 0.400;
  pre-patch FOUL = raw 1/n (např. 0.125 při n=8) ⇒ poměr 0.625 → selže.
- Navíc `EXPECT_LT(prior(FOUL), prior(BLOCK))` je-li BLOCK přítomen (pre-patch
  jsou si rovné na 1/n → selže, post 0.08 < max(1/n, 0.12) → projde).

### T3 `RepositionFloorNoOpAtSmallNodes` — guard, projde pre- I post-patch

Stav: obranný uzel **ASSERT n ≤ 12, BEZ FOUL kandidáta** (žádný prone soupeř).
Při n ≤ 12 je raw `1/n ≥ 0.0833 > 0.08` ⇒ nový floor nebinduje ⇒ priory musí
být identické se starým kódem. `EXPECT_NEAR(prior(REPOSITION)/prior(BLITZ),
(1.0f/n)/0.20f, 1e-4)`. Přibíjí deklarovanou no-op hranici — přesně ten typ
testu, který by odhalil „fix širší, než bylo řečeno" (lekce halfclock).
Pozor na návrh stavu: bez FOUL kandidáta proto, že FOUL cap při n≤12 binduje
a změnil by renorm sumu (a tedy VŠECHNY renormované hodnoty) — poměrový test
by přežil, ale čistota „bit-identické priory" ne; poměr REPO/BLITZ je vůči
tomu imunní, přesto držet stav bez FOUL pro jednoznačnost.

### T4 `OffensivePriorsUntouchedByDefensiveRebalance` — guard, projde pre- i post-patch

Stav: HOME má nosiče (ofenzíva), volní hráči ⇒ REPOSITION kandidáti, prone
soupeř u jednoho hráče ⇒ FOUL kandidát. `onDef=false` ⇒ ani floor, ani cap:
`EXPECT_NEAR(prior(REPOSITION), prior(FOUL), 1e-6)` (oba = raw uniform po
renormu). Přibíjí onDef-gating obou změn.

### Protokol negativní kontroly (vzor `c020212`/`271579e`, závazný)

1. Aplikovat **jen** accessor (§1.2) + testy, rebuild, spustit
   `./engine/build/bb_tests --gtest_filter='MacroMCTS.Defensive*:MacroMCTS.Reposition*:MacroMCTS.Offensive*'`
   → **T1 a T2 MUSÍ SELHAT** s hodnotami odpovídajícími pre-patch vzorcům výše
   (zapsat skutečné naměřené hodnoty do commit message); T3, T4 musí PROJÍT.
2. Aplikovat engine diff (§1.1), rebuild → všechny 4 PASS.
3. Celá suita `./engine/build/bb_tests` zelená (aktuální plný počet + 4).
   Jediný test, který by teoreticky mohl být citlivý: nic v suitě dnes
   nepinuje konkrétní priory (ověřeno grepem) — očekává se 0 dotčených.

---

## 3. 7-counter validační plán (definice, prahy, pořadí)

Rozhodující nástroj: **paired-seed A/B přes `diag_utils.py`** (`a3f29dd`,
závazná konvence z `feedback_draw_rate_noise_floor`): candidate =
HEAD(2a+2b)+item10, reference = HEAD(2a+2b). Mirror konfigurace jako
`diag_fresh_baseline_20260710.py`, N=150 párů, `base_seed=20260715` (konvence:
datum spuštění). Obě ramena **musí** posílat
`policy_weights_path="weights_policy.json"` (jinak se měří no-op, viz §0.2)
a identický `weights_best.json` + `weights_policy.json` snapshot (zaznamenat
SHA souborů vedle `save_arm()` JSONů — konvence z docstringu `save_arm`).

Countery se těží z `LoggedGameResult.get_policy_decisions()`
(`bb_module.cpp:177-196`): decision = jeden makro-výběr, `visits` seřazené
sestupně dle visit count (`macro_mcts.cpp:779-792`; vykonává se
`mostVisitedChild`), tj. **`visits[0]` = vykonané makro**; makro-typ z one-hotu `action_features[0..9]`
(`macro_actions.cpp:1272-1286`: [3]=BLITZ vč. BLITZ_AND_SCORE, [4]=BLOCK,
[7]=FOUL, [8]=REPOSITION); kontext ze `state_features`
(`feature_extractor.cpp`: f12 = mám míč, f13 = soupeř má míč ⇒ obranný tah,
f14 = míč na zemi). Nový skript `diag_prior_floor_ab.py` (worker níže, §3.1).

### C1 — A/A kalibrace + determinismus pipeline (PRVNÍ, před A/B)

**Co:** na baseline binárce (HEAD+2a+2b) spustit counter-worker na dvou
disjunktních seed panelech (`base_seed=20260715` a `20260716`, N=150 každý).
**Definice šumového dna:** pro každý counter `|Δ_AA|` mezi panely = empirická
kalibrace; párová SE z A/B musí být s ní konzistentní (pokud A/B CI vychází
výrazně užší, než co A/A reálně ukazuje, je chyba v pipeline, ne objev).
Navíc levný determinismus check: 5 seedů znovu → bit-identické skóre i
countery (engine je seed-deterministický i přes Pool — ověřeno 2026-07-10).
**Práh:** žádný pass/fail o patchi; kalibrační krok. Efekt v A/B se počítá jen
tehdy, když překročí `max(2×|Δ_AA|, párové 95% CI vylučující 0)`.
**Úspora:** pokud dnešní měření 2b použije stejný counter-worker (doporučeno —
plán 2b už engagement guard vyžaduje), jeho candidate rameno (HEAD+2a+2b) JE
panel 1 baseline itemu 10 i polovina A/A — jen binárka nesmí být mezi tím
přebuildovaná (SHA check).

### C2 — Prior-mass dekompozice / no-op hranice (manipulation check)

**Co:** (a) gtesty T1-T4 z §2 (analytická vrstva); (b) runtime vrstva z A/B:
podíl vykonaných REPOSITION na obranných decisions (f13=1) rozdělený dle
velikosti uzlu — proxy `len(visits)`: `≥13` = velký uzel, `<13` = malý
(caveat: decisions jsou top-20 truncated a n se neloguje, takže `len(visits)`
je dolní odhad n; pro klasifikaci ≥13/<13 postačuje, pro n>20 nerozliší).
**Práh/očekávání:** nárůst REPOSITION share **koncentrovaný na velkých
uzlech**; na malých uzlech bez FOUL beze změny (v rámci C1 šumu). Když se
efekt objeví i na malých uzlech, patch dělá něco jiného, než tvrdí → STOP a
root-cause před jakýmkoli dalším krokem.

### C3 — Engagement guard (ČERVENÁ LINIE, veto nad vším ostatním)

**Co:** `def_engage_rate` = podíl obranných decisions (f13=1), jejichž vykonané
makro je BLITZ nebo BLOCK (one-hot [3] ∨ [4]). Párově per-seed (Δ na páru,
párová SE).
**Proč:** REPOSITION je jediné dice-free/turnover-free makro — víc prior mass
může místo screenů krmit pasivitu/stalling. **A JAK zamýšlený efekt, TAK tahle
patologie snižují conceded-TD** — conceded-TD sám úspěch od patologie
nerozliší. Tohle je jediný counter, který je rozliší.
**Práh:** relativní pokles `def_engage_rate` **> 15 %** (konzervativní kraj
pásma 15-20 % z master listu) s párovým CI vylučujícím 0 ⇒ **REJECT/revert
bez ohledu na všechno ostatní**. Pokles 10-15 % nebo CI přes 0 při bodovém
poklesu >10 % ⇒ prodloužit na N=300 před verdiktem.

### C4 — Conceded-TD/g (primární úspěchová metrika)

**Co:** (a) v mirror A/B: celkové TD/g (obě strany champion — těsnost obrany);
(b) směrovaná sonda `diag_vs_scorer.py` pattern (champion brání vs greedy
scorer, stejné seedy v obou ramenech): conceded-TD/g.
**Očekávání/práh:** pokles; ÚSPĚCH = pokles s párovým 95% CI vylučujícím 0
(nebo > 2×A/A dno) **A SOUČASNĚ C3 zelený**. Pozor na strop: champion už
inkasuje málo (≈0.02 TD/g vs learning) — mirror může být saturovaný, proto
je greedy-scorer sonda primární (víc hrozeb na hru = víc signálu).

### C5 — FOUL usage dekompozice (manipulation check + null check)

**Co:** (a) `def_foul_rate` = podíl obranných decisions s vykonaným FOUL —
**očekávaný POKLES** (cap binduje na řídkých obranných uzlech; kontext:
při f42=1 dnes ~8 % obranných decisions utrácí tah za FOUL, findings §4);
(b) `loose_foul_rate` = podíl FOUL na decisions s f14=1 (míč na zemi) —
**očekávaná NULA ZMĚNY** (onDef tam nesvítí; 853× FOUL-místo-PICKUP je mimo
rozsah v1). Pokud (a) neklesne → cap se v praxi netrefuje (málo řídkých
obranných uzlů s FOUL kandidátem) — patch není nutně špatně, ale FOUL část je
prakticky mrtvá, zapsat. Pokud (b) se pohne nad C1 dno → onDef detekce nebo
atribuce je rozbitá → STOP.

### C6 — Draw-rate tripwire (paired McNemar, NE cíl)

**Co:** `mcnemar_report(cand, base, outcome="draw")` z `diag_utils.py`.
**Interpretace (závazná konvence):** jedno-běhová delta <10pp = INCONCLUSIVE;
u obranného fixu je **nárůst remíz očekávaný výsledek, ne regrese**. Tripwire:
CONFIRMED nárůst remíz **spolu s** červeným/hraničním C3 = patologický
stalling ⇒ revert. CONFIRMED nárůst se zeleným C3 = přijatelné (obrana drží,
engagement neklesl).

### C7 — Watchdog + délka her + suita (sanity)

**Co:** watchdog-skip **0/150 v každém rameni** (referenční stav od `ea13fcb`:
0/400); průměr akcí/hru v rámci ±20 % baseline ramene (item 10 nemění
expanze maker, jen priory — na rozdíl od 2a tu není důvod k růstu; >±20 % =
red flag); žádné MAX_ACTIONS zásahy; plná gtest suita zelená (§2).

### Pořadí provedení

1. §2 testy + negativní kontrola (první rebuild po doběhnutí tréninku).
2. C1: A/A kalibrace na baseline binárce (ideálně recyklací 2b měření).
3. Rebuild s patchem → paired A/B N=150 (mirror) + greedy-scorer sonda.
4. Mining C2-C7 z uložených ramen (`save_arm` + surové decision countery).
5. Verdikt dle matice §3.2, zápis do paměti, commit-před-tréninkem
   (`feedback_commit_before_training`).

### 3.1 Worker pro `diag_prior_floor_ab.py` (skica)

```python
# stejná 9-tuple jako _gate_game; vrací (hs, as, counters)
def _gate_game_counters(args):
    (seed, race_idx, gate_path, frozen_path, mcts_iterations,
     vf_blend, tv, leaf_lookahead, policy_path) = args
    import bb_engine
    hr = bb_engine.get_developed_roster(_RACES[race_idx % 5], tv)
    ar = bb_engine.get_developed_roster(_RACES[(race_idx + 1) % 5], tv)
    lgr = bb_engine.simulate_game_logged(
        hr, ar, home_ai='macro_mcts', away_ai='macro_mcts',
        seed=seed, mcts_iterations=mcts_iterations,
        weights_path=gate_path, away_weights_path=frozen_path,
        epsilon=0.0, vf_blend=vf_blend, leaf_lookahead=leaf_lookahead,
        policy_weights_path=policy_path,           # BEZ TOHO SE MĚŘÍ NO-OP!
        dirichlet_alpha=GATE_DIRICHLET_ALPHA, exploration_c=GATE_EXPLORATION_C)
    c = dict(def_dec=0, def_engage=0, def_foul=0, def_big=0, def_big_repo=0,
             def_small=0, def_small_repo=0, loose_dec=0, loose_foul=0)
    for d in lgr.get_policy_decisions():
        v = d['visits']
        if not v:
            continue
        top = v[0]['action_features']       # visits sestupně => [0] = vykonané
        f = d['state_features']
        if f[13] > 0.5:                     # obranný tah (soupeř má míč)
            c['def_dec'] += 1
            if top[3] > 0.5 or top[4] > 0.5: c['def_engage'] += 1
            if top[7] > 0.5:                 c['def_foul'] += 1
            big = len(v) >= 13              # proxy n (top-20 truncation caveat)
            c['def_big' if big else 'def_small'] += 1
            if top[8] > 0.5:
                c['def_big_repo' if big else 'def_small_repo'] += 1
        elif f[14] > 0.5:                   # loose ball
            c['loose_dec'] += 1
            if top[7] > 0.5:                 c['loose_foul'] += 1
    r = lgr.result
    return (r.home_score, r.away_score, c)
```

Napojení na `diag_utils.run_arm(..., game_fn=_gate_game_counters)`; pro McNemar
na remízách se z výsledku vezmou první dva prvky, countery se agregují zvlášť
(párové per-seed rozdíly rate metrik + párová SE — malá lokální funkce ve
skriptu, `diag_utils.py` netřeba měnit). Decisions pokrývají obě strany hry
(home dávka, pak away — `bb_module.cpp:488-507`), v mirroru se agregují obě.

### 3.2 Rozhodovací matice

| Situace | Verdikt |
|---|---|
| C3 červený (engagement −15 %+, CI mimo 0) | **REVERT**, bez ohledu na C4/C6; zapsat jako potvrzení pasivitní patologie; eskalaci §6 NEzkoušet bez redesignu |
| C2 manipulation check selže (efekt mimo deklarované uzly) | **STOP + root-cause** (patch nedělá, co tvrdí) — vzor halfclock lekce |
| C5(b) loose-ball FOUL se pohne | **STOP + root-cause** (rozbitá onDef detekce/atribuce) |
| C4 CONFIRMED pokles + C3 zelený | **KEEP**; nová baseline, zapsat do paměti |
| C4 INCONCLUSIVE + C3 zelený + C2/C5(a) potvrzený mechanismus + C6/C7 čisté | **KEEP** (mechanismus prokázán, žádný harm signál; FOUL cap má korektnostní charakter) — ale explicitně zapsat „efekt na hru neprokázán", re-review po příštím dlouhém tréninku |
| C4 INCONCLUSIVE + C2/C5(a) ukazují, že floor/cap skoro nebindují (málo velkých/řídkých uzlů) | **KEEP jen FOUL cap? NE** — nedělit patch po změření (nová baseline by byla třetí); zapsat jako podklad pro eskalaci §6 a rozhodnout s uživatelem |
| C6 CONFIRMED nárůst remíz + C3 hraniční | prodloužit N=300; pokud trvá → REVERT |

---

## 4. Proč VÝHRADNĚ až po změření 8+9 (interakční analýza renorm poolu)

### 4.1 Co přesně item 10 udělá s poolem po 2b

Mechanicky: 2b **nemění počet kandidátů** (pořád právě 1 REPOSITION na volného
obránce — Strategy 0.5 jen mění `target` prvního gate-prošlého hráče, viz
`proposals_screen_fixes_20260713.md` §2b/1), takže **binding prahy itemu 10
(n≥13 / n≤12) zůstávají po 2b beze změny**. Co se mění, je **kvalita** cílů:
floored mass, kterou item 10 přidá, po 2b částečně poteče do intercept-lane
cílů (lajna nosiče) místo výhradně do fixních-Y screenů. Tzn.:

- **Bez 2a** by item 10 přidával prior makrům, která se zaseknou po 4 krocích
  a nikdy nedojdou (hardcap) — čisté krmení pasivity; měřit item 10 před 2a
  = měřit jinou (horší) intervenci.
- **Bez 2b** posiluje jen fixní-Y obranu — očekávaný efekt menší a risk/benefit
  jiný. Item 10 „v1 čísla" byla navržena s vědomím, že 2b bude pod ním.
- **S 2a+2b** je to zamýšlená intervence. Proto musí být baseline ramenem A/B
  přesně binárka HEAD+2a+2b — a její counter baseline (engagement, conceded)
  vznikne právě měřením 8+9. Červená linie C3 se počítá RELATIVNĚ k této
  baseline; kdyby 8/9 skončily revertem/přepracováním, prahy itemu 10 jsou
  stale a plán se musí re-derivovat znovu.

### 4.2 Kvantifikace přerozdělení (worked examples)

Pool po floor/cap se renormuje společně; každý ne-REPOSITION kandidát ztrácí
relativně `1 − S_old/S_new`, kde `S` je suma po floor/cap a
`S_new = S_old + 0.03·R (+ případný FOUL cap deficit)`, R = počet REPOSITION
kandidátů s bindujícím floorem.

- Velký uzel n=20 (12 REPO, 2 BLITZ, 4 BLOCK, 1 FOUL, 1 END_TURN; uniform 0.05):
  S_old = 12·0.05 + 2·0.20 + 4·0.12 + 0.05 + 0.05 = 1.58;
  S_new = 12·0.08 + … = 1.94 ⇒ BLITZ/BLOCK ztrácí **~18.6 %** relativně.
- Střední uzel n=16 (8 REPO, 2 BLITZ, 4 BLOCK, FOUL, ET; uniform 0.0625):
  S_old = 1.525, S_new = 1.645 ⇒ ztráta **~7.3 %**.
- Master list uváděl worst-case 11-16 %; přesná hodnota závisí na složení
  uzlu — realistické pásmo **~7-19 %**, roste s podílem REPOSITION kandidátů.
  Per-candidate hierarchie zůstává: BLITZ 0.20 vs REPOSITION 0.08 = **2.5×**
  na kandidáta (master list konzervativně „≥2.3×"). Právě proto je C3
  engagement guard veto metrika — ztráta ~19 % relativního prioru BLOCK/BLITZ
  na velkých uzlech je přesně mechanismus, kterým by se pasivita vyrobila.

### 4.3 Item 7 (PICKUP top-2/3) — proč nesmí sdílet měřicí okno

Item 7 přidává PICKUP kandidáty s vysokými floory (0.20-0.35) do téhož poolu:
mění n (posouvá, kde floor 0.08 binduje), zvyšuje S (ředí všechny), a mění
loose-ball chování, které C5(b) používá jako null-check. Aplikace 7 a 10
v jednom okně ⇒ žádný counter nejde atribuovat. Master pořadí: **8 → 6(už je)
→ 9 → 7 → 10**, každý se svým měřením; 10 jako POSLEDNÍ engine-behavior fix.
(Cross-patch file-mechanika: item 7 je `macro_actions.cpp`, item 10
`macro_mcts.cpp` — konflikt je čistě behaviorální, ne textový; potvrzeno
nálezem itemu 18.)

### 4.4 Baseline resety kolem (koordinace s H2 kickoff fixem)

H2-kickoff bug fix (priorita dneška dle `project_bloodbowl_h2_kickoff_bug_20260713`)
resetuje baseline sám o sobě a má se kombinovat se screen fixy 8+9 do jednoho
měřicího balíku. Item 10 pak přichází NAD tuhle novou baseline jako samostatný
krok — v žádném případě ho nepřibalovat do téhož balíku (přesně ten „apply
close together" scénář, před kterým master list varuje). Po KEEP verdiktu
itemu 10 platí nový referenční stav pro všechna další srovnání; spouštět
další dlouhý trénink až s commitnutým verdiktem (server dělá git pull).

---

## 5. Tvrdá sekvence (checklist před spuštěním čehokoli)

1. ☐ Trénink (PID 2474) doběhl; žádný rebuild dřív.
2. ☐ 2a aplikován, změřen (vlastní A/B dle proposals doc).
3. ☐ 2b aplikován, změřen NAD 2a; counter baseline (engagement, conceded,
   FOUL raty) uložená přes `save_arm` + SHA binárky zapsané.
4. ☐ Verdikt 8+9 = KEEP (jinak re-derivovat tento plán).
5. ☐ Item 7 NENÍ aplikován (musí jít až po 10, nebo 10 po něm — ale nikdy
   ve stejném měřicím okně; dle master listu 7 před 10, tj. pokud mezitím
   padne rozhodnutí 7 aplikovat, item 10 čeká na změření 7).
6. ☐ Accessor + testy → negativní kontrola (T1/T2 fail pre-patch) → patch →
   404+/404+ zelená.
7. ☐ C1 A/A kalibrace → paired A/B N=150 + scorer sonda → C2-C7 → matice §3.2.
8. ☐ Zápis výsledku do paměti + commit-před-tréninkem.

## 6. Eskalace, pokud v1 podstřelí (ZAPSÁNO, NEAPLIKOVAT spekulativně)

Z master listu, beze změny: cap REPOSITION kandidátů už při generaci (top-4
dle priority strategií — po 2b: intercept > cage-tag > safety > marker)
v `macro_actions.cpp` + zvednutí flooru na paritu s BLOCK 0.12. Dvousouborová
změna s vlastním review cyklem a vlastním kompletním validačním kolem (tenhle
dokument pak slouží jako šablona). Trigger: C4 INCONCLUSIVE + C2 ukazuje, že
floor reálně skoro nebinduje (málo n≥13 uzlů) + C3 zelený. NIKDY po červeném
C3 (to je signál redesignovat, ne přitlačit).
