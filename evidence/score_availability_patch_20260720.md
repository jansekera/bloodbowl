# SCORE-availability patch (A+B): applied, unit-tested, measured — result AMBIGUOUS (2026-07-20)

## What was applied

`proposals_score_availability_20260714.md`'s two patches, written 07-14 and
never applied until today (found via a systematic sweep for other
"designed but forgotten" proposals, see
`project_bloodbowl_unapplied_proposals_audit_20260720` memory):

- **Patch A** (`macro_mcts.cpp`): direct SCORE with a safe walk-in (no GFI
  needed) gets a 0.30 prior floor, applied after the existing
  turnsRemaining-based floor chain, whenever the computed floor would
  otherwise be lower.
- **Patch B** (`macro_actions.cpp`): carrier-activation guard — while a
  direct SCORE is on the table, the carrier may not be offered as BLOCK/
  FOUL attacker, and generic BLITZ expansion skips the carrier as blitzer
  candidate.

Patch B was added mid-validation, not part of the original plan to apply A
alone first: a diagnostic replay of the negative-control unit test (raw
`childVisits` dump) showed Patch A alone left the carrier vulnerable to
having its own activation "stolen" by a generic BLITZ macro when it was the
only player in range to blitz an adjacent marker — direct empirical
confirmation of the mechanism (L1b) Patch B was designed to close.

## Unit-level validation

426/426 tests pass (420 pre-existing + 6 new: 2 in `test_macro_mcts.cpp`,
4 in `test_macro_actions.cpp`). The negative-control protocol from the
proposal was followed for all 6: pre-patch fails on the intended assertion,
post-patch passes. One genuine bug found and fixed **in the test design**,
not the patch: the original `MidGameSafeWalkInPrefersScore` scenario placed
a marker adjacent to the carrier's start square, which forces a Dodge roll
on the first movement step — not actually the "safe walk-in" the test
claims to represent. Fixed by moving the marker to a non-adjacent square
(the test's own "mild contest" role is preserved via BLITZ/CAGE/REPOSITION
candidate mass elsewhere on the board). The proposal's authors state
explicitly they never ran this test before writing it; this is the first
time it was actually executed.

## Game-level paired-seed A/B (primary metric)

N=150 pairs, `off` = HEAD (a5dd758, pre-patch, git worktree build) vs
`on` = this tree (patches applied), same weights, same MCTS=100/TV=1200,
base_seed=20260720 (`diag_score_availability_ab_20260720.py`,
`arm_scoreavail_ab_{off,on}_20260720.json`).

| | off | on | delta |
|---|---|---|---|
| draws | 46.0% (69/150) | 43.3% (65/150) | **-2.7pp** |
| home_win | 32.0% | 30.0% | -2.0pp |
| TD/game | 0.73 | 0.74 | +0.01 |

Paired McNemar: draw delta -2.7pp, SE 3.6pp, **95% CI [-9.8, +4.5]pp, p=0.58,
INCONCLUSIVE** (CI includes zero). home_win: delta -2.0pp, CI [-8.0,+4.0]pp,
also INCONCLUSIVE. Direction is consistent with the patch's intent (offensive
fix -> fewer draws), matching the proposal's own expectation ("opposite
direction from screen fixes"), but per the project's established noise-floor
rule this is not evidence of an effect by itself.

## Decision-level sanity check (mechanism check, meant to disambiguate)

