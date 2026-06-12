# Full-Code Review: MCTS + Feature Extraction Layer

**Verdict: ONE CATASTROPHIC BUG FOUND (value-backup perspective). This alone explains a large fraction of the plateau and almost certainly poisons every self-play target where the search tree crosses an `END_TURN`. Fix before any further training compute is spent.**

Scope reviewed: `mcts.cpp`, `macro_mcts.cpp`, `macro_actions.cpp`, `feature_extractor.cpp`, `action_features.cpp`, `policy_network.cpp`, `value_function.cpp`, `policies.cpp`, `pathfinder.cpp` (+ headers), cross-checked against `python/blood_bowl/features.py` and `python/blood_bowl/policy_trainer.py`.

The actually-used self-play searcher is **`MacroMCTSSearch`** (`ai="macro_mcts"`, `bb_module.cpp:440`). `MCTSSearch` (action-level) shares the same backup bug.

---

## CRITICAL

### C1. Value backup never flips sign across opponent plies (perspective bug)
**`macro_mcts.cpp:509-515` (`MacroMCTSSearch::backpropagate`) and `mcts.cpp:276-286` (`MCTSSearch::backpropagate`)**

```cpp
void MacroMCTSSearch::backpropagate(MacroMCTSNode* node, double value) {
    while (node) {
        node->visits++;
        node->totalValue += value;   // <-- same sign at EVERY depth
        node = node->parent;
    }
}
```

`value` is computed once at the leaf, always from `searchingSide`'s (the root active team's) perspective (`simulate(sim, searchingSide)`, `macro_mcts.cpp:132`). It is then added with the **same sign** to every node on the path back to the root.

This is correct only if every node in the tree belongs to `searchingSide`. It does not. An `END_TURN` macro is always enumerated (`macro_actions.cpp:145`) and its expansion (`expandEndTurn`, `macro_actions.cpp:1019-1025`) executes `END_TURN`, which **switches `state.activeTeam`** but does **not** set `result.turnover`. `replayToNode` (`macro_mcts.cpp:517-540`) only aborts on terminal phases or `turnover`, so it happily replays past `END_TURN`. Therefore every subtree under an `END_TURN` node consists of the **opponent's** macros, and the leaf value for those subtrees is still added with `searchingSide`'s sign instead of being negated.

**Failure scenario:** Root team considers `END_TURN`. The child subtree explores the opponent scoring a touchdown. `simulate` returns a strongly negative value for `searchingSide` (opponent has ball near our endzone) — correct. But because it's the opponent's node, a proper minimax/negamax backup should treat "opponent scores" as good *for the opponent at that node*. Worse: the Q-value of the root's `END_TURN` child is the average of leaf values from a mix of our-ply and opponent-ply leaves, all stamped with our sign. The search cannot reason about the opponent acting adversarially at all — deeper exploration of opponent replies makes the estimate *worse*, not better. Q-values for any line that crosses a turn boundary are corrupted, and those Q-values become `lastBestValue_` / visit-distribution targets logged for policy/value training (`MacroMCTSPolicy::operator()` → `decisions_`; visit fractions in `simulate_game_logged`). Garbage targets → the net learns garbage about end-of-turn and defensive value. This is exactly the "flipped-sign perspective in value backup = catastrophic" failure the brief flags.

This very plausibly drives the stuck ~43% nil-nil rate: the agent cannot correctly value "end my turn and let the opponent try to score," so it neither presses its own scoring lines nor defends coherently — it converges to passive 0-0 play.

**Fix (negamax):** flip the value at each ply boundary. The clean fix is to make `simulate` always return the value from the **node-to-move's** perspective, and negate on each step up:
```cpp
void backpropagate(MacroMCTSNode* node, double value) {
    while (node) {
        node->visits++;
        node->totalValue += value;
        value = -value;            // negate per ply
        node = node->parent;
    }
}
```
…but this is only correct if "one node = one ply of one team." Because a single team activates *many* players across multiple macros before `END_TURN`, the sign must flip **only at `END_TURN` boundaries**, not at every macro. The robust implementation: record on each node which team is to move *after* its macro (track `activeTeam` during `replayToNode`/`expand`), evaluate the leaf from a fixed reference side, and when backing up add `+value` to nodes whose active side == reference and `-value` to the others. Equivalent: store `sideToMove` per node and accumulate `value * (node.side == searchingSide ? +1 : -1)`. Add a unit test: a position where `END_TURN` lets the opponent walk in a TD must yield a *lower* Q for `END_TURN` than for a blocking macro that prevents it.

---

## HIGH

