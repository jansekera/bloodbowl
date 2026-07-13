# Návrhy patchů: screen=0 obranná díra — patch 2a (item 8) + patch 2b (item 9)

**Datum:** 2026-07-13 · **Autor:** Fable 5 (read-only agent)
**Zdroj nálezu:** `research_fable_20260709.md`, sekce 3b (screen=0 díra); master apply-order
z paměti `project_bloodbowl_fable_queue_priority_20260710`.

> ⚠ **Kontext vzniku:** V době psaní běžel detached A/B experiment, který přepíná
> `engine/src/macro_actions.cpp` mezi git verzemi a rebuildí. Do enginu jsem proto
> NIC nezapisoval, nic nebuildil a nespouštěl žádné herní pythony. Všechny diffy níže
> jsou kotvené na **committed HEAD blob `a88f5e2`** (`git show HEAD:engine/src/macro_actions.cpp`),
> NE na momentální stav working tree (ten se během session prokazatelně přepnul —
> jednu chvíli `git status` hlásil `M engine/src/macro_actions.cpp`, o pár minut
> později byl strom čistý). **Aplikovat až po doběhnutí A/B, na čistém checkoutu
> `a88f5e2` nebo novějším.**

---

## 0. Ověření kotev proti aktuálnímu HEAD (a88f5e2)

`git log --oneline -5 -- engine/src/macro_actions.cpp`:

```
a88f5e2 feat(engine): stall guard skips the throttle when the carrier is blitzable
39f689a fix(engine): end SCORE macro expansion on an off-pitch carrier instead of spinning
2899cd5 fix(engine): PICKUP move-to-ball step cap matches candidate-generation reach
2af4252 fix(macro): throttle post-pickup carrier movement to preserve cage window
b188b2a fix(tests): update stale test fixtures after NUM_FEATURES/NUM_ACTION_FEATURES expansions
```

Audit z 2026-07-09 běžel na `39f689a`; `a88f5e2` přidal ~18 řádků kolem
`carrierStallAwareSteps` (~:800), takže vše POD tím se posunulo o +18.
Přemapování kotev (staré → aktuální, ověřeno obsahem v HEAD blobu):

| Kotva | Audit (39f689a) | HEAD (a88f5e2) | Ověřený obsah |
|---|---|---|---|
| TZ-exkluze z REPOSITION | ~:548-559 | **:548-559** (beze změny, je nad :800) | `// Check if player is free (no adjacent enemies)` … `if (hasAdjacentEnemy) return;` |
| Vlajky strategií | ~:515-522 | **:513-522** | `int myEndzone = endzoneX(opponent(mySide));` … `int screenSlot = 0;` |
| Obranný řetěz strategií 1-4 | ~:643-671 | **:642-672** | `// Strategies 1-4: only when cage tag not used this iteration` … `} // end !usedCageTag` |
| `expandReposition` (hard-cap 4) | ~:1067-1072 | **:1085-1090** | `movePlayerToward(state, macro.playerId, macro.targetPos, dice, result, 4);` na :1088 |
| item 6 (follow-up smyčka blitz) | ~:947 | **:965** | `for (int step = 0; step < 12; ++step) {` — guard z itemu 6 tam stále NENÍ |
| item 7 (PICKUP generace, bestPicker) | ~:434-475 | **:434-475** (nad :800, beze změny) | `// PICKUP: ball on ground, best player by AG/distance/skills` |

Důležité stavové zjištění: **item 5 (stall-guard blitz-awareness) je už aplikován —
to JE commit `a88f5e2`** (včetně testů `AdvanceThrottlesCarrierWhenNoBlitzThreat` /
`AdvanceSprintsWhenCarrierIsBlitzable`). Item 6 aplikován není (ověřeno obsahem).
Master list v paměti (psaný 2026-07-10) má u itemu 5 stále „NEAPLIKOVÁNO" — je
zastaralý, při příští aktualizaci paměti opravit.

