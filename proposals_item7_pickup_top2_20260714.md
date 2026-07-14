# Návrh patche: item 7 — PICKUP top-2 kandidáti (master list, Phase 2)

**Datum:** 2026-07-14 · **Autor:** Fable 5 (read-only agent)
**Zdroje:** `evidence/fable_replay_mining_20260714.md` (re-mining, GO pro item 7),
`evidence/fable_replay_mining_findings.md` (07-02), master list
`project_bloodbowl_fable_queue_priority_20260710` (item 7 + interakční riziko #1),
vzor formátu `proposals_screen_fixes_20260713.md`.

> ⚠ **Kontext vzniku:** Běží trénink (PID 2474) a ve worktree
> `/home/jan/claude/bloodbowl-fixes-20260714` se dnes aplikují screen fixy 2a/2b
> + H2-kickoff fix. Do enginu jsem NIC nezapisoval, nic nebuildil, nic nespouštěl.
> Všechny diffy jsou kotvené na **worktree HEAD `ca297c9`** (`fix(engine): guard
> the blitz follow-up loop against an off-pitch blocker`) a ověřené jako stabilní
> i po aplikaci 2a/2b (viz sekce 0). **Aplikovat až po zavření dnešního
> screen-fix/H2 okna a po doběhnutí jejich měření.**

---

## 0. Ověření kotev proti aktuálnímu stavu

Worktree HEAD: `ca297c9` — tj. **item 6 (blitz follow-up isOnPitch guard) je už
aplikován** (master list z 07-10 má u itemu 6 stále „neaplikováno" — zastaralé,
při aktualizaci paměti opravit; item 5 = `a88f5e2` byl zastaralý už 07-13).

| Kotva | HEAD `ca297c9` | Ověřený obsah | Posun po 2a/2b? |
|---|---|---|---|
| PICKUP generace (bestPicker) | `engine/src/macro_actions.cpp:434-475` | `// PICKUP: ball on ground, best player by AG/distance/skills` … jediný `out.push_back({MacroType::PICKUP, bestPicker->id, …})` na :473 | **NE** — 2b vkládá na :513+ a :642+, 2a mění :1085+; PICKUP blok je nad oběma |
| `isFreeToAct` | `macro_actions.cpp:98-100` | `return p.canAct() && !p.hasMoved;` | ne |
| Prior-floor switch | `engine/src/macro_mcts.cpp:308-391` | `case MacroType::PICKUP: minPrior = 0.20f; if (scoreDiff < 0) minPrior = 0.30f; if (turnsRemaining <= 3) minPrior = 0.35f;` na :366-370 | ne (2a/2b se `macro_mcts.cpp` nedotýkají) |
| Renorm | `macro_mcts.cpp:392-398` | `if (needsRenorm) { … priors[i] /= sum; }` | ne |
| `lastChildVisits_` fill | `macro_mcts.cpp:213-219` | `lastChildVisits_.push_back({child.macro, child.visits});` | ne |
| `MacroChildVisitInfo` | `engine/include/bb/macro_mcts.h:37-40` | `{ Macro macro; int visits; };` | ne |

Klíčová ověření mechaniky (proti kódu, ne paměti):

- **Prior-floor režim je v produkci AKTIVNÍ.** Blok B (heuristické floory) je
  gatovaný na `config_.policy != nullptr` (`macro_mcts.cpp:292`), NE na
  `policyBlend`. Produkce policy net nahrává vždy (policy_cache trik,
  `run_iteration.py:54-65` a komentáře u 9-tuple v `run_iteration.py:212-227`).
  Takže PICKUP floor 0.20/0.30/0.35 se dnes reálně uplatňuje per kandidát —
  interakční riziko #1 je reálné, ne teoretické.
- Při míči na zemi je `onDef=false` (`onDef = !activeHasBall && state.ball.isHeld`,
  `macro_mcts.cpp:306` — míč na zemi ⇒ `isHeld=false`). Důsledek: na uzlech, kde
  PICKUP vůbec existuje, se **neuplatňuje** BLITZ floor 0.20 ani REPOSITION floor
  0.05 (a tedy ani floory z itemu 10) — viz sekce 6.
- `MacroChildVisitInfo` nemá žádného konzumenta mimo `macro_mcts.cpp/.h`
  (grep: ani `bb_module.cpp`, ani testy) — rozšíření o `prior` je čistě aditivní.
- Jediné místo generující PICKUP makro je :473 (grep `MacroType::PICKUP` přes
  `engine/src`); expanze `expandPickup` (:1026) je per-`playerId`, druhého
  kandidáta zvládne beze změny. PHP strana žádnou macro vrstvu nemá (grep prázdný).

---

## 1. Návrh: top-2, ne top-3

**Změna:** `getAvailableMacros` vygeneruje až **2** PICKUP kandidáty (nejlepší a
druhý nejlepší picker podle stávajícího skóre `AG*10 − dist*3 + skilly`), místo
jediného `bestPicker`. Druhý kandidát jen pokud (a) existuje druhý dosažitelný
picker a (b) jeho skóre není o víc než **15 bodů** horší než nejlepší.

**Proč top-2 a ne top-3:**

1. **Rozpočet vizit.** Produkce hraje na MCTS100; loose-ball uzel má typicky
   n≈15-25 kandidátů → ~4-6 vizit na dítě. Každý kandidát navíc ředí vizity
   všech ostatních. Druhý kandidát je nutný k tomu, aby search vůbec MĚL
   alternativu (dnes nemá žádnou); třetí přidává nejmenší marginální hodnotu
   (skóre-ordered) za stejnou cenu.
2. **Pokrytí reálných stavů.** Mining pracuje se stavy „hráč do 1 pole od míče"
   (f71≤0.13); dosažitelných pickerů (dist ≤ MA+2) bývá v těchto stavech málo
   (typicky 1-3). Top-2 pokryje drtivou většinu reálné volby; top-3 řeší vzácný
   zbytek.
3. **Ovládnutelnost renorm rizika.** Zdvojení floored mass je řešitelné (sekce 3);
   ztrojení by vyžadovalo agresivnější zásah do floor struktury s větším blast
   radiem.
4. **Konvence projektu** ([[feedback_implementation_style]]): jedna minimální
   změna, změřit, pak eskalovat. Eskalační cesta je zdokumentovaná v sekci 5.4
   (top-3 + f72 uncontested boost), NEaplikovat spekulativně.

**Proč gate 15 bodů:** 15 = přesně bonus SureHands, ekvivalent 5 polí vzdálenosti
nebo 1.5 bodu AG. Druhý picker horší o >15 je kategoricky horší volba sběru
(mnohem dál nebo výrazně nemotornější) — emitovat ho znamená darovat floored
prior mass (viz sekce 3) kandidátovi, kterého MCTS na 100 iteracích stejně
spolehlivě nevyhodnotí. Kandidát uvnitř gate naopak pokrývá přesně ty případy,
kvůli kterým item 7 existuje: skoro-stejně-dobrý sběrač, jehož použití uvolní
nejlepšího hráče na blok/blitz/screen, nebo jehož pickup vede do krytější pozice.
Search arbitruje, generace jen přestane být slepá.

**Kontrakt pořadí:** primární picker se emituje PŘED sekundárním. Na tomto pořadí
závisí floor-split v `macro_mcts.cpp` (sekce 3) — kontrakt je zdokumentovaný
komentářem na obou místech a přibitý regresním testem T1.

Fallback větev (`findNearestFreePlayer`, :458-471) zůstává beze změny a sekundárního
kandidáta nikdy neemituje — je to už teď „zoufalá" volba pro stav bez kandidáta
v hlavní smyčce.

---

## 2. Unified diffy

Patch má **3 části** aplikované v tomto pořadí (část 2 je čistá observabilita bez
změny chování; oddělení částí 1 a 3 je záměrné kvůli protokolu negativní kontroly
— viz sekce 4).

### Část 1: generace top-2 (`engine/src/macro_actions.cpp`)

```diff
--- a/engine/src/macro_actions.cpp
+++ b/engine/src/macro_actions.cpp
@@ -431,10 +431,22 @@
         }
     });
 
-    // PICKUP: ball on ground, best player by AG/distance/skills
+    // PICKUP: ball on ground, top-2 pickers by AG/distance/skills.
+    // A single bestPicker candidate left the search with no alternative
+    // recoverer at all: per-turn ground recovery sat at ~28% and the
+    // near-ball pickup miss at 82% across two independent minings
+    // (evidence/fable_replay_mining_findings.md 07-02, _20260714 07-14;
+    // master-list item 7). The macro list is emitted BEST-FIRST -- the
+    // prior-floor split in macro_mcts.cpp (expand(), case PICKUP) relies
+    // on this ordering contract.
     if (ballOnGround) {
         const Player* bestPicker = nullptr;
         int bestPickerScore = -999;
+        const Player* secondPicker = nullptr;
+        int secondPickerScore = -999;
+        // Gate: a second picker more than 15 points behind (= the SureHands
+        // bonus, ~5 squares of extra distance or ~1.5 AG) is categorically
+        // worse -- emitting it would only donate floored prior mass.
+        constexpr int kSecondPickerMaxGap = 15;
 
         state.forEachOnPitch(mySide, [&](const Player& p) {
             if (!isFreeToAct(p)) return;
@@ -449,8 +461,13 @@
             if (p.hasSkill(SkillName::BigHand)) score += 5;
 
             if (score > bestPickerScore) {
+                secondPickerScore = bestPickerScore;
+                secondPicker = bestPicker;
                 bestPickerScore = score;
                 bestPicker = &p;
+            } else if (score > secondPickerScore) {
+                secondPickerScore = score;
+                secondPicker = &p;
             }
         });
 
@@ -472,6 +489,12 @@
         if (bestPicker) {
             out.push_back({MacroType::PICKUP, bestPicker->id, -1, state.ball.position});
         }
+        // Secondary picker: only from the main loop (never from the
+        // findNearestFreePlayer fallback -- secondPicker stays null there).
+        if (secondPicker &&
+            bestPickerScore - secondPickerScore <= kSecondPickerMaxGap) {
+            out.push_back({MacroType::PICKUP, secondPicker->id, -1, state.ball.position});
+        }
     }
 
     // PASS: have ball, pass not used, teammate in range
```

Poznámky: ukládání `&p` z `forEachOnPitch` lambdy je existující idiom téhož bloku
(bestPicker to dělá dnes); hráči žijí ve stabilním poli `GameState`. Při shodě
skóre vyhrává dřív navštívený hráč (striktní `>`), stejně jako dnes.

### Část 2: observabilita priorů (`engine/include/bb/macro_mcts.h` + `engine/src/macro_mcts.cpp`)

Potřebná pro regresní test T4 (a mimochodem zavírá část mining limitu „decision
záznamy nevidí priory"). Žádná změna chování.

```diff
--- a/engine/include/bb/macro_mcts.h
+++ b/engine/include/bb/macro_mcts.h
@@ -37,6 +37,7 @@
 struct MacroChildVisitInfo {
     Macro macro;
     int visits;
+    float prior = 0.0f;  // post-renorm root prior (diagnostics/tests)
 };
```

```diff
--- a/engine/src/macro_mcts.cpp
+++ b/engine/src/macro_mcts.cpp
@@ -214,7 +214,7 @@
     lastChildVisits_.clear();
     for (auto& child : root.children) {
         if (child.visits > 0) {
-            lastChildVisits_.push_back({child.macro, child.visits});
+            lastChildVisits_.push_back({child.macro, child.visits, child.prior});
         }
     }
```

(Jediné konstrukční místo struktury; `bb_module.cpp` ani decision-log ji
neserializují — ověřeno grepem, blast radius nula.)

### Část 3: floor-split sekundárního kandidáta (`engine/src/macro_mcts.cpp`)

```diff
--- a/engine/src/macro_mcts.cpp
+++ b/engine/src/macro_mcts.cpp
@@ -305,6 +305,11 @@
                               state.getPlayer(state.ball.carrierId).teamSide == state.activeTeam);
         bool onDef = !activeHasBall && state.ball.isHeld;
 
+        // Item 7 (top-2 PICKUP pickers): generation emits PICKUP candidates
+        // best-first (ordering contract documented at the generation site,
+        // macro_actions.cpp). Track how many we've floored so the secondary
+        // gets half the floor -- see the PICKUP case below.
+        int pickupSeen = 0;
         for (int i = 0; i < n; ++i) {
             float minPrior = 0.0f;
             float maxPrior = 1.0f;
@@ -366,6 +371,15 @@
                 case MacroType::PICKUP:
                     minPrior = 0.20f;
                     if (scoreDiff < 0) minPrior = 0.30f;
                     if (turnsRemaining <= 3) minPrior = 0.35f;
+                    // Secondary picker gets HALF the floor: the PICKUP
+                    // family's floored mass then grows 1.5x, not 2x
+                    // (master-list interaction risk #1 -- naive doubling
+                    // would give the family +78% post-renorm mass and
+                    // dilute every non-floored candidate by ~15%; the
+                    // split keeps it at ~+43% / ~-9%, numbers in
+                    // proposals_item7_pickup_top2_20260714.md section 3).
+                    if (pickupSeen > 0) minPrior *= 0.5f;
+                    ++pickupSeen;
                     break;
```

---

## 3. Interakční riziko #1 (zdvojení floored prior mass) — řešení + čísla

**Mechanika:** priory jsou uniform `1/n` + floor/cap + renorm (žádný softmax).
Každý PICKUP kandidát dostane vlastní `minPrior` floor; naivní top-2 tedy zdvojí
floored mass rodiny PICKUP před renormem. **Řešení = floor-split:** sekundární
kandidát dostane poloviční floor (0.10 / 0.15 / 0.175 podle situační větve).
Rodina tak před renormem drží 1.5× floor místo 2×, a protože renorm zachovává
poměry, primární kandidát má **vždy přesně 2×** prior sekundárního (pokud oba
floory bindují) — sekundární je „viditelná alternativa", ne rovnocenný konkurent.

**Kdy floory bindují** (base = `1/n`): primární 0.20 při n≥6; sekundární 0.10 při
n≥11. Na malých uzlech (n≤10) je poloviční floor záměrný no-op — stejná filozofie
jako item 10 („deliberate no-op at n≤12").

**Čísla pro typické loose-ball uzly** (složení dle miningu: PICKUP + ~2 BLOCK
floor 0.12 + zbytek non-floored; na loose-ball uzlech je `onDef=false`, takže
BLITZ/REPOSITION floory se neuplatňují; END_TURN cap 0.10 binduje jen při n≤9):

| Scénář | PICKUP mass dnes (1 kandidát) | naivní top-2 (2× floor 0.20) | **top-2 + floor-split (0.20+0.10)** |
|---|---|---|---|
| n=15→16, 2 BLOCK, floor 0.20 | 0.161 | 0.288 (**+79 %**) | **0.233 (+44 %)** · prim 0.155 / sek 0.078 |
| n=20→21, 2 BLOCK, floor 0.20 | 0.155 | 0.276 (**+78 %**) | **0.222 (+43 %)** · prim 0.148 / sek 0.074 |
| n=20→21, floor 0.35 (endgame/trailing) | 0.243 | 0.400 (**+65 %**) | **0.333 (+37 %)** · prim 0.222 / sek 0.111 |
| n=8→9 (malý uzel; sek. floor nebinduje) | 0.190 | 0.343 (+80 %) | **0.289 (+52 %)** · prim 0.186 / sek 0.103 |

Kolaterální ředění ostatních kandidátů (scénář n=20, relativně):

| Kandidát | naivní top-2 | **floor-split** |
|---|---|---|
| non-floored (REPOSITION/BLITZ/FOUL…) | −15.2 % | **−9.0 %** |
| BLOCK (floor 0.12) | −11.0 % | **−4.4 %** |
| primární PICKUP vs. dnešní jediný | −11.0 % | **−4.4 %** |

**Interpretace:** ~+40-50 % prior mass pro rodinu PICKUP je ŽÁDANÝ efekt (search
PICKUP prokazatelně podvažuje: zvolen v 50.2 % loose-ball-near rozhodnutí,
visit-fraction 0.18 když přeskočen — mining 07-14), ale +78-80 % z naivního
zdvojení by bylo slepé přestřelení s dvojnásobným ředěním všech ostatních maker.
Floor-split dává řízený střed a zachovává monotonii situačních floorů
(0.30/0.35 větve škálují stejně). Over-crowding zbytku hlídá tripwire v sekci 5.

Proč split přes `pickupSeen` (pořadí v listu), a ne přes nový field v `Macro`:
nulová změna datových struktur sdílených s expanzí/serializací; pořadí je
deterministické (jedna funkce, jeden push_back za druhým) a kontrakt přibíjí
regresní test T1. Dirichlet noise na root prior (trénink, α=0.3) se aplikuje AŽ
PO flooru+renormu na oba kandidáty nezávisle — beze změny sémantiky.

---

## 4. Regresní testy + protokol negativní kontroly

### Testy T1-T3 (`engine/tests/test_macro_actions.cpp`)

Vložit za `MacroActions.PickupNotAvailableWhenBallHeld` (končí :158), před
`MacroActions.BlockAvailableWithFavorableDice` (:160). Helper `countMacroType`
už v souboru existuje (:76-82). Ball v `makeMinimalState` je onGround {13,7}.

```diff
--- a/engine/tests/test_macro_actions.cpp
+++ b/engine/tests/test_macro_actions.cpp
@@ -158,6 +158,76 @@
     EXPECT_FALSE(hasMacroType(macros, MacroType::PICKUP));
 }
 
+// Regression for master-list item 7 (top-2 PICKUP pickers): with two
+// comparable pickers in reach, generation must emit BOTH, best-first.
+// The best-first ORDER is a contract the prior-floor split in
+// macro_mcts.cpp relies on -- the order assertions below pin it.
+// NEGATIVE CONTROL: pre-patch generation emits a single bestPicker, so
+// countMacroType == 1 and this test fails.
+TEST(MacroActions, PickupEmitsTopTwoPickersBestFirst) {
+    GameState state = makeMinimalState();  // ball on ground at {13,7}
+    // Player 1: dist 3 from ball, AG3 -> score 30 - 9 = 21 (secondary).
+    state.getPlayer(1).position = {10, 7};
+    // Player 2: dist 2 from ball, AG3 -> score 30 - 6 = 24 (primary).
+    Player& p2 = state.getPlayer(2);
+    p2.id = 2;
+    p2.teamSide = TeamSide::HOME;
+    p2.state = PlayerState::STANDING;
+    p2.position = {15, 7};
+    p2.stats = {6, 3, 3, 8};
+    p2.movementRemaining = 6;
+    p2.hasMoved = false;
+    p2.hasActed = false;
+
+    std::vector<Macro> macros;
+    getAvailableMacros(state, macros);
+
+    ASSERT_EQ(countMacroType(macros, MacroType::PICKUP), 2);
+    std::vector<int> pickerIds;
+    for (auto& m : macros) {
+        if (m.type == MacroType::PICKUP) pickerIds.push_back(m.playerId);
+    }
+    EXPECT_EQ(pickerIds[0], 2);  // higher score first (best-first contract)
+    EXPECT_EQ(pickerIds[1], 1);
+}
+
+// Guard (passes pre- and post-patch): a categorically worse second picker
+// (score gap > 15) must NOT be emitted -- no floored prior mass for a
+// picker that is strictly dominated on the pickup itself.
+TEST(MacroActions, PickupSecondPickerGatedByScoreGap) {
+    GameState state = makeMinimalState();
+    // Player 1: dist 1, AG4 -> score 40 - 3 = 37.
+    state.getPlayer(1).position = {12, 7};
+    state.getPlayer(1).stats = {6, 3, 4, 8};
+    // Player 2: dist 8 (reach 6+2 -- still eligible), AG2 -> 20 - 24 = -4.
+    // Gap 41 > 15 -> gated out.
+    Player& p2 = state.getPlayer(2);
+    p2.id = 2;
+    p2.teamSide = TeamSide::HOME;
+    p2.state = PlayerState::STANDING;
+    p2.position = {5, 7};
+    p2.stats = {6, 3, 2, 8};
+    p2.movementRemaining = 6;
+    p2.hasMoved = false;
+    p2.hasActed = false;
+
+    std::vector<Macro> macros;
+    getAvailableMacros(state, macros);
+
+    ASSERT_EQ(countMacroType(macros, MacroType::PICKUP), 1);
+    for (auto& m : macros) {
+        if (m.type == MacroType::PICKUP) EXPECT_EQ(m.playerId, 1);
+    }
+}
+
+// Guard (passes pre- and post-patch): a single eligible picker keeps
+// today's behavior bit-exactly -- one PICKUP macro, no phantom second.
+TEST(MacroActions, PickupSinglePickerUnchanged) {
+    GameState state = makeMinimalState();
+    state.getPlayer(1).position = {10, 7};   // dist 3, in reach
+    // (only home player near the ball; player 12 is AWAY at {20,7})
+
+    std::vector<Macro> macros;
+    getAvailableMacros(state, macros);
+
+    ASSERT_EQ(countMacroType(macros, MacroType::PICKUP), 1);
+}
+
 TEST(MacroActions, BlockAvailableWithFavorableDice) {
     GameState state = makeMinimalState();
```

Ruční ověření T1 proti kódu: oba hráči volní (`isFreeToAct`: STANDING, !hasMoved),
žádný enemy adjacent (AWAY 12 na {20,7}, dist ≥5 od obou) ⇒ žádný BLOCK; míč na
zemi ⇒ žádný ADVANCE/SCORE/CAGE; skóre 24 vs 21, gap 3 ≤ 15 ⇒ oba emitováni,
hráč 2 první.

### Test T4 (`engine/tests/test_macro_mcts.cpp`) — floor-split + renorm

Přesně přibíjí řešení rizika #1: post-renorm poměr priorů primární/sekundární
= 2.0 (renorm zachovává poměry, oba floory bindují při n≥11). Vyžaduje část 2
(observabilita). `PolicyNetwork` s nulovými vahami aktivuje heuristický
floor-blok stejně jako produkce (policy_cache trik, blend=0);
`dirichletAlpha` má default 0 ⇒ priory deterministické.

```cpp
// Item 7 prior-floor split: with two PICKUP candidates the secondary must
// carry HALF the primary's floor, so after renormalization the primary's
// root prior is exactly 2x the secondary's. Requires n >= 11 candidates so
// both floors (0.20 / 0.10) actually bind against the 1/n base.
// NEGATIVE CONTROL is two-stage (see protocol): before the generation
// patch this test fails at the ASSERT (no second PICKUP child exists);
// after generation but before the floor split it fails at EXPECT_NEAR
// with ratio == 1.0 -- the naive doubling this split is designed to avoid.
TEST(MacroMCTS, SecondaryPickupPriorIsHalfOfPrimary) {
    GameState state;
    state.phase = GamePhase::PLAY;
    state.activeTeam = TeamSide::HOME;
    state.half = 1;
    state.homeTeam.turnNumber = 2;   // turnsRemaining 7 > 3, scoreDiff 0 -> floor 0.20
    state.homeTeam.rerolls = 3;
    state.awayTeam.rerolls = 3;
    state.weather = Weather::NICE;
    state.ball = BallState::onGround({13, 7});

    // 10 free HOME players -> ~10 REPOSITION + 2 PICKUP + END_TURN, n >= 11.
    auto mk = [&](int id, Position pos) {
        Player& p = state.getPlayer(id);
        p.id = id;
        p.teamSide = TeamSide::HOME;
        p.state = PlayerState::STANDING;
        p.position = pos;
        p.stats = {6, 3, 3, 8};
        p.movementRemaining = 6;
        p.hasMoved = false;
        p.hasActed = false;
    };
    mk(1, {12, 7});                       // dist 1 -> score 27 (primary)
    mk(2, {10, 7});                       // dist 3 -> score 21 (secondary, gap 6)
    for (int i = 3; i <= 9; ++i) mk(i, {2, static_cast<int8_t>(2 * i - 5)});
    mk(10, {4, 7});                       // all i>=3: dist > MA+2 from the ball
    // One AWAY player far from everyone (no BLOCK/BLITZ/FOUL candidates).
    Player& away = state.getPlayer(12);
    away.id = 12;
    away.teamSide = TeamSide::AWAY;
    away.state = PlayerState::STANDING;
    away.position = {25, 13};
    away.stats = {6, 3, 3, 8};
    away.movementRemaining = 6;

    // Precondition: floors must bind (base 1/n < 0.10) and both pickers emit.
    std::vector<Macro> macros;
    getAvailableMacros(state, macros);
    ASSERT_GE(macros.size(), 11u);
    ASSERT_EQ(countMacroType(macros, MacroType::PICKUP), 2);

    PolicyNetwork pn;  // zero weights -- activates the heuristic floor block
    MCTSConfig cfg;
    cfg.policy = &pn;          // policyBlend stays 0.0 (production regime)
    cfg.maxIterations = 400;   // enough to visit every root child
    cfg.timeBudgetMs = 10000;
    MacroMCTSSearch search(nullptr, cfg, 42);
    search.search(state);

    float primary = -1.0f, secondary = -1.0f;
    for (const auto& cv : search.lastChildVisits()) {
        if (cv.macro.type != MacroType::PICKUP) continue;
        if (cv.macro.playerId == 1) primary = cv.prior;
        if (cv.macro.playerId == 2) secondary = cv.prior;
    }
    ASSERT_GT(primary, 0.0f);
    ASSERT_GT(secondary, 0.0f);
    EXPECT_NEAR(primary / secondary, 2.0f, 0.01f);
}
```

(Do `test_macro_mcts.cpp` podle potřeby doplnit include `bb/policy_network.h` a
lokální kopii/ekvivalent `countMacroType` — helper žije v anonymním namespacu
`test_macro_actions.cpp`.)

### Protokol negativní kontroly (třístupňový, rozšíření protokolu z `c020212`/`271579e`)

1. Aplikovat **jen testové diffy** (T1-T4), rebuild, spustit filtr
   `--gtest_filter='MacroActions.Pickup*:MacroMCTS.SecondaryPickup*'`:
   - T1 musí SELHAT (`countMacroType` **1**, ne 2);
   - T2, T3 musí PROJÍT (guardy dnešního chování);
   - T4 musí SELHAT na `ASSERT_EQ(countMacroType…, 2)` (druhý kandidát neexistuje).
2. Aplikovat **část 1 + část 2** (generace + observabilita), rebuild:
   - T1-T3 PASS;
   - **T4 musí SELHAT na `EXPECT_NEAR` s poměrem 1.0** — tento mezistav JE naivní
     zdvojení floored mass a jeho selhání empiricky dokazuje, že riziko #1 bez
     části 3 reálně nastává (měřená, ne jen papírová negativní kontrola).
3. Aplikovat **část 3** (floor-split), rebuild: T1-T4 PASS + celá suita zelená
   (aktuální plný počet, po dnešních 2a/2b fixech, + 4 nové; `BranchingFactorReasonable`
   musí zůstat zelený — přidáváme max. +1 kandidáta, limit <50 má rezervu).

---

## 5. Validační plán

Nespouštět, dokud běží trénink PID 2474 (žádný rebuild, žádné simulace).
Předpoklad: dnešní screen fixy 2a/2b + H2-kickoff fix jsou aplikované a
**změřené** (jejich okna uzavřená) — HEAD s nimi je referenční rameno.

### 5.1 Pořadí měření (závazné vůči master listu)

1. Dnes (mimo tento návrh): H2-kickoff fix + baseline, 2a samostatně, 2b nad 2a.
2. **Item 7**: paired-seed A/B, candidate = HEAD+item7, reference = HEAD
   (post-2b). **Vlastní okno — nesdílet s měřením itemu 9** (atribuce; master
   list). Item 9 je v referenci OBOU ramen, takže se neplete do delty.
3. Item 10 (prior-floor rebalance) VÝHRADNĚ až po vyhodnocení itemu 7 — sekvence
   z master listu trvá (byť floory itemů 7 a 10 nikdy nebindují na stejném uzlu,
   viz 6.1, atribuce na úrovni celé hry se slévá).

### 5.2 Rozhodující měření

**Paired-seed A/B přes `diag_utils.py`** (`a3f29dd`; cross-rebuild režim
`save_arm`/`load_arm`): N=150 párů, mirror konfigurace jako
`diag_fresh_baseline_20260710.py`, side-swap. Draw-rate delta <10pp z jednoho
N=150 = INCONCLUSIVE (závazná konvence [[feedback_draw_rate_noise_floor]]) —
**draw-rate je tady jen tripwire, ne cíl**. Primární metriky jsou eventové
(per-pair rozdíly, mnohem těsnější SE než W/D/L McNemar).

### 5.3 Metriky a prahy (ofenzivní fix)

Baseline čísla = mining 07-14 (`evidence/fable_replay_mining_20260714.md`);
mining A/B logů stejnou metodikou (vzor `mine_fresh.py`, featury f70-72/f12-15).

| Metrika | Baseline | Očekávání | Práh úspěchu | Red line |
|---|---|---|---|---|
| **Per-turn ground recovery** (primární) | 28.1 % | růst | **≥ +5pp** (párový test na per-game hodnotách) | pokles = regrese |
| Near-pickup miss (do 1 pole) | 82.1 % | pokles | ≤ 75 % | — |
| PICKUP zvolen v loose-ball-near rozhodnutích | 50.2 % | růst | ≥ 60 % | — |
| **TD/hru (mirror)** | z A/B ref. ramene | neklesá, ideálně roste | Δ ≥ 0 | výrazný pokles = regrese |
| Podíl 0-0 mirror her | 49.7 % (šum ±8-11pp) | pokles | směrový signál, ne verdikt | — |
| **Over-crowding tripwire**: PICKUP family visit-fraction na loose-ball uzlech | ~0.18 (nezvolený) | ~0.30-0.45 | — | **> 0.60** ⇒ PICKUP vytlačuje search (riziko #1 se projevilo i přes split) |
| Over-crowding doprovod: REPOSITION+BLITZ podíl na loose-ball rozhodnutích | 39.5 % + 12 % (globální mix) | mírný pokles | — | relativní propad > 50 % **bez** růstu recovery |
| Sanity | — | — | watchdog-skip 0/150, akce/hru bez >2× skoku, žádné MAX_ACTIONS | — |

Interpretační poznámka: recovery↑ + TD/hru↑ + 0-0↓ = jasný úspěch. Recovery↑ ale
TD/hru→0 a 0-0→beze změny = fix funguje mechanicky, ale bottleneck je dál po
řetězu (SCORE dostupnost — doporučení #2 z mining reportu, samostatný item).

### 5.4 Eskalační cesta (dokumentovaná, NEaplikovat spekulativně)

Pokud A/B ukáže recovery bez pohybu (< +5pp) a tripwiry čisté:
1. **f72 uncontested boost** (doplněk z miningu 07-02/07-14): situační zvýšení
   PICKUP flooru, když na míči není soupeřova TZ (čistý sběr má miss 73.9 % —
   search ho podvažuje nejkřiklavěji tam, kde je nejlevnější). Vlastní návrh +
   vlastní A/B.
2. **Top-3** s floorem 0.25× pro třetího kandidáta — až po (1), se stejným
   třístupňovým testovacím protokolem.

---

## 6. Interakční analýza

### 6.1 Item 10 (defenzivní prior-floor rebalance) — riziko #2 master listu

Floory itemů 7 a 10 se **nikdy nepotkají na stejném uzlu**: item 10 je celý
`onDef`-gated (REPOSITION 0.05→0.08, FOUL cap), `onDef` vyžaduje `ball.isHeld`;
PICKUP kandidáti existují jen při `ballOnGround` ⇒ `onDef=false`. Množiny uzlů
jsou disjunktní z konstrukce. Interakce zbývá jen na úrovni atribuce celoherních
metrik ⇒ sekvence z master listu (10 až po změření 7) stačí a trvá.

### 6.2 Item 9 / screen fix 2b (lane intercept)

2b vkládá REPOSITION cíle jen když `oppCarrierPtr != nullptr` (soupeř drží míč)
⇒ opět uzlově disjunktní s PICKUP uzly. Na loose-ball uzlu 2b nemění počet ani
typ kandidátů. Slévání efektů je čistě celoherní (lepší obrana ⇒ víc vyražených
míčů ⇒ víc loose-ball situací, kde item 7 působí) — proto vlastní měřicí okno
a reference ramene = post-2b HEAD.

### 6.3 Screen fix 2a (REPOSITION step-cap)

Expanzní změna, nemění kandidáty ani priory. Žádná mechanická interakce
s itemem 7. Kotvy: PICKUP blok (:434-475) je NAD insertion pointy 2a (:1085+)
i 2b (:513+, :642+) — po jejich aplikaci se čísla řádků PICKUP bloku nemění;
diffy části 1 sedí beze změn (a jsou content-anchored, `git apply --recount`
je fallback).

### 6.4 Dirichlet, progressive widening, leaf lookahead

- Dirichlet (trénink, α=0.3): aplikuje se na root priory PO floorech+renormu,
  per-dítě nezávisle — dva PICKUP kandidáti dostanou dva nezávislé tahy šumu,
  žádná změna sémantiky.
- `maxChildren` (progressive widening) je v produkci 0 = unlimited — kandidát
  navíc nikoho nevytlačí.
- `greedyMacroRank` (leafLookahead, default off): dva PICKUPy mají stejný rank
  40, remízu láme pořadí v listu ⇒ vyhrává primární — konzistentní s kontraktem.

### 6.5 Featurizace a training data

`extractMacroFeatures` rozliší dva PICKUP kandidáty jen přes `[12] player_strength`
(AG/vzdálenost se nefeaturizují, `[13] risk` je konstantní 0.33) — policy hlava
je k identitě pickera téměř slepá. Pro produkci (policy_blend=0, heuristické
floory) irelevantní; pro budoucí AZ bring-up **známá limitace**, kandidát na
samostatný follow-up (např. `[13]` = skutečná obtížnost sběru z AG+TZ), NEbundlovat.
Decision logy: dva PICKUPy zaberou max. 2 sloty v top-20 — mining metriky
(„PICKUP v top-20", „PICKUP zvolen") zůstávají srovnatelné; `lastChildVisits`
filtr `visits > 0` se nemění.

### 6.6 Expanze a replay

`expandPickup` (:1026) je čistě per-`playerId` (step-cap `movementRemaining+2`
z `2899cd5` + stall-aware advance z `a88f5e2` platí pro oba kandidáty shodně).
`replayToNode` sestupuje po ukazatelích, žádné párování maker podle typu —
duplicitní typ s jiným `playerId` je bezpečný. Jediné místo generace PICKUP
je :473 (ověřeno grepem) — žádná sibling cesta k opomenutí (lekce halfclock).

---

## 7. Shrnutí pro apply session

1. Po uzavření dnešního okna (2a/2b + H2 fix změřené) aplikovat ve 3 krocích
   s třístupňovou negativní kontrolou (sekce 4): testy → část 1+2 → část 3.
2. Jeden commit na část, nebo 1+2+3 v jednom commitu s testy — doporučuji
   **jeden commit** (části jsou funkčně jedna změna; mezistavy existují jen pro
   negativní kontrolu), message odkázat na tento dokument.
3. Paired-seed A/B N=150 dle 5.2/5.3, vlastní okno, mining metrik z A/B logů.
4. Verdikt podle recovery/TD/hru + tripwirů, NE podle draw-rate.
5. Při aktualizaci paměti: master list item 6 je aplikován (`ca297c9`), item 7
   → tento návrh.