### H1. Neural policy hidden buffer is hard-capped at 64; silently truncates larger nets
**`policy_network.cpp:43-45`**
```cpp
int H = hiddenSize_;
float hidden[64];      // stack-allocated, max 64
H = std::min(H, 64);
```
If a policy net is ever trained/exported with `policy_hidden_size > 64`, `H` is silently clamped to 64: the extra hidden units are dropped, `W2_`/`b1_` rows past 64 are ignored, and the C++ priors diverge from what Python trained — corrupting priors with no error. Current default is `hidden_size=32` (`policy_trainer.py:135`), so **not currently triggered**, but it is a latent landmine the moment hidden size is bumped (a natural next step when scaling features 70→150→492). Fix: `std::vector<float> hidden(H);` sized from `hiddenSize_`, drop the `min(H,64)`. (The value net already does this correctly, `value_function.cpp:36`.) Severity HIGH because the planned feature-scaling work will likely also widen the hidden layer.

### H2. `carrier_blitzable` (feature 63) and most "reach" features use Chebyshev distance, ignoring tackle zones, dodges, and occupancy
**`feature_extractor.cpp:356-368`** (also affects 59, 60, 63, 64, 66, 67, 68 and `action_features` dist/gfi)
```cpp
int dist = chebyshev(oppStandingPlayers[j].pos, carrierPos);
if (dist <= oppStandingPlayers[j].ma) { carrierBlitzable = true; ... }
```
Confirmed present. Pathfinder is never consulted; the comment's "or could dodge out" is not implemented. `PathNode::dodged` (`pathfinder.cpp:11`) **is dead** — it is written `false` everywhere and never read; the BFS also does not add dodge/GFI cost or respect tackle zones (it only blocks on occupancy, `pathfinder.cpp:86`). Consequences:
- `carrier_blitzable` is **over-triggered**: an opponent caged in by our own players, or one that would need 3 dodges through TZs, is still flagged as able to blitz the carrier. The feature is essentially "is any opponent within MA Chebyshev squares," a much weaker and noisier signal than intended — it tells the net to fear safe cages.
- Same systematic optimism in `one_turn_td_vulnerability` (66), `surfable_opponents` (64), `pass_scoring_threat` (60), `carrier_can_score` (59). They all overcount reachability.

**Impact:** these are *consistent* between C++ and Python (the Python extractor is identically Chebyshev-based, `features.py:521-529` etc.), so they do **not** cause C++/Python divergence — the net just learns a blurrier-than-ideal danger signal. This is a representation-quality ceiling, not a correctness-vs-training-data bug. Given the suspected representation plateau, replacing these with a shared per-player TZ-aware flood-fill (one BFS per standing player, reused across all features) is the highest-value feature improvement. Recommend doing it **in both extractors simultaneously** to preserve parity.

### H3. `replayToNode` is open-loop with fresh dice every iteration — Q-values average over re-sampled outcomes, but `prior`/structure assume a fixed tree
**`macro_mcts.cpp:526-537`, `mcts.cpp:316-337`**
Each selection re-executes the whole path from root with freshly rolled dice (`greedyExpandMacro(state, ..., dice_)`), and aborts the iteration if any replayed macro turns over (`return false` → iteration wasted, `macro_mcts.cpp:116-119`). This is a deliberate open-loop / stochastic-MCTS choice and is *defensible*, but two real problems:
1. **Selection/replay mismatch:** `select()` descends the tree using stored Q/visits, but the state it lands in is a *fresh* stochastic rollout of the same macro sequence — the node's statistics mix outcomes from many different realized states. Combined with C1 this is hard to reason about; on its own it inflates variance and biases toward low-turnover macros (turnover branches silently drop their iteration count, `iterations++; continue`, so a risky-but-high-value macro is under-credited).
2. Wasted iterations on turnover are counted against `maxIterations`, so effective search depth is lower than configured for bashy/dodgy positions. Lower priority than C1/H1 but worth a comment/measurement.

---

## MEDIUM

### M1. `simulate` re-extracts 70 features and runs the full net every leaf (no trunk/feature caching)
**`macro_mcts.cpp:496-499`, `mcts.cpp:255-261`, `expand` also re-extracts at `macro_mcts.cpp:190-191`**
The brief notes value is called ~100×/move and policy is the hot path. Confirmed: every `simulate` call does a fresh `extractFeatures` (a full O(players²) sweep — see the nested loops in feature 65 `favorable_blocks`, 69 `isolation_count`) plus a full dense matmul. `expand` separately re-extracts state features and computes per-macro action features. Nothing is cached per node. With unlimited `maxChildren` (self-play config sets none) and the O(n²) feature cost, this is a large constant factor. Not a correctness bug, but it directly caps iterations-per-second and therefore self-play target quality under a fixed time budget. Cache `extractFeatures(state, side)` result on the node when the state is deterministic; at minimum hoist the state-feature extraction out of the per-action loop (already done in `expand`, but `simulate`'s extraction is redundant with the parent's `expand` extraction for the same state).