Vedlejší ověření: `expandReposition` je jediné místo, které hard-cap 4 používá;
nic jiného na 4 krocích nezávisí (feature `[14] positional_gain` pro REPOSITION je
konstanta `0.3f`, `macro_mcts.cpp` má jen cost `10` a defenzivní `minPrior 0.05` —
obojí nezávislé na step-capu).

---

## Patch 2a (item 8): REPOSITION step-cap 4 → skutečný movement budget

### Proč

`getAvailableMacros` rozdává REPOSITION cíle **bez jakékoli reach-kontroly**
(safety spot je klidně 10+ polí daleko, screen 5-8 polí), ale `expandReposition`
měl chůzi hardcodovanou na `maxSteps=4`. Každý hráč repositionující dál než 4 pole
se zastavil v půli cesty — **každý tah znovu** — takže screeny a safety pozice se
reálně nikdy nezformovaly. Stejná třída bugu jako PICKUP step-cap (`2899cd5`),
jehož idiom (komentář + `int maxSteps = …; movePlayerToward(…, maxSteps);`) tento
patch zrcadlí.

**Záměrná odchylka od `2899cd5`:** PICKUP dostal `movementRemaining + 2` (2 GFI),
protože jeho generace reach-kontrolu `MA+2` MÁ a cap ji musel dorovnat. REPOSITION
žádnou generovanou reach nemá, a hlavně je to **jediné dice-free makro** — GFI by
do něj zaneslo riziko pádu bez míče ve hře (čistý downside). Proto cap =
`movementRemaining` bez GFI headroomu. (Bonus: `movementRemaining=0` ⇒ smyčka
0 iterací ⇒ bezpečný no-op i při open-loop replay na divném stavu.)

### 1. Unified diff (engine/src/macro_actions.cpp, proti HEAD a88f5e2)

```diff
--- a/engine/src/macro_actions.cpp
+++ b/engine/src/macro_actions.cpp
@@ -1082,9 +1082,20 @@
     return result;
 }
 
 static MacroExpansionResult expandReposition(GameState& state, const Macro& macro,
                                               DiceRollerBase& dice) {
     MacroExpansionResult result;
-    movePlayerToward(state, macro.playerId, macro.targetPos, dice, result, 4);
+    // Candidate generation (getAvailableMacros) hands out REPOSITION targets
+    // with no reach check at all (safety/screen spots are routinely 5-10+
+    // squares away), but the walk here was hardcoded to maxSteps=4 -- any
+    // player repositioning farther than 4 squares stopped short every single
+    // turn, so defensive screens/safeties never actually formed
+    // (research_fable_20260709 section 3b; same bug class as the PICKUP step
+    // cap fix, 2899cd5). Cap at the player's real movement budget instead.
+    // Deliberately NO +2 GFI headroom (unlike expandPickup): REPOSITION is
+    // the only dice-free macro, and a failed GFI on a free repositioning
+    // player is pure downside with no ball at stake.
+    int maxSteps = state.getPlayer(macro.playerId).movementRemaining;
+    movePlayerToward(state, macro.playerId, macro.targetPos, dice, result, maxSteps);
     return result;
 }
 
```

(Kdyby se mezitím posunula čísla řádků — kontext je unikátní, `git apply --recount`
resp. `patch -F3` diff usadí podle obsahu.)

### 2. Regresní test (engine/tests/test_macro_actions.cpp)

Vložit za test `MacroExpansion.PickupMoveTowardBall` (končí na :460), před
`MacroExpansion.FoulProducesFoulAction` (:462):

