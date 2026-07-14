# Návrh patchů: SCORE-availability díra (povýšená priorita z re-miningu 07-14)

**Datum:** 2026-07-14 · **Autor:** Fable 5 (read-only agent)
**Zdroj nálezu:** `evidence/fable_replay_mining_20260714.md` (SCORE zvoleno 7.7 % can-score
stavů, 77 % not-in-top-20), historicky `evidence/fable_replay_mining_findings.md` §3 (07-02)
a `research_fable_20260709.md`; master apply-order `project_bloodbowl_fable_queue_priority_20260710`.

> ⚠ **Kontext vzniku:** Během psaní běžel trénink (PID 2474) a build ve worktree
> `bloodbowl-fixes-20260714`. Do enginu jsem NIC nezapisoval, nic nebuildil, nespouštěl
> žádné simulace. Všechny diffy jsou kotvené **obsahem na worktree
> `/home/jan/claude/bloodbowl-fixes-20260714/engine/src`** (= budoucí HEAD: obsahuje
> H2-kickoff fix, REPOSITION step-cap 2a, intercept lane 2b i blitz follow-up guard
> ca297c9). Čísla řádků platí pro worktree; při aplikaci na jiný stav usadí diff
> `git apply --recount` / `patch -F3` podle kontextu.

---

## 0. Ověření kotev (worktree = budoucí HEAD)

| Kotva | Worktree řádky | Ověřený obsah |
|---|---|---|
| SCORE generace (gate) | `macro_actions.cpp:151-158` | `if (iHaveBall && carrier->canAct()) { … maxReach = movementRemaining + 2 … }` |
| BLOCK generace | `macro_actions.cpp:415-432` | `state.forEachOnPitch(mySide, [&](const Player& att) { if (!att.canAct() \|\| att.hasActed) return; …` |
| FOUL generace | `macro_actions.cpp:492-509` | `if (!myTeam.foulUsedThisTurn) { … [&](const Player& fouler) …` |
| `expandBlitz` (výběr blitzera) | `macro_actions.cpp:927-958` | `int score = diceCount * 10 - dist;` |
| Heuristické prior floors | `macro_mcts.cpp:292-398` | blok `B) Compute heuristic priors`, SCORE-family case `:312-341` |
| Leaf eval / scoringBonus | `macro_mcts.cpp:483-693` | `simulate()`, clamp `:673` a `:692` |
| `canAct()` | `include/bb/player.h:40-42` | `STANDING && !hasActed && !lostTacklezones` |
| f59 featura | `feature_extractor.cpp:197,303-307` | `carrierMA = carrier.stats.movement; … (carrierMA + 2 >= carrierDistToTD)` |

Dnešní fixy (intercept lane +33 řádků na `:644+`) jsou NAD `expandBlitz`, takže kotvy
pod nimi už zahrnují posun. Patch A je v `macro_mcts.cpp` — **žádný čekající patch se
toho souboru nedotýká** (item 7 i screen fixy žijí v `macro_actions.cpp`).

---

## 1. Root-cause rozklad: proč search SCORE nevolí

Mechanismus má tři vrstvy + jednu korekci metodiky. Aritmetika z obou miningů
(decision-level, mirror):

| | 07-02 (2 026 dec.) | 07-14 (2 588 dec.) |
|---|---|---|
| SCORE zvoleno | 392 (19.3 %) | 198 (7.7 %) |
| SCORE v top-20, nezvoleno | 288 | 554 |
| **SCORE přítomno celkem** | **680 (33.6 %)** | **752 (29.1 %)** |
| **chosen \| present** | **57.6 %** | **26.3 %** |

Generační vrstva je tedy mezi miningy ~stejná (~30 % přítomnosti); **zkolabovala
podmíněná volba** (58 → 26 %). To odděluje vrstvy L1 (generace) a L2/L3 (search).

### L0 — korekce metodiky: f59 měří něco jiného než generátor

f59 (`carrier_can_score`) počítá s **plným MA** (`stats.movement + 2 >= dist`,
`feature_extractor.cpp:306`) a **ignoruje `hasActed`**. Decision záznamy nesou state
featury v okamžiku rozhodnutí, tj. i **uprostřed tahu**, kdy už carrier část pohybu
utratil nebo celou aktivaci spálil. Pro takové rozhodnutí SCORE legálně existovat
nemůže (hráč aktivuje 1×/tah) — a přesně tyhle stavy tvoří velkou část oněch „77 %
not-in-top-20". **Není to tedy celé generační bug**; z velké části je to artefakt
párování f59×decision. Důsledek pro mining rec. #2 z 07-14: „generovat SCORE i pro
carriera s částečným MA" je **už dnes splněno** — generace používá `movementRemaining`
(`:154`), ne plné MA. Skutečné díry jsou jinde (L1b, L2).

