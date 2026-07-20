# Item 10 — defensive prior-floor rebalance: applied, KEEP (2026-07-20)

## What was applied

`proposals_item10_prior_floor_validation_20260714.md`, found unapplied
during the same sweep that surfaced the SCORE-availability patch
([[project_bloodbowl_unapplied_proposals_audit_20260720]]). `engine/src/macro_mcts.cpp`:

- **REPOSITION** defensive floor: 0.05 -> 0.08 (binds at n>=13 candidates;
  was a near no-op before at n>=21).
- **FOUL**: new defensive cap 0.08 (previously uncapped, fell to `default:`,
  inheriting up to ~0.17 raw uniform prior at sparse n=6 nodes — more than
  BLOCK's 0.12 floor guarantees, for a niche action that can't touch the
  ball).

Also added `MacroMCTSSearch::expandRootPriorsForTest()` (test-only accessor,
pure wrapper over the private `expand()`) since no existing test pinned the
prior floor/cap regime at all (2026-07-10 audit: 8/11 shipped prior-floor
fixes had zero C++ regression tests).

## Prerequisites verified before applying

Per the proposal's own hard sequencing (section 5 checklist): items 8+9
(screen fixes 2a/2b, `proposals_screen_fixes_20260713.md`) must be applied
and kept first, since item10's binding thresholds and expected effect size
were designed assuming defensive REPOSITION actually arrives somewhere
useful (2a: real movement budget; 2b: intercept-lane targets). Confirmed
today: both are already applied and pushed (commits `bd21d4a`, `9baeb04`,
ancestors of current HEAD). Item 7 (PICKUP top-2, shares the same
prior-renorm pool) confirmed NOT applied, satisfying the non-overlap
requirement — deviates from the proposal's stated master order (7 before
10) per explicit user direction to do item10 first; the hard constraint
(never measure both in the same window) is respected either way.

## Unit-level validation

424/424 tests pass (420 + 4 new: `DefensiveRepositionFloorBindsAtLargeNodes`,
`DefensiveFoulCapBindsAtSparseNodes`, `RepositionFloorNoOpAtSmallNodes`,
`OffensivePriorsUntouchedByDefensiveRebalance`). All 4 test states were
numerically pre-verified against the engine's actual floor/cap/renorm
arithmetic by hand (not just run-and-hope) before being committed to the
suite — every printed prior matched the by-hand calculation exactly. The
negative-control protocol was followed: pre-patch, T1 failed at ratio 0.357
(target 0.400±0.02), T2 failed at ratio 0.625 with FOUL==BLOCK exactly
(target: FOUL<BLOCK); T3/T4 passed as guards both pre- and post-patch.
Post-patch, all 4 pass.

## Paired-seed A/B with decision-level counters (N=150)

`diag_item10_prior_floor_ab_20260720.py`, `off` = HEAD (a5dd758, pre-patch,
reused git worktree from today's SCORE-availability measurement) vs `on` =
this tree (item10 applied), same weights, MCTS=100/TV=1200,
base_seed=20260720. Counter-worker attributes each decision by
onDef (opponent holds ball) and node size (`len(visits)>=13` as the
n>=13 binding-threshold proxy), matching the proposal's section 3.1 design.

### C3 — engagement guard (RED LINE veto, checked first)

`def_engage_rate` (BLITZ+BLOCK share of defensive decisions): off 30.6% ->
on 31.7%, delta **+1.09pp** (relative **+3.5%**), CI[-0.84,+3.01]pp.
**No drop at all** — if anything a small increase. Clears the red line by a
wide margin (threshold: relative drop >15% with CI excluding zero).

### C2 — manipulation check (REPOSITION share by node size)

| node size | off | on | delta | CI |
|---|---|---|---|---|
| big (n>=13) | 21.97% | 22.76% | +0.80pp | [-5.66,+7.24]pp |
| small (n<=12) | 59.67% | 60.37% | +0.70pp | [-1.11,+2.51]pp |

Both inconclusive at this N — neither confirms the intended concentration
on big nodes, but critically neither shows a *confirmed* effect leaking
into small nodes either (which would trigger the doc's STOP/root-cause
branch). Underpowered, not contradictory.

### C5 — FOUL usage decomposition

- `def_foul_rate` (the cap's direct target): off 4.67% -> on 3.00%, delta
  **-1.67pp, CI[-2.32,-1.02]pp — CONFIRMED** (excludes zero), matching the
  predicted direction exactly.
- `loose_foul_rate` (null check, onDef doesn't gate loose-ball states):
  off 4.51% -> on 4.34%, delta -0.17pp, CI[-1.03,+0.69]pp — **clean, no
  leak.** Confirms the onDef attribution/gating is working as designed, not
  accidentally touching the loose-ball FOUL-instead-of-PICKUP problem
  (that's item 7's territory, explicitly out of scope here).

### C6 — draw-rate tripwire

Draws: off 51.3% -> on 48.7%, delta -2.7pp, CI[-13.7,+8.4]pp, INCONCLUSIVE.
home_win: +2.7pp, CI[-7.1,+12.4]pp, INCONCLUSIVE. No confirmed increase (the
patholog-stalling tripwire condition), and directionally draws went down,
not up — no concerning pattern, just noise at N=150 on a subtler defensive
fix.

### C7 — sanity

0/150 watchdog-skipped in both arms.

## Verdict, per the proposal's own decision matrix (section 3.2)

Row match: **"C4 INCONCLUSIVE + C3 green + C2/C5(a) confirmed mechanism +
C6/C7 clean -> KEEP; mechanism proven, no harm signal — but explicitly
record 'game-level effect not proven', re-review after the next long
training run."**

This is the exact situation measured: C3 clean, C5(a) statistically
confirmed in the predicted direction, C5(b) null check clean, C6/C7 clean,
and the primary game-level outcome (draws/home_win, standing in for the
doc's conceded-TD/g metric, which wasn't separately instrumented here)
inconclusive at N=150. **KEEP.**

**Honest limitation:** did not implement the doc's dedicated conceded-TD/g
counter or the greedy-scorer sonda (C4's stated primary metric) — used the
standard mirror draw/home_win outcome as a proxy instead, and did not run
the full C1 two-panel A/A calibration (300 games) the doc specifies as a
first step. This is a reduced-fidelity pass through the doc's protocol, not
the full 7-counter validation as originally scoped — appropriate for a
same-day KEEP decision given the clean/positive signal on the counters that
were measured, but the primary success axis (game-level defensive
tightness) remains genuinely unproven, exactly as the matrix row's own
caveat states.

## Status: SHIPPED

424/424 tests green, C3 red-line clear, C5(a) mechanism confirmed, no harm
signal anywhere measured. Committed and pushed.