```diff
--- a/engine/tests/test_macro_actions.cpp
+++ b/engine/tests/test_macro_actions.cpp
@@ -456,6 +456,34 @@
     for (auto& a : result.actions) {
         EXPECT_EQ(a.type, ActionType::MOVE);
         EXPECT_EQ(a.playerId, 1);
     }
 }
 
+// Regression for the REPOSITION step cap (research_fable_20260709 section
+// 3b, same bug class as the PICKUP step cap fix 2899cd5): candidate
+// generation hands out REPOSITION targets with no reach check, so the
+// expansion must walk the player's real movement budget, not a fixed 4.
+// NEGATIVE CONTROL: pre-patch expandReposition hard-caps the walk at 4
+// steps, so this MA8 player stops at x=14 and the last three EXPECTs
+// below fail (actions.size()==4, position=={14,7}).
+TEST(MacroExpansion, RepositionWalksFullMovementBudget) {
+    GameState state = makeMinimalState();
+    Player& p1 = state.getPlayer(1);
+    p1.position = {10, 7};
+    p1.stats = {8, 3, 3, 8};
+    p1.movementRemaining = 8;
+    // Ball held by the far-away opponent so no PICKUP/loose-ball logic runs.
+    state.getPlayer(12).position = {22, 11};
+    state.ball = BallState::carried({22, 11}, 12);
+
+    DiceRoller dice(42);
+    // Screen spot 7 squares away across an open lane (no TZ, no GFI needed).
+    Macro macro{MacroType::REPOSITION, 1, -1, {17, 7}};
+    auto result = greedyExpandMacro(state, macro, dice);
+
+    EXPECT_FALSE(result.turnover);
+    EXPECT_EQ(result.actions.size(), 7u);
+    EXPECT_EQ(state.getPlayer(1).position.x, 17);
+    EXPECT_EQ(state.getPlayer(1).position.y, 7);
+}
+
 TEST(MacroExpansion, FoulProducesFoulAction) {
     GameState state = makeMinimalState();
```

**Proč je počet kroků deterministický:** `distanceTo` je Čebyšev; ve volném poli
každý vybraný MOVE (min. skóre = min. vzdálenost) sníží vzdálenost přesně o 1,
takže cesta {10,7}→{17,7} = přesně 7 akcí a příchod na cíl; s MA8 žádné GFI, žádné
kostky (`turnover=false` garantováno). Soupeř na {22,11} nemá TZ nikde na trase.

**Negativní kontrola (protokol):** aplikovat NEJDŘÍV jen testový diff, rebuild,
`./engine/build/bb_tests --gtest_filter='MacroExpansion.RepositionWalksFullMovementBudget'`
→ musí SELHAT s `actions.size()` **4** (ne 7) a `position.x` **14** (ne 17).
Pak aplikovat engine diff → PASS. (Stejný protokol jako u `c020212`/`271579e`.)

### 3. Validační plán

Nespouštět, dokud běží A/B experiment (CPU saturováno). Pak:

1. **Unit:** `cmake --build engine/build -j` + celá suita `./engine/build/bb_tests`
   — očekávat vše zelené (aktuální plný počet + 1 nový test); negativní kontrola dle
   protokolu výše.
2. **Rozhodující měření: paired-seed A/B** přes `diag_utils.py` (commit `a3f29dd`),
   candidate = HEAD+2a vs. reference = HEAD, mirror konfigurace jako
   `diag_fresh_baseline_20260710.py`. Per závazná konvence
   (`feedback_draw_rate_noise_floor`): jednorámkové N=150 s deltou <10pp je
   INCONCLUSIVE — nepoužívat jako verdikt.
3. **Metriky (obranný fix ⇒ NEsoudit podle draw-rate):**
   - **conceded-TD/g** (champion v obraně) — primární; proti scoring probe
     (`diag_vs_scorer.py`, greedy scorer) i v mirroru.
   - **REPOSITION arrival-rate**: podíl REPOSITION expanzí, kde hráč skončí na
     `targetPos` (mining z replay akcí; pre-patch bude u cílů >4 pole ~0 %) —
     nejpřímější signál, že fix dělá, co má.
   - **Draw rate jen jako tripwire, ne cíl: NÁRŮST remíz je u obranného fixu
     OČEKÁVANÝ výsledek, ne regrese.**
   - Sanity: watchdog-skip 0/150, žádné MAX_ACTIONS red flagy. Mírný nárůst
     akcí/hru je očekávaný (REPOSITION teď generuje až MA MOVE akcí místo 4);
     červená až při >2× skoku.
4. **Interakce:** 2a nemění počet ani typ kandidátů ⇒ žádný dopad na prior-renorm
   pool. Zvyšuje ale účinnost patche 2b (interceptor dál než 4 pole by jinak nikdy
   nedošel) ⇒ **měřit 2a samostatně PŘED 2b**, jinak se efekty slijí.

