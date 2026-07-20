# Phase A-for-POLICY gate test: incremental per-player candidates (2026-07-20)

## Why this test

The original Phase A (`fable_perplayer_phaseA_20260715.md`, retest
`fable_perplayer_phaseA_retest_20260716.md`) gave a twice-confirmed NO-GO on
the full C++ per-player build (~492 features), but that test only ever
checked whether 7 narrow per-player-derived scalar candidates move a
**value** fit against the structurally near-flat `mc_shaped`/drive-level
target. The 2026-07-17 policy plateau diagnosis
(`fable_policy_learning_diagnosis_20260717.md`) separately proved the
**policy** head is capped (captures only ~25% of learnable signal vs the
MCTS visit-count target, flat for 3.5 weeks, architecturally capped per an
overfit probe) and named "richer per-player features" as the thing that
would change that diagnosis — but this was never actually tested. Per
Opus's original recommendation (`team1_results_opus.md`, Q6): validate a
cheap incremental 70→~150 feature step before ever considering the full
492-dim cold-start rebuild. This is that step, run against the POLICY
target — the one combination nobody had run yet.

## New infrastructure (additive, shipped)

`PolicyDecision` (engine/include/bb/policies.h) now carries a raw
per-player board snapshot (`BoardSnapshot`: player id/x/y/state/has_ball +
ball state) at the exact MCTS decision point, via a new shared helper
`captureBoardSnapshot()` (`engine/include/bb/board_snapshot.h`,
`engine/src/board_snapshot.cpp`) also reused by the pre-existing
`TurnLog`/`captureTurnSnapshot` path. Wired into both decision-logging call
sites (`policies.cpp::MCTSPolicy::operator()`, the older non-macro path, and
`macro_mcts.cpp`, the production path). Exposed via
`get_policy_decisions()`'s new `home_players`/`away_players`/`ball_*` keys
(`engine/python/bb_module.cpp`). **Purely additive — no existing behavior,
RNG draw, or float feature value changed. 420/420 C++ tests pass
unmodified.** Unlike the original Phase A, this needs no offline `GameState`
reconstruction from turn-level snapshots (which are only captured once per
turn, not per decision) — the raw board state is now captured at the same
granularity as the policy target itself.

## Candidate feature design

6 slots (carrier + 3 nearest teammates + 2 nearest opponents by Chebyshev
distance to the carrier/ball, `player.id` stable within a game), 17 raw
dims/slot (validity, standing/prone flags, has_ball, x/y/dist-to-endzone,
MA/ST/AG/AV, Block/Dodge/Guard/Wrestle, distance-to-carrier,
enemy-tackle-zone-count) + 2 carrier-specific interaction dims computed
against the single nearest standing opponent: `net_st_for_block` (assist-aware
block-dice class, reusing `diag_perplayer_grounding.block_dice`'s
already-verified assist/Guard/TZ logic) and `carrier_blitzable_bfs` (safe
BFS reachability respecting tackle zones and MA, reusing
`diag_perplayer_grounding.bfs_can_blitz`) — the two "must precompute, net
can't derive from raw stats" interactions Opus's engine-analyst pass (Q4)
specifically flagged. 104 candidate dims total, vs. the original Phase A's 7
scalars.

Script: `diag_perplayer_policy_gate_20260720.py`.

## Data

150 self-play games (`macro_mcts` vs `macro_mcts`, `weights_best.json` +
`weights_policy.json`, MCTS=100, TV=1200, races cycling human/orc/skaven/
dwarf/wood-elf), production self-play defaults (dirichlet_alpha=0.3,
exploration_c=0.5 — NOT the 0.0/1.0 analysis-mode defaults used by
`diag_perplayer_grounding.py`, since the goal is data representative of what
the policy trainer actually learns from). 24,752 decisions total.

## Method

