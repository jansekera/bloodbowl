# Search-side proposal: make MCTS value scoring progress / reach a TD

**Author:** bb-mcts-search specialist · **Date:** 2026-06-25 (CEST) · **Status:** DESIGN ONLY — no training, no commits.

All claims cite `file:line` in the current tree. Engine NOT retrained for any number below; anything empirical is marked **NEEDS ENGINE RUN**.

---

## 0. Restatement of the structural bug (from code, not memory)

The macro search is **tree-expansion + static leaf eval, no rollout, no gamma**:

- Leaf eval is `MacroMCTSSearch::simulate()` — `engine/src/macro_mcts.cpp:346-529`. It returns a hand-coded heuristic, optionally blended with the value head (`macro_mcts.cpp:516-526`), plus a post-blend `scoringBonus` (`macro_mcts.cpp:528`).
- A TD only enters the value if `expand()` lands on a `TOUCHDOWN`/`GAME_OVER` state (`macro_mcts.cpp:180-185`), which from a pre-pickup root at 100 sims (depth ~1-2 macros) essentially never happens.
- There is **no `gamma`** anywhere in the C++ search and **no rollout to terminal** — `rolloutDepth` exists in the config (`bb/mcts.h:18`) but the macro search never calls a rollout; `nRollouts` (`bb/mcts.h:26`) only averages *open-loop leaf samples* (`macro_mcts.cpp:135-141`), it does not deepen.