---

## Patch 2b (item 9): „Strategy 0.5" — intercept-lane targeting v obranném řetězu

### Proč a návrh geometrie

Dnešní obranný řetěz (safety y=7 → marker → endzone guard y∈{5,9} → screen
y∈{3,5,7,9,11}) pokrývá jen **fixní Y**. Nosič sprintující po křídle (y=1-2 /
12-13) nemá v lajně nikdy nikoho — to je jádro „screen=0" díry. Strategy 0.5 pošle
**jednoho** obránce na levný interceptní bod v **reálné Y lajně nosiče**:

- **X:** `laneX = (carrier.x + myEndzone) / 2` — půlka mezi nosičem a bráněnou
  endzónou. Goal-side z konstrukce, stejný vzorec jako už používá Strategy 4 pro
  `screenX` (žádný pathfinding, jen bodová aritmetika). Clamp 1..24 (idiom
  Strategy 3/4).
- **Y:** `laneY = clamp(carrier.y, 1, 13)` — skutečná lajna nosiče; clamp mimo
  krajní řady zrcadlí „avoid sidelines" idiom z `expandScore` (a `scoreMoveAction`
  stejně penalizuje y≤1/y≥13, takže cíl přímo na y=1/13 by se špatně docházel).
- **Gate — kdo interceptuje:** jen obránce, který (a) je **goal-side** nosiče
  s tolerancí 2 pole na stažení se zpět: `(p.x − carrier.x) · dxOpp ≥ −2`, kde
  `dxOpp = forwardDx(opponent(mySide))` je směr útoku nosiče; (b) na bod dosáhne
  do dvou aktivací: `p.distanceTo(lane) ≤ p.stats.movement * 2`. Kdo gate neprojde,
  propadne beze změny do Strategies 1-4. MCTS pak arbitruje, jestli se makro použije.
- **Priorita:** před Strategy 1 (safety) uvnitř `!usedCageTag` — cage-tag (Strategy 0)
  zůstává nejvyšší. Strukturálně zrcadlí přesně idiom Strategy 0
  (`bool usedIntercept` + obalení zbytku do `if (!usedIntercept) { … }`, bez
  re-indentace — soubor uvnitř `!usedCageTag` stejně neindentuje).

**Vědomá změna tvaru obrany:** první rychlý volný obránce dřív bral safety spot
{myEndzone,7}; teď vezme intercept (což JE lane-aware safety) a fixní safety
připadne až dalšímu volnému hráči. Při jediném volném obránci tedy fixní safety
nevznikne — to je zamýšlené (lajna nosiče > prázdný střed).

### 1. Unified diff (engine/src/macro_actions.cpp, proti HEAD a88f5e2, 3 hunky)

