# Full-code review: Python training pipeline (Team 1 style)

**Reviewer:** Claude Opus 4.8 — 2026-06-12
**Scope:** trainer.py, training_loop.py, policy_trainer.py, replay_buffer.py, features.py, train_from_logs.py, game_state.py
**Cross-checked against:** engine/src/feature_extractor.cpp (authoritative inference-time extractor), engine/src/game_simulator.cpp (logging), python/blood_bowl/cpp_runner.py (JSONL writer)

## Verdict

The core MC / TD(0) / TD(λ) value-target code in `trainer.py` is **correct** on sign, perspective, per-perspective grouping, draw handling, and the neural TD(λ) value-gradient. That part is healthy.

The real damage is concentrated in **two places**:

1. **The replay-buffer training path in `training_loop.py` (lines 386-405)** reconstructs fake 2-state "mini-games" and feeds them back through the TD/MC trainers. This systematically corrupts credit assignment: mid-game transitions get the *final game outcome* bootstrapped one step too early, and the true intermediate reward (0) is mislabeled. Replay is auto-enabled whenever MCTS is active (`replay_buffer_size == 0 and mcts_iterations > 0` → 10000), so this is live in every AlphaZero run. **This is the most likely silent learning-corruption source and a prime plateau suspect.**

2. **`features.py` has drifted out of parity with `feature_extractor.cpp`** on at least 4 features (15, 17, 18, and the opp-carrier write to 15). This is latent today (C++ logs its own features for C++ training), but it is a live corruption hazard the moment anyone trains/evals from Python-recomputed features, and the parity test does NOT catch it (the PHP cross-check uses a single hand-built state that never exercises the divergent branches).

Ranked findings below.

---

## CRITICAL

### C1 — Replay buffer mislabels mid-game transitions as terminal game outcomes
**File:** `training_loop.py:386-405` (reconstruction) + `replay_buffer.py:58-69`
**Failure scenario:** A non-terminal transition (`is_terminal=False`, the common case) is turned into a 2-state mini_log and run through `_train_on_log` with the *same* method (e.g. `td_lambda`). The trainer groups both states under one perspective: state 0 is non-terminal, state 1 is terminal. The reconstructed `result` record carries `winner` = the **full game outcome** (±1), derived from `transition.reward`. So:
- The successor state (`next_features`) — an arbitrary mid-game position — is pinned to a value target of ±1 (the eventual game result), as if the game ended there.
- State 0 then bootstraps `v_next` off that mispinned successor, or under MC gets ±1 directly.
- The transition's *true* intermediate reward (0, no TD scored on this step) is never represented; instead the terminal reward leaks in one step early.

Net effect: every replayed non-terminal step trains the value head to predict the final result from arbitrary intermediate states with no discounting horizon — flattening V(s) toward the global win-rate and destroying the gradient that distinguishes good from bad positions. With replay auto-enabled for all MCTS runs and batch=64 every epoch, this is a continuous corruption stream and a strong candidate for the 85-87% plateau / 43% draw stall.
**Fix:** Don't round-trip transitions through the game-log trainers. Add a dedicated `trainer.train_transition(features, reward, next_features, is_terminal, perspective)` that computes the correct one-step TD target: `target = reward + gamma * V(next) * (0 if is_terminal else 1)` with `reward = stored_reward if is_terminal else 0`. Critically, `Transition.reward` must store the **per-step reward**, not the final game outcome stamped on every step. As stored today (`replay_buffer.py:50-69` stamps the final ±1 on *every* transition of the perspective), even a corrected one-step update would be wrong — the intermediate reward must be 0 and only the terminal transition should carry ±1.