So the **only** thing pulling the carrier toward the endzone is the `scoringBonus` term inside `simulate()`. If that term is too weak relative to the rest of the leaf value (score-diff, possession, player-count, defensive bonuses), the search has no gradient to actually advance-and-score, and symmetric strong play collapses to 0-0. Reward-side (fix#2) and feature-side (fix#3) levers are exhausted; the bottleneck is **inside the search's leaf value**.

A second, independent structural finding surfaced while reading the code (see Proposal #4): the **ADVANCE macro deliberately under-advances** early game (`macro_actions.cpp:773-780`), so even when the search *does* pick ADVANCE, the ball crawls forward at ~½ MA/turn.

---

## 1. Leaf-eval scoring term — **RECOMMEND (this is the #1 pick)**

### Current code (`engine/src/macro_mcts.cpp`)
Forward pull is accumulated into `scoringBonus` and added *after* the vf blend:

| line | term | magnitude |
|---|---|---|
| 359 | `heuristic += (my.score - opp.score) * 0.5` | **±0.5 per goal** (dominant, blended) |
| 374 | `scoringBonus += 0.25 * proximity` | 0..0.25, `proximity = 1 - dist/25` (`:369`) |
| 377-379 | safe walk-in TD this turn | +0.4 |
| 381-383 | GFI-reachable TD | +0.2 |
| 387-391 | stall pacing toward last-turn arrival | ≤0.1 |
| 394-396 | urgency (last 2 turns & near EZ) | +0.3 |
| 399-405 | one-turn TD on last turn | +0.5..0.8 |
| 528 | `return clamp(leaf + scoringBonus, -1, 1)` | |

### The defect, read from the numbers
The forward-pull that is **active on a normal mid-drive turn** (not last-2, not already in scoring range) is *only* line 374: `0.25 * proximity`. With `proximity = 1 - dist/25` (`:369`), moving the carrier one square forward changes the leaf by `0.25 * (1/25) = 0.01`. That **0.01/square gradient is swamped** by:

- the ±0.03/player count term (`:507`) — caging/screening one extra standing player is worth 3× a square of progress,
- the defensive marking/sideline/contain bonuses (`:446-496`, up to +0.24+0.10+0.12),
- and at `vfBlend>0`, by a value head that is flat-to-negative on scoring-frontier states (established fact).

So the search's leaf gradient *rationally prefers* to stand still and improve board position over advancing the ball — exactly the observed 0-0 passivity. All the *big* scoring bonuses (0.3-0.8) only switch on in the **last 1-2 turns or when already in MA+2 range** (`:394, :399, :377`), i.e. they reward *finishing* a drive but give almost nothing for *building* one. The drive never gets built, so the finishing terms never fire.

### Proposed change — a continuous, always-on advancement gradient that dominates the positional noise

Replace the single weak proximity term (`macro_mcts.cpp:374`) with a stronger, convex forward-progress term that (a) is meaningful every turn, (b) accelerates near the endzone, and (c) stays in `scoringBonus` so it is **never diluted by `vfBlend`** (preserving fix#1's invariant).

```cpp
//  macro_mcts.cpp  (inside the `carrier.teamSide == perspective` branch, replacing line 374)
//  proximity in [0,1], 1 == at endzone (defined :369)
//  Convex pull: ~0.45 at the goal line, ~0.11 at midfield, and crucially a
//  per-square gradient of ~0.45*2*proximity/25 that GROWS as we advance.
scoringBonus += 0.45 * proximity * proximity;   // was: 0.25 * proximity
```

Gradient at midfield (dist 12, proximity 0.52): `d/dx = 0.45 * 2 * 0.52 / 25 ≈ 0.019/square` — already ~2× the old term and ~comparable to one player-count unit; near the EZ (proximity 0.9) it is `≈0.032/square`, i.e. it **out-pulls** an extra standing player. The convex shape means progress is worth more the closer you get, which is the right incentive and matches BB stall-then-score play.

Optionally also widen the "building a drive" credit so it isn't a last-2-turns cliff — make the urgency term ramp instead of a step (`macro_mcts.cpp:394-396`):

```cpp
// soft urgency ramp (replaces the hard turnsLeft<=2 step at :394-396)
if (dist > 0 && dist <= ma + 2) {
    double readiness = 1.0;                          // in scoring range now
    scoringBonus += 0.30 * readiness;                // same magnitude, no turn gate
}
```

This makes "carrier is within striking distance" valuable on *every* turn, not only the last two — so the search will manoeuvre into scoring range and then the existing finish terms (`:399-405`) close it out.

- **Mechanism:** strengthens the only un-diluted forward signal so it dominates positional leaf noise on a normal turn.
- **Change site:** `engine/src/macro_mcts.cpp:374` (one-line swap) ± `:394-396` (optional ramp).
- **Expected effect on draw-rate:** highest of all candidates — directly attacks the measured gradient inversion. **NEEDS ENGINE RUN** to quantify.
- **Cost:** 1-3 lines, `cmake --build engine/build --target bb_engine_py`, **no retrain needed** for a first smoke (heuristic-only, `vfBlend=0`, champion weights untouched). A full run is only needed to confirm it holds under self-play/gating.
- **Risk:** low. It cannot dilute the value head (stays in `scoringBonus`, post-blend, `:528`). Worst case it over-pushes a lone carrier into a sack; the safe-walk-in/GFI gating on the *big* bonuses (`:377-405`) is unchanged, and the convex term is still bounded (≤0.45) well under the ±0.5 score term, so it never overrides "we're winning, don't fumble".

---

## 2. Limited rollout toward the endzone — **REJECT for first try (keep as fallback)**

**Mechanism:** at the leaf, instead of (or in addition to) the static heuristic, run a short, greedy, ball-carrier-only rollout that walks the carrier toward the endzone for K steps and returns +1 if it reaches a TD, else the static eval. This lets a reachable TD's +1 propagate back through `backpropagate()` (`macro_mcts.cpp:531-537`) without full game simulation.

**Why reject as the first move:**
- The infrastructure is dormant but **not wired**: `rollout()` is declared (`bb/mcts.h:74`) but the macro search never calls it; you'd add a real rollout loop inside `simulate()` and a `rolloutDepth` plumb-through. That is dozens of lines + new dice/turnover handling, vs the 1-line Proposal #1.
- A greedy carrier-only rollout largely **re-derives** what the heuristic already encodes (dist-to-EZ, GFI reach). The reason the heuristic fails is *magnitude/competition with positional terms*, not absence of TD knowledge — so a rollout would help only if it produced a clean +1 that out-weighs the noise, which Proposal #1 achieves far more cheaply.
- Cost: ~30-60 lines, rebuild, and it **slows every leaf** (each sim now does a multi-step greedy expansion). At 100 sims × 16 epochs this materially lengthens self-play/gating, and the engine already has a hang/robustness problem in gating (`fuzz_gate.py`, draw-collapse memo) that more in-leaf simulation could aggravate.

**Keep as fallback** only if Proposal #1's stronger gradient still doesn't convert builds into TDs in the smoke test — i.e. if the problem is genuinely "search can't *see* the multi-turn path", not "search under-values the path".

---

## 3. Gamma / deeper macro expansion — **REJECT**

**Gamma:** adding discounting changes nothing here because **there is no terminal +1 in the search to discount** — the leaf is a static heuristic, not a bootstrapped return (`macro_mcts.cpp:516-528`). Gamma lives only in the Python value target (`trainer.py`, per memo) and credit propagation there is already healthy. Adding gamma to the C++ backprop (`:531-537`) would discount a heuristic that has no temporal meaning. No effect on draw-rate.

**Deeper expansion:** the depth is ~1-2 macros because progressive widening + 100 sims spread thin over many root children; pushing depth so a TD leaf is reached would need either many more sims (cost) or a rollout (Proposal #2). Even at depth 3-4 the carrier is still mid-field from a pre-pickup root, so a TODO-leaf still won't appear. Deeper search *amplifies* whatever the leaf rewards — if the leaf still prefers standing still (today's bug), deeper search just stalls more confidently. So depth is a force-multiplier for Proposal #1, **not a fix on its own**. Reject as a standalone lever.

---

## 4. Macro availability (`macro_actions.cpp`) — **PARTIAL: one real defect (ADVANCE under-step), SCORE gate is fine**

**SCORE gate is correctly permissive.** SCORE is offered whenever `dist <= movementRemaining + 2` (MA+2 GFI) and `dist>0` (`macro_actions.cpp:151-157`). HAND_OFF/PASS/CHAIN_SCORE add fallbacks when the carrier is stuck (`:160-270`), and ADVANCE covers the "can't score this turn" case (`:272-279`). The search is **not** starved of the scoring move when a TD is actually reachable. So the SCORE-within-MA+2 gate is **not** the bottleneck — reject that sub-hypothesis.

**Real defect — ADVANCE deliberately crawls.** `expandAdvance` (`macro_actions.cpp:757-792`) caps the step at half the carrier's remaining movement on any non-final turn:

```
macro_actions.cpp:771  idealStepsThisTurn = ceil(dist / turnsRemaining)
macro_actions.cpp:775  maxSafe = max(1, mvRemaining / 2)      // <-- only HALF MA
macro_actions.cpp:776  steps   = min(idealStepsThisTurn, maxSafe)
macro_actions.cpp:778-780  (uses full MA only when turnsRemaining <= 2)
```

A MA6 carrier therefore advances at most **3 squares/turn** until the last two turns, and often fewer (`idealStepsThisTurn` for a stalling pace). Crossing a 25-wide pitch while also caging is barely feasible inside 8 turns — this structurally biases toward *holding* rather than *driving*, compounding the leaf-eval bug. This is a **macro-expansion** defect, not a leaf-eval one, and it is a clean second-order fix:

```cpp
// macro_actions.cpp:775  loosen the early-game cap from 1/2 to ~2/3 MA
int maxSafe = std::max(2, (mvRemaining * 2) / 3);   // was: max(1, mvRemaining/2)
```

- **Mechanism:** lets ADVANCE actually move the ball downfield, so the search's (now stronger) forward gradient translates into real distance closed.
- **Effect on draw-rate:** secondary but synergistic with #1; alone it just moves the ball faster without changing *whether* the search wants to. **NEEDS ENGINE RUN.**
- **Cost:** 1 line, rebuild, no retrain for smoke. **Risk:** moderate — advancing further exposes the carrier; that's why it should be tried *after/with* #1, not before, and the `mvRemaining/2` reserve was presumably there to keep movement for dodges. Keep it as the **second** one-change commit.

---

## 5. vf_blend interaction — design constraint, satisfied by #1

The dilution failure mode (established): a calibrated value head is flat/negative on scoring-frontier states, so `leaf = (1-blend)*heuristic + blend*vf` (`macro_mcts.cpp:523`) *erases* forward pull as `vfBlend` rises. Fix#1 already moved `scoringBonus` **after** the blend (`:528`), but the smoke showed draws were high even at `vfBlend=0`, proving the dominant problem is the *weakness of the term itself*, not the blend.

Proposal #1 keeps the strengthened gradient **entirely inside `scoringBonus`** (post-blend), so:
- it is `vfBlend`-invariant by construction — raising `vfBlend` later to get value-head generalization will **not** re-dilute scoring;
- it does not touch line 523, so the value head's contribution to *positional* judgement is preserved.

This is the key reason Proposal #1 is preferred over "just lower vfBlend": it fixes the gradient *and* leaves the value-blend lever usable.

---

## RANKED RECOMMENDATION

1. **#1 Leaf-eval forward gradient** (`macro_mcts.cpp:374`, 1 line) — cheapest, attacks the measured gradient inversion directly, `vfBlend`-safe, no retrain for smoke. **Do this first.**
2. **#4 ADVANCE under-step** (`macro_actions.cpp:775`, 1 line) — synergistic second commit if #1 helps but ball still crawls.
3. **#2 Bounded carrier rollout** — fallback only if #1+#4 still can't convert drives into TDs (i.e. if the issue is genuinely path-visibility, not magnitude).
4. Reject: **#3 gamma/depth** (nothing terminal to discount; depth amplifies whatever the leaf rewards). **SCORE-gate** is not the problem.

### #1 minimal diff sketch (the single cheapest decisive change)

```diff
--- a/engine/src/macro_mcts.cpp
+++ b/engine/src/macro_mcts.cpp
@@  (inside simulate(), carrier.teamSide == perspective branch, ~line 374)
             heuristic += 0.1;  // have ball (possession value — may be blended)
-            // fix #1: all offensive endzone/scoring pull below -> scoringBonus
-            scoringBonus += 0.25 * proximity;  // closer to endzone = better
+            // fix #1: all offensive endzone/scoring pull below -> scoringBonus
+            // search-fix: convex, always-on forward gradient. Per-square pull
+            // grows as the carrier nears the endzone (d/dx ~ 0.9*proximity/25),
+            // so on a normal turn advancing the ball out-weighs the positional
+            // leaf noise (player-count 0.03/unit, defensive bonuses up to 0.46)
+            // that otherwise rationally prefers standing still -> 0-0 collapse.
+            scoringBonus += 0.45 * proximity * proximity;  // was 0.25 * proximity
```

(Stays inside `scoringBonus`, so the post-blend add at `:528` keeps it `vfBlend`-invariant. One line of logic.)

---

## Measurement that confirms it BEFORE a full run

Script written: **`/home/jan/claude/bloodbowl/measure_advance_horizon.py`** — marked **NEEDS ENGINE RUN** (no numbers claimed).

It checks the two things that decide whether #1 is the right fix:

- **Q2 (decisive, cheap):** sweep a synthetic carrier from dist 24→0 and print `simulate()` leaf value at `vfBlend ∈ {0, 0.5}`. **Pass criterion:** with the patch, the leaf rises *monotonically and convexly* as dist→0 at vfBlend=0, and the per-square delta near the EZ exceeds one player-count unit (0.03). Today (unpatched) it should be ~flat (0.01/square). This is a pure leaf-eval check — needs only a `eval_leaf` read-only hook, no self-play, no retrain.
- **Q1/Q3 (context):** count how often a 100-sim search expands onto a TOUCHDOWN leaf (expect ~0, confirming "search can't see the TD") and how many squares ADVANCE actually moves a MA6 carrier (expect ~3, confirming the #4 under-step defect).

**Gate before any full training run:** after rebuild, run a short paired smoke (champion weights, inference-only, `vfBlend=0`, MCTS100, n≈32 paired games) and compare draw-rate vs the current build. If draws drop and TDs/game rise with the value head untouched, promote to a full self-play run. Per repo convention: one change per commit, **commit before training** (server does `git pull`), measure before concluding, and gate on draw-rate not just benchmark.

> Caveat (do not skip): the prior fix#1 smoke showed high draws even at `vfBlend=0`, so the *leaf-eval magnitude* hypothesis (this proposal) must be validated by Q2 *before* a full run — if Q2 shows the gradient is already steep and draws persist, pivot to Proposal #2 (rollout) rather than re-tuning constants blindly.
