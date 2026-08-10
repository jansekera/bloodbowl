# Is the policy network's imitation training converging on anything? (2026-07-17, Fable 5)

**Question being answered:** `policy-lr=0.01` imitation training has been genuinely ON
in production since 2026-06-17 (the old "policy-lr is 0" belief was wrong; the real
off-switch is `policy_blend=0.0`). Yesterday's 16-epoch run (`epoch_metrics.csv`)
shows policy_loss ~2.02-2.04 and top1 agreement ~0.39-0.42, apparently flat. Is the
policy head **healthy-but-slow**, **plateaued/stuck**, or is there **too little data
with training truly enabled** to judge?

## Verdict (short form)

**Plateaued — and provably so, not under-trained.** But the flat numbers are far less
alarming than they look, because most of the reported loss is an irreducible floor:

1. **~92% of the reported policy_loss is target entropy, not error.** The trainer
   minimizes cross-entropy against the full MCTS visit distribution
   (`policy_trainer.py:35-90`), so the loss is lower-bounded by H(target). Measured on
   the latest run's decision logs: H(target)=1.86 nats (H_norm 0.89, mean 9.3
   candidate actions). CE=2.03 therefore means residual KL(target‖policy)=**0.17**,
   against KL(target‖uniform)=**0.225**. The net genuinely beats uniform — it captures
   **~25% of the available signal** — and top-3 recall is 0.61. `top1_agree` ~40% is
   additionally capped by construction: 34% of decisions have a near-tie at the top
   (2nd-best visit ≥ 80% of best). All of this re-confirms the 2026-06-23 Team-1
   finding (`team1_diagnostic_findings.md`: KL 0.096 vs uniform 0.133, "top1 is
   unhittable by construction").
2. **The captured-signal share has NOT grown in 3.5 weeks of cumulative training.**
   06-23: (0.133−0.096)/0.133 = **28%** of signal captured. Today (current
   `weights_policy.json` on the 07-16 run's logs): (0.225−0.170)/0.225 = **24%**.
   The policy head is carried across iterations since 06-18 (`_carry_over_policy`,
   commit 8da15f7) and has seen ~160 logged epochs across 8+ runs — with zero growth
   in fit quality relative to what's learnable.
3. **Overfit probe proves the plateau is architectural, not "needs more epochs".**
   Warm-starting from production weights and running **30 extra passes over the same
   4,949 decisions** (pure memorization opportunity) moves in-sample CE by only
   0.0015 nats (2.0316→2.0301). Controls: lr×5 gains 0.003 nats; a **fresh net from
   scratch reaches CE 2.045 in 30 passes** — within 0.015 nats of the 3.5-week
   cumulative net. Conclusion: the current 64-hidden net on 73 state + 23 action
   features absorbs everything it can from one run's data almost immediately, and
   nothing in (more epochs, more carry-over, higher lr, re-init) buys more.

This coherently explains today's blend bring-up A/B pattern: a prior that is ~25%
better than uniform reliably nudges results **against a random opponent** (any
non-uniform signal helps there) but is too weak to move **self-play against a real
opponent** at blend 0.15. The prior is not noise — it is weak, and weak for reasons
"train longer" cannot fix.

## Data inventory (what history actually exists)

`policy-lr=0.01, model=neural` is printed in the header of **every** surviving run
log — no run had to be excluded for training being off:

| run log | date | epochs | mean policy_loss | mean top1 |
|---|---|---|---|---|
| `evidence/training_full.log` | ~06-21..23 (committed 3db6185) | 48 (3 iter × 16) | 2.236 | 38.8% |
| `leverb_full_20260629.log` | 06-29 | 16 | 2.123 | 43.2% |
| `betarun_full_20260630.log` | 06-30 | 16 | 2.129 | 43.0% |
| `training_post_stepcap_fix_20260708.log` | 07-08 | 16 | 2.084 | 38.7% |
| `training_post_expandscore_fix_20260709.log` | 07-09 | 16 | 2.085 | 39.6% |
| `training_mc_td_mix_stage1_20260713.log` | 07-13 | 16 | 2.076 | 40.9% |
| `training_mc_td_mix_null_alpha1_20260714.log` | 07-14 | 15 | 2.074 | 40.4% |
| `training_postfixes_20260716.log` (= `epoch_metrics.csv`) | 07-16 | 16 | 2.026 | 40.4% |

Reading the table: the cross-run loss decline 2.24→2.03 is **not** cumulative learning
— per Finding 3 a fresh net reaches each run's level within one run, so the level
tracks the changing target distribution (each engine fix — halfclock, hasActed,
ADVANCE floor… — reshapes MCTS decisions). Top1 is non-monotone (43% in late June,
back to 40% now) for the same reason.

## Finding 1 — the 16-epoch run is genuinely noise-flat, not weakly-trending

`epoch_metrics.csv` (16 epochs × 40 games, 07-16 run), OLS vs epoch:

- policy_loss: mean 2.026, sd 0.007; slope **+0.00024/epoch** (SE 0.00037, t=0.65) —
  a statistically-zero, if anything *upward* drift; total 16-epoch movement +0.004.
- policy_top1_agreement: mean 0.404, sd 0.008; slope **−0.00039/epoch** (t=−0.90) — zero.

Within-run there is no discernible trend, weak or otherwise. Same holds by eye in all
seven older logs.

## Finding 2 — decomposing the loss on current weights (the key reframe)

Current `weights_policy.json` (post-07-16-run stash) evaluated on the 07-16 run's
decision logs (`training_logs/epoch_*/decisions_*.json`, ~5k decisions per epoch),
forward pass matching the trainer exactly (96 = 73+23 inputs, hidden 64, softmax T=1):

| epoch data | CE | H(target) | KL(t‖policy) | KL(t‖uniform) | top1 | top3 | near-tie@top |
|---|---|---|---|---|---|---|---|
| epoch_001 | 2.030 | 1.864 | 0.166 | 0.223 | 0.283 | 0.601 | 0.342 |
| epoch_008 | 2.028 | 1.853 | 0.175 | 0.235 | 0.310 | 0.609 | 0.339 |
| epoch_016 | 2.032 | 1.862 | 0.170 | 0.225 | 0.316 | 0.614 | 0.344 |

Uniform-prior CE would be H + KL(t‖u) = **2.087**; perfect fit would be **1.862**.
The whole learnable range is 0.225 nats; the net sits at 2.030, i.e. ~25% of the way
down. That share was ~28% on 06-23 (`team1_diagnostic_findings.md`) — no growth.
The dominant limiter is the target itself: MCTS visit distributions are near-uniform
(H_norm 0.886-0.893 across all 16 epochs of `epoch_metrics.csv`, `mcts_H` column),
and the same Team-1 doc measured residual H_norm ~0.81 as "largely irreducible by
search" at feasible sim budgets.

## Finding 3 — overfit + controls probe (in-memory only, nothing written)

Starting point: production weights, in-sample CE on epoch_016 data = 2.0316.

| probe | result |
|---|---|
| +30 passes, lr=0.01 (production setting) | CE 2.0301 (−0.0015), top1 0.412 |
| +15 passes, lr=0.05 (5× production) | CE 2.0289 (−0.0027), top1 0.416 |
| fresh random init, 30 passes, lr=0.01 | CE 2.0450, top1 0.385 |

Even with unlimited passes over fixed data the architecture cannot descend more than
a few thousandths of a nat below its plateau; and a fresh net closes ~93% of the gap
to the cumulative net within a single run's training. Cumulative carry-over since
06-18 is worth ~0.015 nats total. **"More epochs" is not a lever here.**

## Finding 4 — pre/post 06-19 fix comparison is impossible (and moot)

Timeline (git): imitation training first enabled 06-17 (878b795, `policy-lr=0.01`);
linear→neural 06-17 (7ac17c2); carry-over persistence 06-18 (8da15f7); hidden 32→64
06-18 (13799c2); **action features 15→23** 06-19 (f1cadb4 — the root-cause fix for
"~50% of decisions had the best action feature-identical to a worse one, capping
top1", see `engine/include/bb/action_features.h:11-15` and
`team_neural_policy_brief.md`). The earliest surviving epoch-level policy log was
committed 06-23. So there are at most ~2 days of pre-fix training and **no surviving
pre-fix epoch logs** — a before/after log comparison cannot be made. It is also moot:
the post-fix KL measurements (06-23 and today) already show the head learns *something*
post-fix; the open question was never "did the fix work" but "does learning progress",
answered above (no).

## Diagnosis

**Plateaued/stuck — high confidence.** Precisely: the loss number is 92% entropy
floor (not a problem), but the genuinely learnable residual has been stuck at ~25%
captured since at least 06-23, and the overfit probe shows the ceiling is the
representation (73+23 features, hidden-64) and/or the low-signal target — not
training time, not lr, not carry-over. There is *enough* historical data to rule out
"inconclusive": 8+ runs, ~160 epochs, all with training verified on.

**What would change my mind / what would actually move this:**
1. **Richer action/state features** (the per-player Team-1 track, 70→~150 first per
   Opus) — if the captured-signal share rises materially with new features, the
   feature-ceiling explanation is confirmed; this is the same conclusion as the
   capacity-vs-features tests (3× confirmed: features are the ceiling).
2. **Sharper targets**: anything that drops mcts_H below ~0.85 (more sims,
   `exploration_c`, or value-head improvements that discriminate actions in search)
   grows the 0.225-nat signal pool itself.
3. A **held-out generalization measurement** showing a large train/test KL gap would
   shift the story toward overfitting-with-noise — unlikely given the overfit probe
   barely moves even in-sample, but it is the one unchecked box (all measurements
   here are in-sample by necessity; decision logs only exist for the run that
   produced the weights).

## Repro notes

- Trend stats: OLS on `epoch_metrics.csv` columns (n=16).
- Fit metrics: reimplementation of `measure_policy_metrics.py` decision_metrics with
  current dims (that script hardcodes NUM_FEATURES=70; production is 73 →
  `POLICY_INPUT_SIZE=96`), weights from `weights_policy.json`, data from
  `training_logs/epoch_{001,008,016}/decisions_*.json`.
- Probes: `blood_bowl.policy_trainer.NeuralPolicyTrainer.train_on_decisions`
  (passes=5 per round, batch_size=32), weights loaded/kept in memory only.
