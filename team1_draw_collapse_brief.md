# Team 1 brief — the 0-0 draw collapse is MODEL-INDEPENDENT

**Date:** 2026-06-24
**Status:** focused diagnostic question for Team 1 (you have the full code: engine + python training)
**One-line ask:** *Self-play and head-to-head both collapse to ~0-0 draws regardless of how good the value/policy heads are. Find the structural cause (reward / credit horizon / MCTS) of why SCORING (pickup → carry → TD) is not worth pursuing — and propose the minimal fix.*

---

## Why this is now the priority (evidence the draw tendency is structural, not a model defect)

We just ran a validation training run (`mc_return_shaped` value target + `vf_blend=0.5` + `policy_blend=0.15`) and gated it. It was **HARD-REJECTED**:

- Benchmark dropped **89.0% → 80.0%** (limit −5%).
- Head-to-head New vs Frozen: 41W **525D** 34L → **87.5% of 600 games were draws** (only 75 decisive).
- Self-play `nil_nil` rate climbed **22% → 55–65%** during training.

We suspected a value collapse (draw-heavy data → `mc_return≈0` targets → value head goes flat → passive MCTS → more draws). **We measured it offline and REFUTED it:**

| weights | value std (10k states) | corr(v, mc_return) | MSE | mean v: WIN / DRAW / LOSS |
|---|---|---|---|---|
| Champion (89%) | 0.616 | +0.762 | 0.189 | +0.702 / +0.036 / −0.627 |
| **Rejected (80%)** | **0.586** | **+0.795** | **0.156** | +0.676 / +0.010 / −0.641 |

The rejected value head is **healthy and BETTER-calibrated** than the champion (higher correlation, lower MSE, cleanly ordered WIN>DRAW>LOSS) — yet it produces **more draws**. So:

> **The slide to 0-0 is independent of model quality.** A strictly better value head does not score more; injecting it into search (vf_blend) makes play *more* passive. This matches the long-standing observation: `nil_nil` has sat at 40–50% across every config since 2026-06-01, and earlier ceiling-4 measurements showed enabling value blend *increased* near-ties (34% → 45%).

`mc_return` distribution in the buffer (n=10000): **51% of outcomes are draws** (|return|<0.05), balanced, std 0.637. Half of all self-play games end 0-0.

---

## The question for Team 1 (you have the code — verify, don't assume)

**Why is scoring structurally not incentivized, such that both players rationally converge to 0-0?** Concretely, trace these candidate causes in the code and tell us which actually bind:

1. **Terminal reward symmetry.** Reward is +1 / 0 / −1 for win/draw/loss. A 0-0 draw costs nothing. With two equally-matched sides, "don't lose" → "don't commit to scoring" is a Nash equilibrium. (An old plan proposed a draw penalty −0.3 to break it — never validated. Is that the right lever, or does it just trade draws for blunders?)

2. **Credit-assignment horizon vs. the scoring sequence.** A TD requires a multi-turn committed sequence: pickup → carry downfield → survive → score. Does MCTS (100 sims, macro depth) + the value/return target ever actually *see* the TD payoff from the pre-pickup state? Is the effective horizon too short for the risky multi-step payoff to out-value the safe local move? (`macro_mcts.cpp` search depth/rollout; `gamma=0.99` discounting over how many decisions?)

3. **Carrier-conditional feature dead-zone.** Carrier-conditional features ([12], [40], [42], [63]) are 0.0 while the ball is on the ground, so the gradient toward *picking the ball up* may be ~zero until someone already holds it (documented 2026-06-01 in `nil_nil_fix_plan.md`). Is the pickup decision starved of signal? Is feature [14] `ball_on_ground` / a loose-ball-proximity feature wired into both the shaping AND the value/policy input?

4. **Why does vf_blend make it WORSE?** A well-calibrated value (sees aggregate state) replacing the hand-coded leaf heuristic *increases* passivity. Hypothesis: the value correctly predicts "this risky scoring attempt has high variance / often fails" and therefore steers MCTS toward the safe 0-0 line, whereas the old heuristic had a hard-coded pickup/forward bonus. If true, the fix is in the **reward/shaping** (make scoring positive-EV), not in the heads or the wiring.

## Deliverable we want back

- Which of (1)–(4) actually bind, with code references and a quick measurement (e.g. EV of a scoring attempt vs. a safe move under the current reward; does the search horizon reach a TD).
- The **minimal** reward/structure change that makes scoring positive-EV without inducing reckless blunders — ideally testable with a short run and a draw-rate metric, not a full retrain.
- Explicit verdict: is this fixable in reward/featurization, or does it need richer state (per-player) to even represent the scoring opportunity?

## Pointers
- Gate verdict + measurements: `/tmp/training_iter1_rejected.log`, rejected weights `/tmp/weights_iter1_rejected_train.json`.
- Offline diagnosis reproducible from `replay_buffer.pkl` (fields: features, mc_return, reward, is_terminal, perspective) + value head `value_W1/b1/W2/b2` (features @ W1 + b1 → ReLU → @ W2 + b2 → tanh).
- Old shaping analysis: `nil_nil_fix_plan.md` (2026-06-01). Reward path: `trainer.py::_get_reward` (both Linear + Neural). Leaf eval / blend: `macro_mcts.cpp:188` (policy gate), `:496` (value gate).
- Prior context: `team1_diagnostic_brief.md` (direction #3 already flagged "0-0 = risky pickup→carry→TD sequence — does reward incentivize it?").
