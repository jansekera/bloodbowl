# Phase A retest: clean corpus + drive-level target (2026-07-16)

Follow-up to `evidence/fable_perplayer_phaseA_20260715.md` (original Phase A, NO-GO,
mean|dV|=0.024), which flagged two unresolved caveats:

1. The 150-game source corpus (`diag_perplayer_grounding_data/main/`) predates the
   hasActed fix (shipped 2026-07-16) — a systematic check found 7/12 curated situations
   in the accompanying replay-grounding survey showed the bug's illegal-reactivation
   signature (see `project_bloodbowl_survey_hasacted_contamination_20260716` memory).
2. The ridge fit was tested against the standard `mc_shaped` target G, which is
   near-flat within an episode (ep-std 0.026-0.030) — the same structural ceiling that
   sank `mc_td_mix` and the drive-level target (as a *training* target) twice before.
   A fairer test was proposed (never run until now): retest against the drive-level
   target instead, which carries more within-episode structure.

This run does both fixes at once.

## What changed vs the original run

- **Corpus**: regenerated 150 games (`diag_perplayer_grounding_data/main_postfix/`) on
  the current engine (both the hasActed fix, commit `0bb378b`, and the throw-in fix,
  commit `2c4ff02`, are in this build). Same generation script
  (`diag_perplayer_grounding.py run main_postfix 150`), same seed base/race cycling as
  the original.
- **Target**: added a drive-level target T alongside G, reconstructed from the turn
  snapshots' `home_score`/`away_score` deltas (a `reward_step` equivalent, since these
  episodes come from the JSON snapshots, not `replay_buffer.pkl`'s `Transition` objects).
  Formula matches `diag_drive_target_diff.py`'s `build_drive_target` exactly:
  `T_t = clip(lam*G_t + (1-lam)*gamma^(k_d-t)*drive_outcome, -1, 1)`, using
  `lam=0.5, D=0.6, d0=0.1` — one exact grid cell already tested in that script (not a
  new ad-hoc choice).
- Everything else (73-dim baseline features via the real `bb_engine.extract_features`,
  the same 7 grounded candidate features, ridge methodology, episode-level 80/20 split,
  5 seeds) is unchanged from the original Phase A script — reused via import, not
  reimplemented.

Script: `diag_perplayer_phaseA_drivetarget_20260716.py` (`run` reconstructs+caches
against `main_postfix`, `fit` runs both the G-target and T-target ridge comparisons in
one pass). Cache: `diag_perplayer_phaseA_drivetarget_cache_20260716.pkl`.

## Results

300 episodes (150 games x 2 perspectives), 4,806 states (vs 4,803 in the original —
trivial difference from re-simulation, not a discrepancy). Feature distinctness
unchanged (1.000, no collapsed states). Candidate prevalence is close to the original
run's numbers (e.g. `mobility_advantage_progress` nonzero 24.8% here vs 23.1%
originally, `carrier_blitzable_bfs` 12.8% vs 16.6% — normal game-to-game variation from a
fresh 150-game sample, not a methodology change).

**T's within-episode structure**: ep-std = 0.0604 (whole dataset) vs G's 0.0262 — **2.3x
richer**, consistent with the 2.5-5.8x range reported for the drive-target-as-training-
target experiment (this is a different, smaller sample and a fixed single grid cell
rather than the full grid, so isn't expected to match exactly).

| target | mean\|dV\| | roadmap bar | verdict |
|---|---|---|---|
| **G** (reference, same methodology as 2026-07-15, clean corpus) | **+0.0229** (std 0.0060) | >0.1 | NO-GO, ~4.4x below bar |
| **T** (drive-level, the fair retest) | **+0.0141** (std 0.0049) | >0.1 | NO-GO, ~7.1x below bar |

Both `corr(baseline, combined)` stay at 0.996-0.996 (candidates barely move the fit's
predictions either way). `dR2`/`dMSE` are inside noise and sign-inconsistent across
seeds for both targets, same pattern as the original run. Candidate coefficients
(standardized) stay small (|coef|<0.05) and flip sign across seeds for several
candidates under both targets — no candidate emerges as a consistent driver against
either target.

**The ep-std-recovered-vs-ceiling numbers are unusual for a different reason, not a bug in this test:**
both arms *overshoot* the label's own within-episode std by roughly 2-8x (e.g. baseline
predictions have ep-std 0.206 against G's ceiling of 0.026 — 798%; 0.117 against T's
ceiling of 0.057 — 205%). This means the linear model isn't under-fitting within-episode
variation, it's producing MORE swing than the true label has, which is why within-episode
R2 is strongly negative for both baseline and combined under both targets (as it was in
the original run against G) — the ridge fit is picking up spurious within-episode
correlations from the 73 aggregate features that don't track either target's actual
game-to-game structure. This is the same failure mode already documented, now confirmed
to persist against the richer T target too, not just G.

## Verdict: NO-GO confirmed, on cleaner grounds than before

**Neither the corpus fix nor the richer target flips the original conclusion.** If
anything, mean|dV| is *smaller* against the richer T target (0.014) than against G
(0.023) — the candidates move the fit even less when there's more true structure to
capture, not more. This directly answers the open question from
`project_bloodbowl_phaseA_result_20260715`: the original NO-GO was not an artifact of
G's flatness or of hasActed-contaminated data. Per the decision rule in
`project_bloodbowl_survey_hasacted_contamination_20260716`: since this directly-related
retest's verdict did NOT flip, there is no positive signal to widen re-evaluation to
`mc_td_mix`/drive-target-as-training-target/`capacity-vs-features` — their structural
"73-dim linear features are the bottleneck" conclusions should be treated as holding,
not as needing a fresh look purely because of the hasActed bug.

**Recommendation unchanged from the original Phase A report:** do not start the full
C++ per-player build (~492 features, new network head) on the strength of this
candidate set. The macro-generation/search-visibility gaps repeatedly found during
today's (2026-07-16) replay-grounding deep-dive (ADVANCE never selected for a
long-stalled carrier despite active heuristic-pull terms already rewarding it, PICKUP
available-and-free-but-ignored, defensive coverage collapsing to one band/rvačka) remain
the higher-leverage next targets, not per-player value features.