Game-level 80/20 split (never split within a game), 5 seeds. Two
`FlexPolicyTrainer` instances per seed (a from-scratch copy of
`policy_trainer.NeuralPolicyTrainer` with a parametrized state width — the
production class hardcodes `NUM_FEATURES` from `features.py` as the
state/action split point, which silently drops any extra candidate dims if
reused as-is): baseline (73 state + 23 action = 96 inputs, hidden 64) vs
combined (73 + 104 + 23 = 200 inputs, hidden 64), 8 passes, batch 32,
lr=0.01 — matching production `training_loop.py`/`policy_trainer.py`
defaults. Metric: held-out cross-entropy decomposed into
`H(target) + KL(target‖policy)`, mirroring the 2026-07-17 diagnosis's own
framing (captured-signal share), plus top1 agreement.

## Results

Two runs on the identical 150-game/24,752-decision dataset (no
recollection needed between them — only feature extraction + refit):

| run | mean KL baseline | mean KL combined | delta KL (mean) | direction across 5 seeds |
|---|---|---|---|---|
| raw slot features only (96 dims) | 0.1796 (std 0.0052) | 0.1810 (std 0.0048) | **+0.0013** | 5/5 seeds worse |
| + BFS/block-dice interactions (104 dims) | 0.1796 (std 0.0052) | 0.1801 (std 0.0045) | **+0.0004** | 3/5 worse, 1/5 better, 1/5 flat |

Top1 agreement: baseline 0.413, combined (interaction run) 0.411 — flat.

Both deltas are small relative to the baseline KL (0.2–0.7%) and within/at
the edge of seed-to-seed noise (std ≈ 0.005). Adding the richer,
interaction-aware feature set did not turn a negative delta into a positive
one — it turned a *consistent* small negative into a *flat, near-zero*
delta. Neither run shows the kind of material, reproducible-in-direction
improvement that would justify committing to the full incremental build.

## Verdict: NO-GO, third independent confirmation

Per-player-derived features do not move the POLICY captured-signal share on
this evidence, extending the two prior value-target NO-GOs
(`fable_perplayer_phaseA_20260715.md`, `fable_perplayer_phaseA_retest_20260716.md`)
to the head the 2026-07-17 diagnosis actually flagged as plateaued. Unlike
the original Phase A (which tested a narrow 7-scalar set), this test used a
104-dim set including the two specific nonlinear interaction terms
(BFS-blitzable, assist-aware block-dice) that Opus's own engine-analyst pass
argued the net "cannot reliably derive from raw stats alone" — so the
result is not explained by "the candidates were too shallow."

**Caveats, none of which flip the verdict but which bound its scope:**
1. Slot selection (nearest-3-teammates + nearest-2-opponents by distance to
   carrier) is one reasonable canonical choice, not the only one — a
   selection prioritizing cage corners or "most dangerous blitzer" specifically
   might carry more signal. Untested here.
2. Model: a from-scratch 64-hidden MLP, 8 passes, mirroring production
   training scale. The 2026-07-17 diagnosis's own overfit probe found this
   architecture class converges within a single run's data almost
   immediately (30 extra passes moved loss by only 0.0015 nats) — so more
   passes/capacity is not expected to change this result, consistent with
   why this test did not scale up model size or pass count.
3. This is in-sample-scale (24.7k decisions from 150 games), smaller than
   full production epoch data (~80k+ decisions/16-epoch run) — a genuinely
   small true effect could be below this test's noise floor. But per the
   project's established decision rule (`project_bloodbowl_survey_hasacted_contamination_20260716`),
   a retest that does not flip a prior verdict does not by itself justify
   another escalation without a new, specific reason to expect a different
   result at larger N — and the interaction-feature augmentation (the
   obvious "make it fairer" lever) already ran and did not flip it either.

**Recommendation:** do not proceed to the incremental 70→~150 C++ per-player
build on this evidence. Per-player value/policy enrichment has now failed
three independent, reasonably-designed gates (2 value, 1 policy) across two
different candidate richness levels. The board-snapshot logging
infrastructure built for this test is retained (additive, zero production
risk, 420/420 tests green) as reusable groundwork for any future per-player
investigation, but the next priority should shift to the one structural
lever this project has not yet directly tested: MCTS search
budget/visibility (100 iterations; prior grounding found the SCORE macro
invisible to search in 77% of decisions and mobility-advantage capitalized
only 3% of the time it was present) — a search-breadth problem that better
value/policy features cannot fix if the relevant branches are never visited
enough to be evaluated at all.