*(Doporučení pro příští mining: can-score podmínku pro decision-level počítat
z `movementRemaining` + proxy hasActed, ne z f59. Featuru samotnou NEměnit — je
vstupem natrénovaných 73-dim vah i PHP parity, viz §5.)*

### L1 — generace: kdy can-score stav reálně nemá SCORE kandidáta

SCORE se generuje jen když `carrier->canAct() && 0 < dist <= movementRemaining+2`
(`:152-155`). V průběhu tahu šanci **nenávratně spálí** (pro celý tah) tato cesta:

- **(L1b — jediná ovlivnitelná)** search vybere makro, jehož AKTÉREM je carrier,
  ale necílí na skórování: carrier jako **BLOCK útočník** (`:416` — smyčka carriera
  nevynechává), carrier jako **FOUL fouler** (`:494` — dtto), carrier jako **blitzer
  generického BLITZ makra** (`expandBlitz :941-952` vybírá čistě `dice*10 − dist`
  přes všechny, adjacentní carrier typicky vyhraje). Všechny nastaví `hasActed`
  (block/foul/blitz handlery) → `canAct()=false` → SCORE zmizí do konce tahu.
- Neutrálně (nejsou bug): CAGE hýbe jen spoluhráči (`expandCage :917` carriera
  explicitně vylučuje), REPOSITION carriera přeskakuje (`:547`), ADVANCE se
  v can-score stavu vůbec negeneruje (`:276` vyžaduje `dist > maxReach`); parciálně
  selhavší SCORE expanze invariant `dist ≤ remaining+2` zachovává (Čebyšev krok
  ubírá 1 dist i 1 movement), takže SCORE zůstává na stole.

### L2 — prior: SCORE má mid-game efektivní prior ≈ uniform ≈ šum

Heuristické floors (`macro_mcts.cpp:308-398`) jsou v produkci AKTIVNÍ (oba měřené
běhy: `Policy training: lr=0.01` → `use_policy=True` → `cfg.policy != nullptr`;
blend 0.0 vypíná jen sekci A, ne floors v sekci B). Ale pro SCORE-family platí
mid-game (turnsRemaining 5-8, vyrovnaný stav) jen **floor 0.08** (`:338`), a po
renormalizaci proti per-kandidátové mase ostatních maker (REPOSITION ~8-10 kandidátů
à 1/n, BLOCK à 0.12 za KAŽDÝ pár, CAGE 0.12…) klesne na **≈0.06** — tedy přesně
observovaná průměrná visit-fraction nezvoleného SCORE **0.065** (07-02: 0.061).
Je to stejná „per-candidate vs per-type mass" past, kterou už dokumentuje CAGE
komentář `:352-363`. Navíc **záměrné cappy**: první tah `maxPrior=0.05` (`:330`),
**vedení +1 při >2 zbývajících tazích `maxPrior=0.02`** (`:332`) — vedoucí tým má
skórování prakticky zakázané (relevantní pro konverzi 1-0 → 2-0).

### L3 — value: Q nerozlišuje „můžu skórovat" od „skóroval jsem"

`simulate()` (`:483-693`): stav „carrier bezpečně dojde" sbírá scoringBonus
(+0.4 safe walk-in, +0.25·proximity, +0.1 držení, pacing, cage-advance…) a leaf se
**clampuje na 1.0** (`:692`); stav „právě padl TD" má +0.5 score-diff a řádově
srovnatelný zbytek. ΔQ mezi SCORE-childem a stall-childem je proto ≈0 (nebo mírně
záporná, když SCORE expanze v open-loop replayi hodí kostkou fail) → **visits ≈ prior**
→ argmax visits SCORE mid-game prakticky nikdy nevybere. Odtud i příklady
END_TURN vf 0.50 vs SCORE 0.02. Tohle je známý „value-target flatness" root cause —
řeší ho linie mc_td_mix, **ne tento patch**; v flat-Q režimu je ale prior (L2)
správná a účinná páka.

---

## 2. Proč se to od 07-02 zhoršilo (19 % → 7.7 %)

Zhoršení sedí skoro celé do `chosen|present` (58 → 26 %), ne do generace. Srovnání
konfigurací obou zdrojových běhů (ověřeno z hlaviček logů):

| | betarun 06-30 (data 07-02) | Stage-1 mc_td_mix 07-13 (data 07-14) |
|---|---|---|
| epsilon | 0.35 → 0.10 | 0.35 → 0.10 (stejné) |
| policy | lr=0.01, blend=0.0 (floors aktivní) | stejné |
| **vf_blend** | **0.3 (ramp 10 epoch)** | **0 (řádek chybí — Stage-1 nulo-rizikový design)** |
| value target | mc_return | mc_td_mix |
| engine | před 6 obrannými fixy | po nich; H2-kickoff bug v datech |