```diff
--- a/engine/src/macro_actions.cpp
+++ b/engine/src/macro_actions.cpp
@@ -513,7 +513,8 @@
     int myEndzone = endzoneX(opponent(mySide));  // our own endzone to defend
     bool onDefense = !iHaveBall && !ballOnGround;
     bool receiverPlaced = false;
     bool hunterPlaced = false;
     bool cageTagPlaced = false;
+    bool interceptPlaced = false;
     bool safetyPlaced = false;
     bool markerPlaced = false;
@@ -639,7 +640,38 @@
                     }
                 }
             }
-            // Strategies 1-4: only when cage tag not used this iteration
+            // Strategies 0.5-4: only when cage tag not used this iteration
             if (!usedCageTag) {
+            // Strategy 0.5: Intercept lane -- put a defender between the
+            // carrier and the defended endzone in the carrier's ACTUAL Y
+            // lane. The fixed safety spot (y=7) and the fixed screen Ys
+            // {3,5,7,9,11} never cover a carrier sprinting the flank
+            // (y=1-2 / 12-13), so nothing ever actually stood in his lane
+            // (research_fable_20260709 section 3b, "screen=0" hole). One
+            // interceptor per generation pass; cheap point geometry only,
+            // no pathfinding -- MCTS arbitrates whether the macro is used.
+            bool usedIntercept = false;
+            if (!interceptPlaced && oppCarrierPtr != nullptr) {
+                int dxOpp = forwardDx(opponent(mySide)); // carrier's attack direction
+                // Intercept point: halfway between the carrier and the
+                // defended endzone (same X idiom as Strategy 4's screenX),
+                // in the carrier's own lane, clamped off the sidelines.
+                int laneX = (oppCarrierPtr->position.x + myEndzone) / 2;
+                int laneY = std::clamp(static_cast<int>(oppCarrierPtr->position.y), 1, 13);
+                Position lane{static_cast<int8_t>(std::clamp(laneX, 1, 24)),
+                              static_cast<int8_t>(laneY)};
+                // Gate: only a defender who is goal-side of the carrier
+                // (2-square slack to cut back in) and can reach the lane
+                // within two activations commits to it; everyone else
+                // falls through to Strategies 1-4 unchanged.
+                bool goalSide =
+                    (p.position.x - oppCarrierPtr->position.x) * dxOpp >= -2;
+                if (goalSide && p.position.distanceTo(lane) <= p.stats.movement * 2) {
+                    target = lane;
+                    interceptPlaced = true;
+                    usedIntercept = true;
+                }
+            }
+            if (!usedIntercept) {
             // Strategy 1: Safety player (fast, near our endzone)
             if (!safetyPlaced && p.stats.movement >= 6) {
@@ -667,7 +699,8 @@
                 int screenY = screenYs[screenSlot % 5];
                 screenSlot++;
                 target = {static_cast<int8_t>(std::clamp(screenX, 1, 24)),
                           static_cast<int8_t>(screenY)};
             }
+            } // end !usedIntercept
             } // end !usedCageTag
         } else {
```