### C2 — Replay buffer stores final game outcome as the reward of every transition
**File:** `replay_buffer.py:50-69`
**Failure scenario:** `add_game` assigns `reward = ±1/0` (the whole-game result) to **every** transition in a perspective group, terminal or not. Combined with C1's bootstrapping, the buffer teaches "every state in a won game ≈ +1, every state in a lost game ≈ -1." That is undiscounted MC labeling masquerading as TD data, and it removes any intra-game credit assignment. Even if C1's reconstruction were fixed, the stored reward is wrong for TD(λ)/TD(0)/mc_shaped.
**Fix:** Store `reward = 0.0` for non-terminal transitions and `reward = ±1/0` only for the terminal transition of each perspective group. Keep `is_terminal` as the gate. (For pure-MC training you'd instead store the discounted return — but the buffer is consumed by TD methods here, so per-step reward + bootstrap is the right representation.)

---

## HIGH

### H1 — features.py [15] carrier_dist_to_td diverges from C++ (default + opp-carrier write + standing guard)
**File:** `features.py:70,73-83` vs `feature_extractor.cpp:180,188-209,506`
Three concrete divergences on the same feature:
1. **Opp-carrier contamination:** Python writes `carrier_dist_to_td` (feature 15) whenever the ball is held, *including when the opponent holds it* (line 82 runs in both the `==perspective` and `else` branches). C++ only sets it when `iHaveBall` and otherwise emits `0.5` (`out[15] = iHaveBall ? carrierDistToTD/26 : 0.5`). So when the opponent carries the ball, Python reports the opponent's distance-to-*my*-endzone normalized; C++ reports 0.5.
2. **Default value:** Python default `0.5`; C++ default for `iHaveBall && carrier not standing` is `13/26 = 0.5` — coincidentally equal, but for the ground-ball case Python falls through to the `0.5` default and C++ also gives 0.5, OK. The divergence is specifically the opp-carrier and not-standing cases.
3. **Standing guard:** C++ only computes the distance when `carrier.state == STANDING`; Python ignores carrier state.
**Why it matters:** Feature 15 carries shaping weight `-1.5` (the single strongest shaping signal) and is a value-net input. Any training/eval done from Python features would mis-shape the strongest gradient. Latent today (C++ training logs C++ features) but a live landmine.
**Fix:** Mirror C++ exactly: `feat15 = (i_have_ball and carrier standing) ? dist/26 : 0.5`; never write it for the opp-carrier branch.

### H2 — features.py [17]/[18] avg-x empty default diverges (0.0 vs 0.5)
**File:** `features.py:89-90` vs `feature_extractor.cpp:510-511`
When a side has no standing players on pitch, Python emits `my_avg_x = 0.0` / `opp_avg_x = 0.0`; C++ emits `0.5`. End-of-half / heavy-casualty states (exactly the nil-nil grind states under investigation) hit this branch. Different constant → different value-net input and any Python-side shaping/eval.
**Fix:** Default to `0.5` to match C++.

### H3 — Parity test does not exercise the divergent branches
**File:** `python/tests/test_features.py:51-111`
The only Python↔engine cross-check (`test_cross_validation_with_php`) builds one fixed state with both teams having standing players and the **home** team carrying the ball. It never hits: opponent-carries-ball (H1), empty-standing-side (H2), not-standing carrier, or the C++ path at all (it compares to PHP, not to `feature_extractor.cpp` / `bb_engine.extract_features`). So all of H1/H2 pass silently. Given the note "a parity test exists in tests/ … a mismatch silently corrupts training," the test gives false confidence.
**Fix:** Add a property/fuzz test that compares `features.extract_features` against `bb_engine.extract_features` over randomized states (ball held by each side, ground ball, empty sides, non-standing carrier). This would have caught H1/H2 immediately.

---

## MEDIUM

### M1 — `train_monte_carlo_shaped` adds the full final reward at every step (not just terminal)
**File:** `trainer.py:137` and `:427` (neural)
Target for a non-terminal state is `final_reward + γ·Φ(s') − Φ(s)`. Because the value head regresses *directly* to this scalar (it is not summing a trajectory of per-step rewards), this is defensible: the MC return for every state is the same ±1, and PBRS adds the potential shaping term on top. It is internally consistent and the sign is correct. Flagging as MEDIUM only because it is easy to misread as double-counting and because the shaping term `γΦ(s')−Φ(s)` telescopes to zero over a trajectory only if the head learned exact returns — with function approximation the per-state shaping bias persists and can bias V. Recommend documenting the intended semantics; no code change strictly required.

### M2 — Replay-buffer reconstruction ignores `gamma` discounting and mixes methods
**File:** `training_loop.py:388-405`
Even setting C1/C2 aside, the mini-log only ever has ≤2 states, so TD(λ) eligibility traces never build up and `td0`/`td_lambda` collapse to a single one-step update with no discount across the real horizon. The replayed gradient is therefore a different objective than the on-policy game-log gradient that shares the same weights — the two fight each other. After fixing C1/C2 with a direct transition update this disappears; listed so the fix is scoped correctly.

### M3 — No NaN/inf guard on value targets or weights
**File:** `trainer.py` (all update methods), `policy_trainer.py`
Neural value uses `tanh` (bounded) but `W1/W2` are unbounded and `_update` has no gradient clipping (unlike the policy trainer, which clips to ±5). A single corrupted feature row (e.g. an inf from a future feature change) silently propagates to all weights with no guard and no detection until win-rate collapses. Recommend adding `np.isfinite` assertions on targets and an optional global grad-norm clip in `NeuralTrainer._update`.

---

## LOW

### L1 — `train_from_logs.py` hardcodes `LinearTrainer`
**File:** `train_from_logs.py:46`
This offline trainer always instantiates `LinearTrainer`, so running it against a neural/alphazero weights file will `load_weights` a neural dict into a linear trainer (or error). Fine if only ever used for linear, but a foot-gun given the project now trains neural. Use `load_trainer()` for auto-detection.

### L2 — `LinearTrainer._align_features` silently pads/truncates
**File:** `trainer.py:232-241` (and neural `:532`)
Padding mismatched feature vectors with zeros hides feature-count drift instead of failing loudly. If a logged game ever has the wrong feature count (e.g. C++ extractor changes to 71 features but Python `NUM_FEATURES` stays 70), training continues silently on misaligned/zeroed features. Prefer an assertion or at least a one-time warning when truncation/padding occurs.

### L3 — stall_incentive (idx 35) confirmed IN PARITY (non-issue)
**File:** `features.py:188-193` vs `feature_extractor.cpp:236-242`
Python's threshold `turns_remaining > 0.25` on the already-normalized value equals C++'s `turnsRemaining > 2` on the raw int; both emit `turnsRemaining/8` and ×1.5 when leading. No drift. Documented here because the prompt flagged idx 35 specifically.

### L4 — Reward-shaping sign/perspective audit: clean
The shaping weights in `DEFAULT_SHAPING_WEIGHTS` are applied via `_compute_potential` to the **logged** features, which are already extracted from the correct per-state perspective by the C++ extractor (`game_simulator.cpp:505,537` use `log.perspective`). Signs are coherent: +my_score / −opp_score, −carrier_dist_to_td (closer = better), +carrier_can_score, −opp_scoring_threat. No perspective inversion found. `_get_reward` returns the correct ±1 per perspective and 0 for draws across all methods and the replay buffer. Draws are never treated as wins/losses (`winner is None → 0.0` everywhere).

---

## Summary table

| ID | Sev | One-liner |
|----|-----|-----------|
| C1 | CRITICAL | Replay reconstruction bootstraps final game outcome onto arbitrary mid-game successor states |
| C2 | CRITICAL | Replay buffer stamps final ±1 reward on every transition (no per-step reward) |
| H1 | HIGH | features.py[15] carrier_dist_to_td diverges from C++ (opp-carrier write, standing guard) |
| H2 | HIGH | features.py[17/18] avg-x empty-side default 0.0 vs C++ 0.5 |
| H3 | HIGH | Parity test never exercises divergent branches / compares to PHP not C++ |
| M1 | MED | mc_shaped adds full final reward each step (documented as intentional, verify) |
| M2 | MED | Replay mini-logs defeat γ/eligibility; replayed objective ≠ game-log objective |
| M3 | MED | No NaN/inf guard or grad clip in NeuralTrainer value updates |
| L1 | LOW | train_from_logs.py hardcodes LinearTrainer |
| L2 | LOW | _align_features silently pads/truncates, hiding feature drift |
| L3 | LOW | stall_incentive idx 35 confirmed in parity (non-issue) |
| L4 | LOW | Reward-shaping signs/perspective + draw handling audited clean |
