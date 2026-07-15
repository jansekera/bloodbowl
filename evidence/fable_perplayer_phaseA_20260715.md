# Phase A offline ridge-fit validation: per-player candidate features (2026-07-15)

**Per `proposals_value_signal_roadmap_20260714.md` §4.2** — the go/no-go gate that decides
whether today's grounded per-player candidate features are worth pursuing as a full C++
per-player build (~492 features, new network head, ~12 engine-side change points per the Opus
audit) before any of that work is attempted. This is the actual Phase A run, not a plan for one.

## Data and reconstruction

**No new games collected.** Reused the 150-game dataset from today's replay-grounding pass
(`diag_perplayer_grounding_data/main/g****.json.gz`, full per-turn board snapshots) — this
already satisfies the roadmap's "persist board snapshots" prerequisite. Two pieces were
reconstructed fresh from those snapshots, both reusing existing project machinery rather than
new logic:

1. **Baseline f73**: for each snapshot, rebuilt a `bb_engine.GameState` (roster via
   `get_developed_roster`/`setup_half` for correct per-player stats/skills, then overrode
   position/state/score/ball/turn/active-team from the logged snapshot) and called the actual
   production `bb_engine.extract_features()` — zero drift by construction, since it *is* the
   production C++ extractor, not a reimplementation. Three disclosed approximations (none of
   the 5 candidate features depend on them): rerolls fixed at 3/3 (not logged), weather fixed at
   NICE (not logged), and off-pitch players (KO/injured/dead — collapsed into one bucket by the
   engine's own turn-snapshot capture) uniformly reconstructed as KO, which only blurs the
   KO-vs-injured split within features 6-9, not their sum.
2. **Target G**: `blood_bowl.rewards.episode_returns()`/`terminal_value()` — the project's SSOT
   for the standard `mc_shaped` target (γ=0.99) — applied exactly as
   `replay_buffer.ReplayBuffer.add_game()` does it (group each game's turns by perspective,
   discount from the game's final result). This reproduces the production `mc_return` field
   without needing the production buffer, which lacks per-player positions and so cannot host
   the candidate features below.

Script: `diag_perplayer_phaseA.py` (`run` reconstructs+caches, `fit` runs the ridge comparison).
Reconstruction took 10s for all 150 games (4,803 states, 300 episodes = one per game per
perspective, same granularity `diag_capacity_probe.py` uses on the production replay buffer).

## Candidate features (7 scalars, all from today's grounded/corrected definitions)

Reused the BFS/Board machinery already built for today's `carrier_blitzable` and
`is_free_receiver` grounding work — per Opus's Q2 "one flood-fill per player, shared across
features" framing — rather than new feature-extraction code:

| # | Feature | Definition | Grounding basis |
|---|---|---|---|
| c1 | `carrier_blitzable_bfs` | binary, BFS safe-path (replaces f63's Chebyshev) | SUPPORTED, 22.6% false-danger rate |
| c2 | `net_st_bad_exposure_frac` | fraction of my standing players adjacent to an opponent in a net<0 block, excluding Wrestle attackers (today's Task-2 correction) | SUPPORTED, 8.0% genuinely-bad blocks |
| c3 | `cage_worst_corner_tier` | 0/1/2 severity of my cage's worst corner | PARTIALLY SUPPORTED |
| c4 | `cage_worst_corner_dodge` | does that worst corner have Dodge (today's Dodge×Tackle follow-up) | directional support, small n |
| c5 | `mobility_advantage_progress` | best teammate's safe-BFS endzone progress minus the carrier's | reframed is_free_receiver — strongest single finding, 30.8% prevalence |
| c6 | `mobility_advantage_flag` | c5 ≥ 3 squares, binary | same |
| c7 | `carrier_adjacent_sideline` | carrier on y∈{0,1,13,14} | UNCLEAR, rare |

Prevalence over the full dataset (300 episodes, 4,803 states): c1 16.6% nonzero, c2 24.6%, c3
5.7%, c4 3.6%, c5 23.1% nonzero (mean 0.628), c6 9.0%, c7 1.8%. Feature-distinctness check:
unique-rows/episode-length = 1.000 (no state literally collapses to an identical vector within
an episode, so degeneracy is not what's limiting the fit below).

## Method: ridge fit, episode-level split

Exact methodology/thresholds from `diag_capacity_probe.py` (same-day sibling test, production
replay buffer): 80/20 **episode-level** train/test split (never split by state — avoids leakage
across a single episode's highly correlated snapshots), `RidgeCV` on standardized inputs,
`G ~ f73` (baseline, 73-dim) vs `G ~ f73+candidates` (combined, 80-dim), 5 seeds. Metrics: held-out
MSE/R², within-episode prediction std (mean over test episodes with ≥3 states), within-episode R²
(both prediction and label centered per episode — the sharpest test of "captures within-game
structure, not just per-episode offsets"), between-episode R² (on per-episode means), and
`mean|ΔV|` / `corr(pred_baseline, pred_combined)` — the roadmap's own stated verdict axis
("kandidáti musí zvednout within-episode strukturu predikce, analogicky mean|ΔV|>0.1 / std ↑").

## Results (5 seeds: 20260715-20260719)

| metric | mean | std | roadmap bar |
|---|---|---|---|
| mean\|ΔV\| (combined vs baseline predictions) | **+0.0243** | 0.0039 | **>0.1** |
| corr(baseline, combined) | +0.9963 | 0.0008 | <0.99 |
| ΔR² (test) | -0.0021 | 0.0029 | positive/material |
| ΔMSE (test) | +0.0009 | 0.0012 | negative/material |
| Δ(within-episode std) | +0.0034 | 0.0012 | up, toward label's own std |
| Δ(within-episode R²) | -1.06 | 0.38 | up |

**mean|ΔV| = 0.024, roughly 4x below the roadmap's own >0.1 threshold, stable across 5 seeds**
(std=0.0039 — this is a reproducible null, not seed noise). `corr(baseline, combined) = 0.996`:
adding the 7 candidate dimensions leaves the ridge fit's predictions almost unchanged.
ΔR²/ΔMSE are inside noise and inconsistent in sign across seeds (2 of 5 seeds show a *worse*
test R² with candidates added — mild overfitting from 7 extra, mostly-sparse dimensions, not a
real gain anywhere). Per-seed candidate coefficients (standardized scale) stay small
(|coef|<0.06 throughout) and flip sign across seeds for several candidates (e.g.
`cage_worst_corner_dodge`: +0.029, +0.056, +0.036, -0.0003, +0.058 — directionally positive but
noisy; `carrier_blitzable_bfs`: +0.0005, -0.020, -0.003, +0.008, -0.025 — no stable sign at all).
No candidate emerges as a consistent, non-trivial driver of the fit.

## A structural caveat that matters more than the headline number

The **label G itself is nearly flat within an episode**: mean test-set within-episode std of G
is **0.030** (range 0.028-0.032 across seeds) — for comparison, both the baseline and combined
models predict within-episode std around **0.21-0.26**, i.e. **7-9x more variation than the true
label has**. This is why within-episode R² is wildly negative (-46 to -62) for *both* arms, not
just the candidate-augmented one: with γ=0.99 and episodes averaging ~16 states, `G_t =
γ^(T-t)·terminal_value` barely decays over the length of one episode (for a typical -0.5 nil-nil
terminal value, G_t ranges only from about -0.43 to -0.50 across an entire episode) — there is
almost no true within-episode target variance for *any* function class to fit, linear or not.
This is not a new finding — it is the project's own documented **value-target flatness root
cause** (see memory: "Value-target flatness = root cause"), and it is the exact same mechanism
already REFUTED twice this project via other channels (`mc_td_mix` Stage 1;
`evidence/fable_drive_target_prefilter_20260715.md`'s drive-level target, whose ep-std of
0.07-0.16 — richer than plain G, but still described in today's other diag work as "73-dim
lineární hlava within-episode strukturu nevyjádří ani s bohatšími labely"). The candidates were
tested against the identical structural ceiling that already sank two label-shaping approaches
this week. **Between-episode R² (0.45-0.57, healthy) is where G's real variance lives** — and
candidates do not move that either (deltas ±0.003, inside noise, no consistent sign).

## Sample-size honesty

300 episodes / 4,803 states is smaller than the production replay buffer used in today's other
offline tests (~10,000 transitions, per `diag_capacity_probe.py`/`diag_drive_target_diff.py`).
This limits statistical power for detecting a *small* true effect. However: (1) the primary
verdict metric, mean|ΔV|=0.024, is **4x below** the roadmap's threshold, not marginally below
it — a small-sample confidence interval would need to be implausibly wide to flip this
conclusion; (2) the result is stable across 5 independent train/test splits (std=0.0039); (3)
the structural value-target-flatness ceiling applies regardless of sample size — more episodes
of the same G target would not manufacture within-episode variance that isn't there. A larger
sample could sharpen the between-episode R² estimate and might surface a small but real
between-episode lift from candidates that this run's noise floor (±0.003) can't resolve — that
would be the one thing worth re-running with more data, not the within-episode axis.

## Verdict: **NO-GO** on the full C++ per-player build, on this evidence

Per the roadmap's own stated bar (mean|ΔV|>0.1 or within-episode std materially up), today's
7 grounded candidate features **do not clear Phase A**. This is a faithful, reproducible null
result against the roadmap's actual gate, not a methodology artifact — feature distinctness is
confirmed (no collapsed states), the reconstruction pipeline was smoke-tested end to end, and
the result reproduces across 5 seeds.

**Two important qualifications, neither of which flips the verdict but both of which should
shape what happens next:**

1. **This does not contradict today's grounding findings.** Cage-corner Dodge×Tackle, BFS
   carrier_blitzable, and especially relative mobility advantage (30.8% prevalence, the
   strongest single result of the day) are real, observed, recurring patterns in how the AI
   plays — Phase A tests something narrower: whether *scalar aggregates* of those patterns move
   a *linear* value fit against the *current, structurally flat* MC-return target. Today's
   grounding pass repeatedly found the actual gap is in **action selection / macro visibility**
   (PASS chosen 1.4% of decisions, SCORE macro invisible to search 77% of the time, mobility
   advantage capitalized only 3.0% of the time when present) — these are search/policy problems,
   not value-function blind spots, so it is unsurprising they don't move V(s) much even when
   real.
2. **The value-target flatness ceiling is the higher-leverage blocker.** Per the staleness memo
   written earlier today (`evidence/fable_perplayer_staleness_20260715.md`), Phase B's own
   trigger condition already requires "vf_blend bring-up demonstrated V reaches into play at
   all" — this Phase A result adds a second, independent reason the value-feature channel is not
   currently the right lever: the label it would be trained against carries almost no
   within-episode information to learn from in the first place.

**Recommendation:** do not start the C++ per-player build on the strength of this candidate set.
Before revisiting per-player features, prioritize (a) the already-flagged macro-generation/
search-visibility fixes (PICKUP top-2/3 candidates, SCORE macro floor — both already GO per
`evidence/fable_replay_mining_20260714.md`) since that is where today's grounding consistently
found the actual behavioral gap, and (b) the vf_blend bring-up gate itself, since a value head
that never reaches into search cannot benefit from richer features regardless of what Phase A
says. If either of those unlocks a less-flat effective value signal, Phase A is cheap to re-run
(`python3 diag_perplayer_phaseA.py run main && ... fit`) against whatever target replaces plain
`mc_shaped` — this script and its 7 candidate features are reusable as-is.