Per the proposal's own §6 validation plan (step 2, cheap check before
escalating N): measured, over a fresh 30-game batch per arm
(`diag_score_availability_decisionlevel_20260720.py`, base_seed=20260731,
distinct from the game-level A/B's seeds), what fraction of decisions where
a SCORE-family macro is a candidate at all (any visited action with the
SCORE one-hot feature set) have it as the top-visited (chosen) candidate.

| arm | SCORE-family candidate decisions | chosen (top visit) | rate |
|---|---|---|---|
| off | 124 | 34 | **27.4%** |
| on | 116 | 32 | **27.6%** |

**Essentially flat — no detectable shift (+0.2pp).** This does NOT match
the proposal's own predicted jump (7.7% -> >30%). Two things temper how much
weight to put on the shortfall:

1. **The 7.7% baseline is stale.** It was measured 07-14, before the
   hasActed fix, throw-in fix, and ADVANCE floor fix all shipped (each of
   those reshapes which macros compete for prior mass at any given state).
   Today's `off` arm (current HEAD, all those fixes already in) measures
   27.4% -- 3.5x higher than the reference point the patch was designed
   against. The whole prior-competition landscape has already shifted
   independent of this patch, which may mean today's `off` baseline is much
   closer to whatever ceiling this floor mechanism can reach.
2. **A synthetic-state diagnostic (ad hoc, not part of the permanent suite,
   see `MidGameSafeWalkInPrefersScore` calibration history above) already
   proved the floor mechanism itself is NOT broken** -- in a controlled
   scenario with a genuinely risk-free safe walk-in and no confounding
   marker, SCORE won 136/400 visits (34%) after the patch, matching the
   proposal's own hand-estimate almost exactly. So this isn't "the patch
   does nothing" -- it's "the patch's effect isn't showing up at a
   detectable rate in this 30-game/~120-decision sample of real, unscripted
   games," which could be small-sample noise, or could mean genuinely
   risk-free safe-walk-in states (no adjacent marker at all) are rarer in
   real play than the aggregate can-score metric suggests.

## Refined decision-level split (2026-07-20, follow-up)

The aggregate 27.4%/27.6% check above mixes states where the carrier is
genuinely free (the patch's actual target) with states where the carrier is
marked (where `directSafeWalkIn` is misleadingly true but the real risk is
high, per `evidence/marked_carrier_no_blitzer_gap_20260720.md`) — a real
concern that the aggregate could be diluting a hidden effect. Re-ran the
same 30-game/arm batch with the carrier's tacklezone count captured at each
decision (reusing the board-snapshot infra, zero new engine changes) and
split into `free` (carrier_tz==0, the patch's actual target subset) vs
`marked` (carrier_tz>=1):

| bucket | off | on |
|---|---|---|
| free (TZ=0) | 41.7% (25/60) | 41.1% (23/56) |
| marked (TZ>=1) | 14.1% (9/64) | 15.0% (9/60) |

**Still flat in both buckets.** This is a stronger negative signal than the
aggregate check: even isolated to exactly the subset the floor boost
targets, there is no detectable shift. The most likely explanation: in
today's engine (post hasActed/throw-in/ADVANCE-floor fixes), SCORE already
wins 41.7% of free-carrier can-score decisions on the OLD 0.08 floor —
meaning that floor was often not the binding constraint to begin with (the
turnsRemaining<=2/<=4 branches, 0.35/0.20, already exceed the old 0.08 and
likely dominate in most observed late-drive scoring opportunities, which is
where most genuinely marker-free scoring chances cluster). The floor boost
to 0.30 only matters in the specific generic mid-game branch (turnsRemaining
>4, no other condition, base floor 0.08) — which may simply be rarer in
practice than the aggregate "can-score" framing suggested when the proposal
was designed against 07-14 data.

## Verdict: AMBIGUOUS, not a clean GO or NO-GO

Unlike the three-times-confirmed per-player NO-GO
(`evidence/phase_a_policy_gate_20260720.md`) or the clean, reproducible
CAGE/ADVANCE wins, this patch currently has:
- a real, correctly-firing mechanism (proven in isolation),
- a game-level trend in the right direction but not statistically
  distinguishable from noise at N=150,
- a decision-level check that failed to confirm the mechanism is moving the
  needle in real games at the sample size measured.

**Update after the refined split (see above): leaning NO-GO, not just
inconclusive.** The targeted decision-level check (exactly the subset the
patch affects) came back flat too, not just the diluted aggregate — two
independent decision-level measurements now agree there's no detectable
mechanism-level effect in real games, even though the mechanism is proven
to work in isolation. Escalating the game-level A/B to N=400 no longer
looks like the right next move: if the floor boost isn't measurably firing
at the decision level even in its own target subset, a bigger N mostly buys
a tighter confidence interval around what is likely a genuinely small or
null game-level effect, not a chance to "discover" a real one hiding in
noise — the same trap lever-c fell into (N150 promising -> N400 promising
-> N800 reversed). Recommend NOT shipping this patch and NOT escalating
further without new evidence changing the picture (e.g. a state-space
argument for why the generic mid-game floor branch should be more common
than these ~120-decision samples suggest).

## Status

Applied to the working tree (uncommitted as of this writing), not yet
shipped/pushed pending a clearer verdict. Board-snapshot logging
infrastructure (from the earlier per-player policy-gate investigation
today) was reused for the decision-level check with zero additional engine
changes needed.