### M2. `MCTSSearch::expand` picks `children[0]` as the first node to evaluate regardless of prior
**`mcts.cpp:139-146`**
After expansion it always evaluates `node->children[0]` (`mcts.cpp:142-144`). In the *action-level* `MCTSSearch`, progressive widening sorts children by prior desc (`mcts.cpp:224`), so `children[0]` is the top-prior child — fine. But in `MacroMCTSSearch::expand` children are **not** sorted (`macro_mcts.cpp:327-333` pushes in enumeration order), and `select` then evaluates `children[0]` (`macro_mcts.cpp:124-127`), which is always `END_TURN` (enumerated first, `macro_actions.cpp:145`). So the very first evaluation of every newly expanded macro node is forced to be `END_TURN`, giving it a guaranteed visit and a first-mover bias in the visit-count target. Combined with C1's broken `END_TURN` valuation this compounds the passivity. Fix: pick the highest-prior unvisited child, or descend via PUCT immediately rather than hard-coding index 0.

### M3. `MCTSSearch::backpropagate` ignores its `searchingSide`/`rootState` params and the dead-code `rootState`
**`mcts.cpp:276-286`** — same root cause as C1 for the action-level searcher; parameters `searchingSide` and `rootState` are accepted and unused, signaling the perspective logic was intended but never implemented. Folded into C1.

### M4. `stallIncentive` threshold differs subtly between C++ and Python — verify intended
**`feature_extractor.cpp:237` vs `features.py:190`**
C++ gates on `turnsRemaining > 2` where `turnsRemaining = max(0, 9 - turnNumber)` (an integer count). Python gates on `turns_remaining > 0.25` where `turns_remaining = max(0, 9 - turn_number) / 8.0` (already normalized). `2` vs `0.25*8 = 2.0` — these are equal at the boundary, so they actually match (`>2` int vs `>0.25` of the /8 value both exclude turnsRemaining==2). **Non-issue on inspection**, but it is fragile: the two extractors express the same threshold in different units, and any future edit to one will silently desync feature 35. Flagging as a parity-maintenance hazard, not a current bug.

---

## LOW / NON-ISSUES (verified)

- **N1 (was flagged): policy neural `W1` indexing `W1_[i*H+j]`.** Verified **correct.** Python exports `self.W1.flatten()` of shape `(n_features, hidden)` row-major (`policy_trainer.py:144,265`), so flat index `i*H+j` matches. Not a bug.
- **N2: NUM_FEATURES=70 duplication / C++↔Python divergence.** I diffed the full 70-vector ordering and normalization element-by-element. **They match**, including the strategic features 56-69 and the Chebyshev approximations (Python replicates the same approximations, so parity holds). `NUM_ACTION_FEATURES=15` likewise consistent. No divergence found. The duplication remains a maintenance risk (M4-style) but is currently sound. Note: GameLogger logs C++-computed features, so the Python extractor is mostly verification — divergence would bite only on re-extraction paths.
- **N3: value net W1 2D indexing / W2 nesting / b2 array-vs-scalar.** `value_function.cpp:59-101` handles both PHP `[[w]]` and flat layouts; matches Python `value_W1.tolist()` 2D export. Fine.
- **N4: Dirichlet noise at root.** Applied correctly to root children priors with `(1-w)*p + w*noise` and renorm-free (noise pre-normalized), only in training config (`bb_module.cpp:445`), disabled in eval (`:359`). Correct. Minor: RNG seeded from two D6 rolls (`macro_mcts.cpp:90`) gives only ~36 distinct seeds per call — low noise diversity, but not wrong.
- **N5: virtual loss / leaf parallelism.** None present; search is single-threaded, so absence of virtual loss is fine (not a bug, just no parallelism).
- **N6: root `visits=1` virtual visit.** Intentional so PUCT `sqrt(parentVisits)` is nonzero on first descent. Fine.
- **N7: terminal/draw value handling.** `simulate` returns heuristic/VF eval even at `GAME_OVER`/`TOUCHDOWN`/`HALF_TIME` leaves (expand marks them expanded with no children, so `select` stops there and `simulate` evaluates the static state). There is no explicit terminal +1/-1/0 reward — the VF/heuristic is used instead. Not strictly a bug (the VF approximates outcome) but a true terminal-result signal would be cleaner and interacts with C1; consider after fixing C1.

---

## Recommended order of fixes
1. **C1** — perspective-correct backup (negamax flipping at `END_TURN` boundaries). Add a regression test. This is the one that is actively burning compute.
2. **M2** — stop forcing `END_TURN` as the first evaluated child in macro search (compounds C1).
3. **H1** — size the policy hidden buffer dynamically before any hidden-size increase.
4. **H2** — shared TZ-aware flood-fill for reach features, applied to *both* C++ and Python extractors in lockstep (best lever against the representation plateau).
5. **H3 / M1 / M3 / M4** — variance/perf/parity hardening.
