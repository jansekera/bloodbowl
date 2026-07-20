# Item 7 — top-2 PICKUP pickers: applied, KEEP (2026-07-20)

## What was applied

`proposals_item7_pickup_top2_20260714.md`, third item worked from the
unapplied-proposals audit. `engine/src/macro_actions.cpp` +
`engine/src/macro_mcts.cpp` (3 parts, applied together):

1. **Generation**: `getAvailableMacros` now emits up to 2 PICKUP candidates
   (best + second-best picker by the existing `AG*10 - dist*3 + skill
   bonuses` score), gated to a max 15-point score gap. Previously only ever
   emitted a single `bestPicker`, leaving search with zero alternative when
   the best picker's use elsewhere (block/blitz/screen) might have been
   better.
2. **Observability** (pure addition, zero behavior change): `MacroChildVisitInfo`
   gained a `prior` field, populated from the already-computed post-renorm
   root prior — needed for the T4 regression test, no other consumer.
3. **Floor-split**: the secondary PICKUP candidate gets HALF the primary's
   floor (`pickupSeen` counter in the priors loop), keeping the family's
   floored prior mass growth at ~1.5x instead of a naive 2x (avoids
   "interaction risk #1" from the master list -- doubling would have
   diluted every other candidate by ~15%, the split keeps it at ~9%).

## Prerequisites and sequencing

Per the proposal's own section 5.1, item 7's measurement window must not
overlap item 10's (both touch the same prior-renorm pool, though on
disjoint node types -- PICKUP only exists on `ballOnGround` states, which
forces `onDef=false`, so items 7 and 10's floors never bind on the same
node; see proposal section 6.1). Applied and measured AFTER item 10
(`0f70dab`) was shipped, matching that constraint (deviates from the
proposal's stated master order of 7-before-10, per explicit user direction
today, but the hard non-overlap rule was respected either way).

## Unit-level validation

428/428 tests pass (424 + 4 new: `PickupEmitsTopTwoPickersBestFirst`,
`PickupSecondPickerGatedByScoreGap`, `PickupSinglePickerUnchanged`,
`SecondaryPickupPriorIsHalfOfPrimary`). Three-stage negative-control
protocol followed exactly as specified:
1. Tests + observability only: T1 failed at count==1 (not 2); T2/T3 passed
   as guards; T4 failed at the `ASSERT_EQ(...,2)` precondition (no second
   candidate exists yet).
2. + generation (part 1): T1-T3 passed; T4 failed specifically at
   `EXPECT_NEAR(primary/secondary, 2.0, ...)` with the actual ratio == 1.0
   -- empirically proving the naive-doubling risk is real before the fix,
   not just a paper concern.
3. + floor-split (part 3): all 4 pass, ratio == 2.0 exactly.
`BranchingFactorReasonable` (existing test, caps candidate count) stays
green -- item 7 adds at most +1 candidate, well within its margin.

## Paired-seed A/B with decision-level counters (N=150)

`diag_item7_pickup_top2_ab_20260720.py`, `off` = HEAD (`0f70dab`, item10
applied, item7 not -- fresh git worktree) vs `on` = this tree (item7
applied), same weights, MCTS=100/TV=1200, base_seed=20260720.

**Honest proxy substitution:** the proposal's stated primary metric is
"per-turn ground recovery" (mined from turn-level replay logs). This
measurement instead used a decision-level proxy available directly from
`get_policy_decisions()`: "PICKUP chosen (top-visited) rate among loose-ball
decisions" -- a related but not identical quantity (decision-level choice
frequency, not turn-level recovery success). Treat the result below as
strong evidence for the mechanism, not a literal confirmation of the
doc's exact stated metric.

| metric | off | on | delta | CI | verdict |
|---|---|---|---|---|---|
| PICKUP chosen rate (loose-ball decisions) | 48.01% | 57.48% | **+9.47pp** | [+5.54,+13.39]pp | **CONFIRMED** |
| PICKUP family visit-mass (over-crowding tripwire, red line >60%) | 25.70% | 35.35% | +9.65pp | [+7.41,+11.88]pp | clean, well under 60% |
| REPOSITION+BLITZ chosen rate (collateral dilution) | 40.02% | 30.38% | -9.65pp | [-13.02,-6.27]pp | expected magnitude (doc predicted ~-9%) |
| def_engage_rate (attribution guard -- item7 shouldn't touch defense) | 31.71% | 31.39% | -0.32pp | [-2.46,+1.83]pp | clean, no leak |
| draws (tripwire) | 48.7% | 48.7% | +0.0pp | [-11.2,+11.2]pp | flat, no concern |
| home_win (tripwire) | 28.0% | 27.3% | -0.7pp | [-10.9,+9.5]pp | flat, no concern |
| watchdog-skip | 0/150 | 0/150 | -- | -- | clean |

This is the cleanest result of today's three items: the primary mechanism
is not just directionally right but **statistically confirmed** (CI clears
zero by a wide margin), the over-crowding tripwire and attribution guard
are both clean, and the collateral dilution lands within the range the
proposal's own worked-example math predicted (not an unexpected blowout).

## Verdict: KEEP, shipped

Clear positive signal on the mechanism this patch targets, no red-line
violations, no attribution leaks, no draw-rate tripwire. Committed and
pushed.