Seřazené příspěvky:

1. **vf_blend 0.3 → 0**: v 07-02 datech byla natrénovaná value v Q (30 % blendu) a
   mohla SCORE-child odlišit; ve Stage-1 je Q čistě ruční heuristika, která je mezi
   score/stall plochá (L3, clamp) → volba spadla na prior (L2) → kolaps chosen|present.
   Tj. **část „zhoršení" je režim měření, ne regrese enginu** — 7.7 % vs 19 % nejsou
   plně srovnatelná čísla.
2. **Obranné fixy** (halfclock, kicking-team, stall-guard, …): can-score stavy jsou
   kontestovanější → SCORE expanze v in-tree replayích častěji failuje → nižší Q(SCORE).
   Konzistentní s kolapsem GFI konverze 58 → 31.5 % a f41 konverze 80 → 67.3 %.
3. **H2-kickoff bug** v datech Stage-1 (fixnut až dnes) — deformace druhých poločasů.

Podstatné: strukturní díry L1b+L2 jsou v kódu v OBOU režimech beze změny a i 07-02
úroveň (19 % chosen, vf 0.061) byla patologická. Gate fáze bez šumu má 59.3 % remíz
(nejvíc v historii) — zhoršení není jen mirror-noise.

---

## Patch A: prior floor pro safe walk-in SCORE (macro_mcts.cpp)

### Proč

Bezpečný walk-in (dist ≤ `movementRemaining`, žádné GFI) je nejblíž „free EV", jaké
ve hře existuje — a přesto má mid-game efektivní prior ≈ šum (L2). Zvednout floor
**jen pro direct SCORE s bezpečným dojezdem** na 0.30 (post-renorm ≈0.20-0.24, tj.
~3× dnešek a nejvyšší jednotlivý kandidát) nechává rizikové GFI skóry na starém
floors/Q arbitráži a **nedotýká se záměrných cappů** (první tah 0.05, vedení 0.02 —
cap se aplikuje až PO floors, `:383-390`, takže je nadřazený i novému floору).
Analogie: přesně takhle už kód řeší last-turn (`safeWalkIn ? 0.90 : 0.70`, `:322-323`)
— patch tentýž rozlišovací idiom rozšiřuje do mid-game s konzervativní hodnotou.

### 1. Unified diff (engine/src/macro_mcts.cpp, proti worktree)

```diff
--- a/engine/src/macro_mcts.cpp
+++ b/engine/src/macro_mcts.cpp
@@ -309,10 +309,22 @@
             float minPrior = 0.0f;
             float maxPrior = 1.0f;
             switch (macros[i].type) {
                 case MacroType::SCORE:
                 case MacroType::BLITZ_AND_SCORE:
                 case MacroType::HAND_OFF_SCORE:
                 case MacroType::PASS_SCORE:
                 case MacroType::CHAIN_SCORE: {
+                    // Direct SCORE with a safe walk-in (no GFI dice needed):
+                    // the generic mid-game floor below renormalizes to about
+                    // uniform (0.08 -> ~0.06 post-renorm = the observed 0.065
+                    // visit share, fable_replay_mining_20260714), so with the
+                    // flat leaf eval the search almost never converts even
+                    // free touchdowns (SCORE chosen in 7.7% of can-score
+                    // states). Computed here, applied after the branch chain
+                    // below; the deliberate first-turn/leading maxPrior caps
+                    // still win because caps are applied after floors.
+                    bool directSafeWalkIn = false;
+                    if (macros[i].type == MacroType::SCORE && macros[i].playerId > 0) {
+                        const Player& sp = state.getPlayer(macros[i].playerId);
+                        int sd = distToEndzone(sp.position, state.activeTeam);
+                        directSafeWalkIn =
+                            (sd > 0 && sd <= static_cast<int>(sp.movementRemaining));
+                    }
                     if (turnsRemaining <= 1) {
                         // One-turn TD: last turn, force scoring attempt
                         if (macros[i].playerId > 0) {
@@ -334,8 +346,11 @@
                     } else if (turnsRemaining <= 2) {
                         minPrior = 0.35f;
                     } else if (turnsRemaining <= 4) {
                         minPrior = 0.20f;
                     } else {
                         minPrior = 0.08f;
                     }
+                    if (directSafeWalkIn && minPrior < 0.30f) {
+                        minPrior = 0.30f;
+                    }
                     break;
                 }
```

Poznámky:
- `distToEndzone` i `state.getPlayer` se v témže case bloku už používají (`:320-321`)
  — žádné nové závislosti.