Poznámky k diffu:
- Kontextové řádky jsou doslovné vč. em-dash „—" v komentářích Strategy 2/3/4
  (soubor je má, nové komentáře používají „--" per styl `2899cd5`).
- Vše potřebné je v scope: `opponent()`, `forwardDx()`, `oppCarrierPtr`,
  `myEndzone`, `p`; `<algorithm>` (pro `std::clamp`) už soubor includuje.
- Cíl může být obsazené pole — stejně jako u Strategy 2 (marker cílí přímo na pole
  nosiče); `movePlayerToward` prostě dojde co nejblíž. Existující chování.

### 2. Regresní testy (engine/tests/test_macro_actions.cpp)

Vložit za `MacroActions.RepositionNotForEngagedPlayer` (končí na :285), před
`MacroActions.PassAvailableWithTeammateAhead` (:287):

```diff
--- a/engine/tests/test_macro_actions.cpp
+++ b/engine/tests/test_macro_actions.cpp
@@ -283,5 +283,59 @@
     }
     EXPECT_FALSE(hasRepoForP1);
 }
 
+// Regression for Strategy 0.5 (intercept lane, research_fable_20260709
+// section 3b): on defense the first goal-side defender must be sent into
+// the carrier's ACTUAL Y lane, between the carrier and the defended
+// endzone -- not to a fixed-Y spot.
+// NEGATIVE CONTROL: pre-patch the defensive chain can only emit fixed Ys
+// (safety y=7, guards y=5/9, screen y in {3,5,7,9,11}) or the carrier's
+// own square as marker ({8,2} here); the intercept point {4,2} is emitted
+// by nothing, and this defender gets the safety spot {0,7} instead -- the
+// EXPECT_TRUE below fails without the patch.
+TEST(MacroActions, DefensiveRepositionTargetsCarrierLane) {
+    GameState state = makeMinimalState();
+    // AWAY carrier sprinting the flank at y=2, attacking toward x=0.
+    state.getPlayer(12).position = {8, 2};
+    state.ball = BallState::carried({8, 2}, 12);
+    // Free HOME defender (MA6), goal-side of the carrier, off the lane.
+    state.getPlayer(1).position = {5, 7};
+
+    std::vector<Macro> macros;
+    getAvailableMacros(state, macros);
+
+    // Intercept point = {(8 + 0) / 2, clamp(2, 1, 13)} = {4, 2}.
+    bool hasLaneIntercept = false;
+    for (auto& m : macros) {
+        if (m.type == MacroType::REPOSITION && m.playerId == 1 &&
+            m.targetPos.x == 4 && m.targetPos.y == 2) {
+            hasLaneIntercept = true;
+        }
+    }
+    EXPECT_TRUE(hasLaneIntercept);
+}
+
+// Guard (passes pre- and post-patch): a defender already beaten by the
+// carrier (not goal-side) must NOT chase the intercept lane; it falls
+// through to Strategy 1 (safety) exactly as before the patch.
+TEST(MacroActions, DefensiveRepositionInterceptRequiresGoalSide) {
+    GameState state = makeMinimalState();
+    // Same flank carrier, but the defender starts 4 squares BEHIND the
+    // play (carrier attacks toward x=0, defender at x=12): not goal-side.
+    state.getPlayer(12).position = {8, 2};
+    state.ball = BallState::carried({8, 2}, 12);
+    state.getPlayer(1).position = {12, 7};
+
+    std::vector<Macro> macros;
+    getAvailableMacros(state, macros);
+
+    bool hasSafety = false;
+    for (auto& m : macros) {
+        if (m.type != MacroType::REPOSITION || m.playerId != 1) continue;
+        EXPECT_NE(m.targetPos.y, 2);
+        if (m.targetPos.x == 0 && m.targetPos.y == 7) hasSafety = true;
+    }
+    EXPECT_TRUE(hasSafety);
+}
+
 TEST(MacroActions, PassAvailableWithTeammateAhead) {
     GameState state = makeMinimalState();
```

Ruční ověření scénářů proti kódu (HEAD):
- Test 1: activeTeam=HOME, míč drží AWAY hráč 12 ⇒ `findCarrier`=nullptr ⇒
  `iHaveBall=false`, `ballOnGround=false` ⇒ `onDefense=true`;
  `myEndzone=endzoneX(AWAY)=0`; hráč 1 na {5,7} je volný (žádný soupeř adjacent).
  Gate: `dxOpp=forwardDx(AWAY)=-1`, `(5−8)·(−1)=3 ≥ −2` ✓;
  `distanceTo({4,2}) = max(1,5) = 5 ≤ 6·2` ✓ ⇒ target {4,2}.
  Pre-patch: Strategy 1 (MA6≥6) ⇒ {0,7} ⇒ EXPECT_TRUE selže. Jediný soupeř =
  nosič ⇒ cage-tag (potřebuje ≥2 adjacenty) nevystřelí.
- Test 2: `(12−8)·(−1) = −4 < −2` ⇒ gate zamítne ⇒ Strategy 1 ⇒ {0,7} (pre- i
  post-patch stejné; test přibíjí goal-side gate).

**Negativní kontrola (protokol):** aplikovat nejdřív jen testový diff, rebuild,
`--gtest_filter='MacroActions.DefensiveReposition*'` → `DefensiveRepositionTargetsCarrierLane`
musí SELHAT (macro pro hráče 1 má targetPos {0,7}, ne {4,2});
`DefensiveRepositionInterceptRequiresGoalSide` projde už pre-patch (guard).
Pak aplikovat engine diff → oba PASS.

### 3. Validační plán

1. **Unit:** celá gtest suita + negativní kontrola dle protokolu; 2b nemění počet
   maker (pořád 1 REPOSITION na volného hráče), takže `BranchingFactorReasonable`
   musí zůstat zelený beze změn.
2. **Paired-seed A/B** (`diag_utils.py`, `a3f29dd`): candidate = HEAD+2a+2b vs.
   reference = HEAD+2a (tj. **měřit 2b nad již změřeným 2a**, ne nad holým HEAD —
   bez 2a interceptor dál než 4 pole nikdy nedojde a efekt 2b se podměří).
3. **Metriky — je to OBRANNÝ fix:**
   - **Draw rate smí VZRŮST a je to očekávané, ne regrese** (champion v mirroru
     už teď skoro neinkasuje; lepší obrana v self-play tlačí k 0:0). Jednorázová
     delta <10pp z N=150 = INCONCLUSIVE (závazná konvence).
   - **conceded-TD/g v obraně** proti scoring probe (`diag_vs_scorer.py` greedy) —
     primární úspěchová metrika.
   - **Lane-coverage**: podíl soupeřových carrier-tahů, kdy má obrana aspoň
     jednoho stojícího hráče goal-side v |Δy|≤1 od lajny nosiče — mining z replayů
     (navrhovaný read-only skript `diag_lane_coverage.py`, psát až po A/B; vzor
     `evidence/fable_replay_mining_findings.md` tooling). Očekávání: znatelný
     nárůst hlavně pro nosiče na y∈{1,2,12,13}.
   - **Engagement guard** (převzato z validačního plánu itemu 10, platí už tady):
     bloky+blitzy na obranný tah; **červená linie >15-20% relativní pokles** =
     patologická pasivita. Důvod: REPOSITION je jediné dice-free makro; posílení
     jeho užitečnosti může krmit stalling — a JAK zamýšlený efekt, TAK patologie
     snižují conceded-TD, takže conceded-TD sám o sobě úspěch od patologie nerozliší.
   - Sanity tripwiry: watchdog-skip 0/150, žádné MAX_ACTIONS red flagy.
4. **⚠ Interakční riziko (renorm pool):** 2b vkládá lane-intercept cíle do téhož
   REPOSITION kandidátního/prior-renorm poolu, kterého se týkají **item 7**
   (PICKUP top-2/3 — přidává kandidáty, mění renormalizaci všech priorů) a
   **item 10** (defenzivní prior-floor rebalance 0.05→0.08 + FOUL cap). Závazná
   sekvence z master listu: **item 10 aplikovat VÝHRADNĚ až PO změření 8+9**;
   item 7 neměřit ve stejném okně jako 9 (atribuce by se slila).

---

## 4. Pořadí aplikace (cross-patch conflict check)

Master pořadí uvnitř `macro_actions.cpp` je **zdola nahoru: 8 → 6 → 5 → 9 → 7**.
Stav k HEAD `a88f5e2`:

| Item | Stav | Kotva v HEAD |
|---|---|---|
| **5** stall-guard blitz-awareness | **UŽ APLIKOVÁN** = commit `a88f5e2` | — |
| **8** = patch 2a | tento návrh | `:1085-1090` (nejníž v souboru) |
| **6** blitz follow-up isOnPitch guard | neaplikován (ověřeno) | smyčka `for (int step = 0; step < 12; ++step)` na `:965` |
| **9** = patch 2b | tento návrh | `:513-522` + `:642-673` |
| **7** PICKUP top-2/3 kandidáti | neaplikován | `:434-475` (nejvýš) |

**Efektivní zbývající pořadí: 8 → 6 → 9 → 7** (item 5 z řetězu vypadl aplikací).

Co se posune při porušení pořadí (všechny diffy jsou content-anchored, takže
`git apply` s malým fuzz/`--recount` projde, jen @@ čísla přestanou sedět):
- **9 před 8**: 2b přidává 33 řádků nad `expandReposition` (+1 vlajka, +31 řetěz,
  +1 uzávěr) ⇒ hunk 2a se posune z `:1082` na ~`:1115`.
- **6 před 8**: item 6 je 1-2 řádky na `:965` ⇒ hunk 2a se posune o +1/+2.
- **8 první** (doporučeno): je pod vším ostatním ⇒ nic se neposouvá a všechny
  ostatní diffy sedí přesně.
- Pozn. z master listu o „item 5 hunk se posune o +1 po itemu 6" je už bezpředmětná
  (5 aplikován dřív než 6).

Měřicí pořadí zůstává: **2a změřit samostatně, pak 2b nad 2a** (viz plány výše);
6 lze přibalit k nejbližšímu N=150 tripwiru bez vlastního diag skriptu (dle
master listu), 7 a 10 až po vyhodnocení 8+9.