- Vnitřní `bool safeWalkIn` v turnsRemaining≤1 větvi zůstává (jiná sémantika: platí
  pro celou score-family; nový `directSafeWalkIn` jen pro direct SCORE). Jména se
  neliší jen náhodou — zabraňují shadow warningu.
- Ve větvích s capem (první tah, vedení) floor 0.30 sice minPrior zvedne, ale
  následný cap (`:387-390`) ho zase srazí na 0.05/0.02 → **chování capovaných větví
  se nemění** (post-renorm identické, protože pre-cap hodnota je v obou případech
  nad capem už dnes: uniform 1/n > 0.02).
- Sekce B běží jen při `config_.policy != nullptr` — to patch **záměrně nemění**
  (v produkci, gatingu i diag stacku je policy vždy nastavená: trénink `lr=0.01`,
  `diag_utils.py:52-54` předává `weights_policy.json`). Pozor ale při ad-hoc
  měřeních: **bez policy_weights se floors — a tedy i patch A — vůbec neprojeví.**

### 2. Regresní testy (engine/tests/test_macro_mcts.cpp)

Vložit za `TEST(MacroMCTS, ScoringPositionFindsScore)` (končí ~:188):

```cpp
// Regression for the mid-game safe-walk-in SCORE floor (proposals_score_
// availability_20260714): with the flat heuristic leaf eval, root visit
// shares track priors, so the 0.08 floor (~uniform post-renorm) meant a
// contested but safe walk-in was practically never chosen mid-game.
// NEGATIVE CONTROL: pre-patch this search picks CAGE/REPOSITION (SCORE
// prior ~1/n); post-patch SCORE (floor 0.30 -> highest single prior).
TEST(MacroMCTS, MidGameSafeWalkInPrefersScore) {
    GameState state;
    state.phase = GamePhase::PLAY;
    state.activeTeam = TeamSide::HOME;
    state.half = 1;
    state.homeTeam.turnNumber = 4;   // turnsRemaining = 5 -> generic branch
    state.homeTeam.rerolls = 3;
    state.awayTeam.rerolls = 3;
    state.weather = Weather::NICE;

    Player& carrier = state.getPlayer(1);
    carrier.id = 1;
    carrier.teamSide = TeamSide::HOME;
    carrier.state = PlayerState::STANDING;
    carrier.position = {20, 7};
    carrier.stats = {6, 3, 3, 8};
    carrier.movementRemaining = 6;   // dist 5 <= 6 -> safe walk-in
    state.ball = BallState::carried({20, 7}, 1);

    // Mild contest: one opponent marking the carrier keeps Q(SCORE) from
    // saturating the clamp on its own (see negative-control protocol).
    Player& marker = state.getPlayer(12);
    marker.id = 12;
    marker.teamSide = TeamSide::AWAY;
    marker.state = PlayerState::STANDING;
    marker.position = {20, 8};
    marker.stats = {6, 3, 3, 8};

    // Free teammates -> realistic candidate mass (CAGE + REPOSITIONs).
    for (int id = 2; id <= 5; ++id) {
        Player& p = state.getPlayer(id);
        p.id = id;
        p.teamSide = TeamSide::HOME;
        p.state = PlayerState::STANDING;
        p.position = {14, static_cast<int8_t>(2 * id - 1)}; // y = 3,5,7,9
        p.stats = {6, 3, 3, 8};
        p.movementRemaining = 6;
    }

    PolicyNetwork zeroPolicy;        // linear zeros -> uniform policy logits
    MCTSConfig config;
    config.timeBudgetMs = 0;
    config.maxIterations = 200;
    config.policy = &zeroPolicy;     // heuristic floors active (blend 0)
    config.policyBlend = 0.0f;

    MacroMCTSSearch search(nullptr, config, 42);
    Macro result = search.search(state);

    EXPECT_EQ(result.type, MacroType::SCORE);
}

// Guard (passes pre- AND post-patch): the deliberate "leading" cap
// (maxPrior 0.02 with >2 turns remaining) must survive the new floor --
// caps are applied after floors, so a leading team still stalls.
TEST(MacroMCTS, LeadingCapStillSuppressesScore) {
    // ...identical state as above, plus:
    //   state.homeTeam.score = 1;
    // then the same config/search; assert:
    //   EXPECT_NE(result.type, MacroType::SCORE);
}
```

**Negativní kontrola (protokol):** aplikovat NEJDŘÍV jen testový diff, rebuild,
`--gtest_filter='MacroMCTS.MidGameSafeWalkInPrefersScore'` → musí SELHAT
(pre-patch argmax = CAGE/REPOSITION). Pak engine diff → PASS; guard test musí
projít v OBOU stavech. ⚠ Search je deterministický (fixní seed 42, Dirichlet
default off), ale výslednou trajektorii jsem read-only nemohl spustit — pokud
pre-patch test projde (Q(SCORE) přes marker moc vysoká), přitvrdit kontest
(druhý marker na {21,8} → dodge při 2 TZ); pokud post-patch selže (Q(SCORE) moc
nízká vs prior), zvednout iterace na 400. Kalibrovat PŘED aplikací engine diffu.

### 3. Validační plán

Viz společný plán v §6 (primární metriky jsou pro A i B stejné).

---

## Patch B: carrier-activation guard (macro_actions.cpp)

### Proč

L1b: dokud je direct SCORE na stole, nemá žádné jiné makro utratit carrierovu
aktivaci na neskórovací akci — spálí šanci pro celý tah, aniž by to search
z plochého Q poznal. Guard **neodebírá týmu žádnou možnost**: BLOCK/FOUL kandidáti
ostatních hráčů se generují nezávisle dál, generický BLITZ si v `expandBlitz` vybere
jiného blitzera, a případ „blitz JE cesta ke skóre" pokrývá `BLITZ_AND_SCORE`
(preferuje ne-carrier blitzera a krok 2 = carrier skóruje). Je to věrná minimální
verze doporučení z obou miningů („zakázat makrům hýbat carrierem, dokud je SCORE
na stole") — ostatní pohybová makra (CAGE/REPOSITION/ADVANCE) carriera už dnes
nechávají být, zbývají přesně tyto tři aktérské díry.

Vědomé rozhodnutí: guard platí pro **jakýkoli** generovaný direct SCORE (i s GFI),
ne jen safe walk-in — carrier blokující/faulující v dosahu endzóny je špatná třída
tahů obecně a jednotné pravidlo je nejmenší sémantika. Riziko (vzácný stav, kdy je
carrier jediný příznivý útočník a tým o blok přijde) hlídá engagement-guard metrika.

### 1. Unified diff (engine/src/macro_actions.cpp, proti worktree, 4 hunky)

```diff
--- a/engine/src/macro_actions.cpp
+++ b/engine/src/macro_actions.cpp
@@ -148,13 +148,18 @@
     const Player* carrier = findCarrier(state);
     bool iHaveBall = (carrier != nullptr);
     bool ballOnGround = !state.ball.isHeld && state.ball.isOnPitch();
 
     // SCORE: carrier can reach endzone with MA + 2 GFI
+    bool directScoreAvailable = false;
     if (iHaveBall && carrier->canAct()) {
         int dist = distToEndzone(carrier->position, mySide);
         int maxReach = carrier->movementRemaining + 2; // +2 GFI
         if (dist <= maxReach && dist > 0) {
             out.push_back({MacroType::SCORE, carrier->id, -1, {-1, -1}});
+            directScoreAvailable = true;
         }
     }
@@ -415,6 +420,13 @@
     // BLOCK: favorable block (2+ dice, attacker chooses)
     state.forEachOnPitch(mySide, [&](const Player& att) {
         if (!att.canAct() || att.hasActed) return;
         if (att.hasSkill(SkillName::BallAndChain)) return;
+        // Carrier-activation guard: while a direct SCORE is on the table,
+        // blocking with the carrier burns the scoring activation for the
+        // whole turn (hasActed -> canAct() false at the SCORE gate above)
+        // and the flat leaf eval cannot see that cost. Teammates' block
+        // candidates are unaffected (proposals_score_availability_20260714,
+        // mechanism L1b; SCORE chosen in 7.7% of can-score states).
+        if (directScoreAvailable && att.id == carrier->id) return;
 
         auto adj = att.position.getAdjacent();
@@ -493,6 +505,9 @@
     if (!myTeam.foulUsedThisTurn) {
         state.forEachOnPitch(mySide, [&](const Player& fouler) {
             if (!fouler.canAct() || fouler.hasActed) return;
             if (fouler.hasSkill(SkillName::BallAndChain)) return;
+            // Same carrier-activation guard as BLOCK above: never spend the
+            // scoring carrier's activation on a foul.
+            if (directScoreAvailable && fouler.id == carrier->id) return;
 
             auto adj = fouler.position.getAdjacent();
@@ -927,10 +942,22 @@
 static MacroExpansionResult expandBlitz(GameState& state, const Macro& macro,
                                          DiceRollerBase& dice) {
     MacroExpansionResult result;
 
     const Player& target = state.getPlayer(macro.targetId);
 
+    // Expansion-side mirror of the generation guard: a generic BLITZ macro
+    // must not pick the carrier as blitzer while the carrier can still
+    // score directly this turn (the "blitz clears the path to the score"
+    // case is BLITZ_AND_SCORE's job, which already prefers a non-carrier
+    // blitzer and then walks the carrier in).
+    const Player* scorer = findCarrier(state);
+    bool carrierCanScore = false;
+    if (scorer && scorer->canAct()) {
+        int cd = distToEndzone(scorer->position, scorer->teamSide);
+        carrierCanScore = (cd > 0 && cd <= scorer->movementRemaining + 2);
+    }
+
     // Find best BLITZ action for this target (prefer more dice, closer blitzer)
     std::vector<Action> actions;
     getAvailableActions(state, actions);
 
     Action bestBlitzAction{};
     bool found = false;
     int bestScore = -999;
 
     for (auto& a : actions) {
         if (a.type != ActionType::BLITZ || a.targetId != macro.targetId) continue;
+        if (carrierCanScore && a.playerId == scorer->id) continue;
         const Player& blitzer = state.getPlayer(a.playerId);
```

Poznámky:
- Hunk 1: `carrier` je zaručeně nenulový, kdykoli `directScoreAvailable==true`
  (nastavuje se jen uvnitř `iHaveBall`), takže dereference v lambdách hunků 2-3
  je bezpečná.
- Hunk 4 (`expandBlitz`): přepočítává can-score čerstvě z aktuálního stavu — nutné,
  protože expanze běží open-loop v in-tree replayích na jiných stavech, než pro
  které se makro generovalo (idiom `expandScore` guardu z 39f689a). Krajní případ:
  carrier je jediný možný blitzer → `found=false` → prázdný result (promarněné
  rozhodnutí, stejná třída jako existující „can't blitz, abort" `:954`); search to
  vidí jako nízké Q, žádný hang.
- Generační pořadí: SCORE (`:151`) se generuje lexikálně PŘED BLOCK (`:415`)
  i FOUL (`:492`), flag je tedy v okamžiku použití vždy platný.

### 2. Regresní testy (engine/tests/test_macro_actions.cpp)

Vložit za `TEST(MacroActions, FoulNotAvailableWhenUsed)` (končí ~:230). Fixtury
`makeScoringState()` (carrier id1 {23,7}, MA6, dist 2) a `makeAdvanceState()`
(carrier {5,7}, dist 20) už existují.

```cpp
// Carrier-activation guard (proposals_score_availability_20260714, L1b):
// while a direct SCORE exists, the carrier must not be offered as BLOCK
// attacker -- but teammates' blocks stay.
// NEGATIVE CONTROL: pre-patch the BLOCK loop includes the carrier, so
// EXPECT_FALSE(carrierBlock) fails.
TEST(MacroActions, CarrierBlockSuppressedWhileScoreAvailable) {
    GameState state = makeScoringState();
    state.getPlayer(1).stats = {6, 4, 3, 8};      // ST4 -> favorable dice
    state.getPlayer(12).position = {23, 8};       // enemy adjacent to carrier

    Player& mate = state.getPlayer(2);
    mate.id = 2;
    mate.teamSide = TeamSide::HOME;
    mate.state = PlayerState::STANDING;
    mate.position = {22, 8};                      // adjacent to the enemy too
    mate.stats = {6, 4, 3, 8};
    mate.movementRemaining = 6;

    std::vector<Macro> macros;
    getAvailableMacros(state, macros);

    EXPECT_TRUE(hasMacroType(macros, MacroType::SCORE));
    bool carrierBlock = false, mateBlock = false;
    for (auto& m : macros) {
        if (m.type != MacroType::BLOCK) continue;
        if (m.playerId == 1) carrierBlock = true;
        if (m.playerId == 2) mateBlock = true;
    }
    EXPECT_FALSE(carrierBlock);
    EXPECT_TRUE(mateBlock);
}

// Same guard for FOUL. NEGATIVE CONTROL: pre-patch FOUL(carrier) exists.
TEST(MacroActions, CarrierFoulSuppressedWhileScoreAvailable) {
    GameState state = makeScoringState();
    state.getPlayer(12).position = {23, 8};
    state.getPlayer(12).state = PlayerState::PRONE;

    Player& mate = state.getPlayer(2);
    mate.id = 2;
    mate.teamSide = TeamSide::HOME;
    mate.state = PlayerState::STANDING;
    mate.position = {22, 8};
    mate.stats = {6, 3, 3, 8};
    mate.movementRemaining = 6;

    std::vector<Macro> macros;
    getAvailableMacros(state, macros);

    EXPECT_TRUE(hasMacroType(macros, MacroType::SCORE));
    bool carrierFoul = false, mateFoul = false;
    for (auto& m : macros) {
        if (m.type != MacroType::FOUL) continue;
        if (m.playerId == 1) carrierFoul = true;
        if (m.playerId == 2) mateFoul = true;
    }
    EXPECT_FALSE(carrierFoul);
    EXPECT_TRUE(mateFoul);
}

// Guard test (passes pre- AND post-patch): when the carrier CANNOT score,
// its blocks are offered exactly as before.
TEST(MacroActions, CarrierBlockAllowedWhenCannotScore) {
    GameState state = makeAdvanceState();         // dist 20 -> no SCORE
    state.getPlayer(1).stats = {6, 4, 3, 8};
    state.getPlayer(12).position = {5, 8};        // adjacent enemy

    std::vector<Macro> macros;
    getAvailableMacros(state, macros);

    EXPECT_FALSE(hasMacroType(macros, MacroType::SCORE));
    bool carrierBlock = false;
    for (auto& m : macros) {
        if (m.type == MacroType::BLOCK && m.playerId == 1) carrierBlock = true;
    }
    EXPECT_TRUE(carrierBlock);
}

// Expansion-side guard: a generic BLITZ must pick a non-carrier blitzer
// while the carrier can score. NEGATIVE CONTROL: pre-patch the carrier
// (better dice, ST4 + assist) wins the blitzer selection.
TEST(MacroExpansion, BlitzExpansionSkipsScoringCarrierAsBlitzer) {
    GameState state = makeScoringState();
    state.getPlayer(1).stats = {6, 4, 3, 8};      // carrier: best dice
    state.getPlayer(12).position = {22, 8};       // target adjacent to both

    Player& mate = state.getPlayer(2);
    mate.id = 2;
    mate.teamSide = TeamSide::HOME;
    mate.state = PlayerState::STANDING;
    mate.position = {21, 8};                      // adjacent to target
    mate.stats = {6, 3, 3, 8};
    mate.movementRemaining = 6;

    DiceRoller dice(42);
    Macro macro{MacroType::BLITZ, -1, 12, {-1, -1}};
    auto result = greedyExpandMacro(state, macro, dice);

    ASSERT_FALSE(result.actions.empty());
    for (auto& a : result.actions) {
        EXPECT_NE(a.playerId, 1);
    }
}
```

Ruční ověření scénářů proti kódu (worktree):
- B1: enemy {23,8} stojí adjacentně carrierovi {23,7} i mate {22,8}; carrier ST4
  (+ assist mate) vs ST3 → ≥2 kostky → pre-patch BLOCK(1) generován; SCORE: dist 2
  ≤ 6+2, canAct ✓. `carrierStuck` (1 TZ < 2) false → žádné HAND_OFF/PASS_SCORE šumy.
- B2: FOUL emituje jedno makro na foulera (`:506 return`) → pre-patch přesně
  FOUL(1) a FOUL(2).
- B4: oba blitzeři adjacentní targetu; carrier skóre `dice*10 − 1` s ST4+assist
  přebíjí mate (ST3) → pre-patch vybraný blitzer = carrier → assert selže.
  ⚠ Předpoklad: `getAvailableActions` emituje BLITZ akce pro oba adjacentní hráče
  (stejný vzor používá `BlitzAndScoreStopsFollowUpWhenBlockerSurfedOffPitch`);
  ověřit při kalibraci negativní kontroly.

**Negativní kontrola (protokol):** nejdřív jen testový diff, rebuild,
`--gtest_filter='MacroActions.Carrier*:MacroExpansion.BlitzExpansionSkips*'` →
B1/B2/B4 musí SELHAT přesně na uvedených EXPECTech, B3 (guard) projít. Pak engine
diff → vše PASS. Celá suita: `BranchingFactorReasonable` může klesnout o carrier-BLOCK
kandidáty v can-score stavech — test hlídá horní mez, pokles je v pořádku.

---

## 5. Zamítnuté varianty (a proč)

1. **Změna f59 na `movementRemaining`/hasActed** — f59 je vstup natrénovaných
   73-dim value vah a PHP feature parity (30539d6); změna sémantiky = tichá
   invalidace champion vah. Nesoulad řešit v miningu (L0), ne ve featuře.
2. **Generovat SCORE i po `hasActed`** — porušuje pravidla (1 aktivace/hráč/tah);
   expanze by stejně nic legálního nesložila.
3. **„SCORE-first ordering"** (doporučení 07-14) — v macro-MCTS neexistuje pořadí
   uvnitř tahu, které by šlo vynutit jinak než priory (=Patch A) nebo odebráním
   konkurentů (=Patch B); samostatný ordering mechanismus by byl větší zásah.
4. **Zvednout floor i pro GFI skóry / zrušit leading-cap 0.02** — širší sémantika
   s reálným EV trade-offem; leading-cap navíc záměrný stall design. Ponecháno jako
   follow-up páka **A2** (cap 0.02 → ~0.10 pro safe walk-in), zvážit až podle
   výsledků A — konverze 1-0 → 2-0 je druhý největší zdroj remíz (1-1).
5. **Úprava leaf evalu (clamp/score-diff váhy)** — to je value-side root cause (L3),
   patří do linie mc_td_mix, ne do minimálního SCORE-availability patche; interakční
   riziko s probíhajícím Stage-1/2 vyhodnocením.

---

## 6. Validační plán (společný pro A i B)

**Primární metrika = konverze šancí, NE draw-rate** (závazně: draw-rate delta <10pp
z jednoho N=150 je INCONCLUSIVE, `feedback_draw_rate_noise_floor`).

1. **Unit:** plný rebuild + celá gtest suita (aktuálně 404 testů) + negativní
   kontroly dle protokolů výše. Žádná simulace předem nutná.
2. **Decision-level sanity (levné, před dlouhým A/B):** krátká self-play dávka
   (1 epocha, 40 her, detached) + mining stejnou metodikou jako
   `mine_fresh.py` (07-14). Sledovat:
   - **SCORE chosen v can-score stavech** (correctly spočtených z movementRemaining,
     viz L0): 7.7 % → očekávám >30 % (Patch A samotný), safe-walk-in podmnožina >50 %.
   - podíl can-score rozhodnutí, kde SCORE není kandidátem (L1b): má klesnout po B.
3. **Rozhodující měření: paired-seed A/B** přes `diag_utils.py` (a3f29dd) — pozor,
   policy path (`weights_policy.json`) je pro aktivaci floors POVINNÁ (viz Patch A
   pozn.); diag_utils ji předává standardně.
   - **f41 šance/hru a f41 konverze** (turn-level mining z epoch logů):
     konverze 67.3 % → cíl ≥ 80 % (úroveň 07-02); GFI konverze jako sekundární.
   - **TD/hru v mirror** (dnes ~0.5 gólu/hru v gate) a **mirror 0-0 podíl**
     (49.7 %) — očekávaný POKLES remíz; jde o ofenzivní fix, směr opačný než
     u screen fixů.
   - **Guardrails:** benchmark vs random ≥ 93 %; conceded-TD/hru (dřívější skórování
     = víc kickoffů = víc soupeřových drivů — mírný nárůst OK, skok ne); engagement
     guard (bloky+blitzy/tah, červená >15-20 % relativní pokles — Patch B odebírá
     carrier-bloky, tým je má nahrazovat spoluhráči); watchdog-skip 0/150.
4. **Pořadí a atribuce:**
   - Aplikovat až PO doměření dnešních fixů (H2 kickoff + 2a/2b resetují baseline —
     nová baseline je nutná reference).
   - **A samostatně → změřit → B nad A → změřit.** A je větší páka a je
     v `macro_mcts.cpp` (nulový diff-konflikt s čímkoli čekajícím).
   - **Item 7 (PICKUP top-2/3) neměřit ve stejném okně** — sdílí prior-renorm pool
     (A zvedá SCORE masu → mírně ředí budoucí PICKUP floors a naopak; oba fixy jsou
     kompatibilní, jen se musí atribuovat sekvenčně). Doporučené pořadí: A → B →
     item 7 (ofenzivní řetěz „šanci vytvoř (7) → šanci nesahej (B) → šanci vezmi (A)").
   - **mc_td_mix Stage-2:** pokud by se zapínal vf_blend > 0, měřit A/B PŘED tím
     (vf_blend mění Q, floors mění priory — nesmí se slít do jednoho okna).

---

## 7. Očekávaný dopad (kvantitativní odhad z kódu)

Typický ofenzivní can-score uzel (n≈20; ~9 REPOSITION à 0.05, CAGE 0.12, 1-3 BLOCK
à 0.12, BLITZ 0.05, END_TURN 0.05→cap 0.10, SCORE 0.08): suma ≈ 1.15-1.35 →
SCORE dnes ≈ **0.06-0.07** (= observace 0.065). Po A: 0.30/1.4 ≈ **0.21-0.24** —
nejvyšší jednotlivý kandidát; ve flat-Q režimu (visits ∝ prior) se argmax překlápí
na SCORE ve většině safe-walk-in stavů. Po B navíc šance přežívá celý tah (carrier
nemůže být obětován na block/foul/blitz), takže i pozdní rozhodnutí v tahu mají
SCORE mezi kandidáty. Řetěz na remízy: f41 konverze ↑ → míň „kept_ball" a
„lost_ball" spálených šancí (0.61 ztrát/hru z držení) → mirror 0-0 ↓. Sekundární
smyčka: policy imitation se učí z MCTS visits — dnes SCORE = 0.5 % rozhodnutí
(policy se skórovat nikdy nenaučí), po patchi se smyčka otáčí pozitivně.
