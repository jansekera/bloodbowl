# hasActed double-activation fix: activation close-out at actor switch (2026-07-15, Fable 5)

Fixes the CONFIRMED bug from `evidence/fable_hasacted_bug_20260715.md`: a
successful MOVE never set `hasActed`, so a player could be independently
reactivated later in the same team-turn (free BLOCK/PASS/FOUL after other
players acted in between) — 13.3% of team-turns, 145/150 games.

## The fix (diff summary)

Preferred mechanism from the bug report: activation close-out at the
actor-switch boundary, one mechanism covering both the primitive
(rules_engine/MCTS/game_simulator) and macro layers, since every execution
path funnels through `executeAction`.

- `engine/include/bb/game_state.h`: new field `int currentActivationId = -1`
  on `GameState` — id of the player whose activation is currently open.
  Copied by `clone()` (trivial member), not exposed to Python (bindings list
  members explicitly), no serialization impact.
- `engine/src/action_resolver.cpp` (`executeAction`, before `resolveAction`):
  for player actions (`requiresPlayer && playerId > 0`), if
  `currentActivationId` is set and differs from the incoming `playerId`, the
  previous player's activation is closed: `prev.hasActed = true` iff
  `prev.hasMoved` (a successful move was the only way to leave an activation
  open). Then `currentActivationId = action.playerId`.
- `engine/src/game_state.cpp` (`resetPlayersForNewTurn`): resets
  `currentActivationId = -1`. This single reset point covers END_TURN and
  turnover (both via `resolveEndTurn`) and new drives (both kickoff call
  sites: `kickoff_handler.cpp:216`, `game_simulator.cpp:343`).

Preserved by design: multi-step movement and continuous same-player
sequences (move -> pass/foul/score) — the close-out fires only on an actor
SWITCH. Frenzy's temporary `hasActed` resets inside a single `resolveBlock`
call are untouched. Killed by design: interleaved reactivation, including
the "free blitz" (move, others act, then BLOCK) and carrier-revisit macro
plans (ADVANCE now, SCORE later) — the latter is an expected behavior
change, not a regression.

## Tests

- Full C++ suite: **417/417 PASS** (414 pre-existing + 3 new; was 414 before
  this change).
- Python suite (`python/tests` + `engine/python/test_bb_engine.py`):
  **167 passed, 1 skipped** against the rebuilt post-fix engine (venv is
  py3.12; the fresh `bb_engine.cpython-312` .so is the one loaded).
- New tests in `engine/tests/test_action_resolver.cpp`:
  - `InterleavedReactivationClosedOut` — NEGATIVE CONTROL: player 1 completes
    a successful move ending adjacent to an opponent, player 2 then acts;
    asserts player 1 `hasActed == true` and that `getAvailableActions` offers
    player 1 nothing (pre-fix it offered the free BLOCK). Verified to FAIL
    against pre-fix behavior: with the close-out hook disabled
    (`if (false && ...)`) and everything else identical, the test fails
    exactly on the buggy assertions ("[ FAILED ] ActionResolver.
    InterleavedReactivationClosedOut"). With the hook restored it passes.
  - `SamePlayerMultiStepMoveStaysOpen` — positive control: two move steps by
    the same player keep the activation open (`hasActed == false`, actions
    still offered).
  - `ActivationTrackerResetsOnEndTurn` — tracker returns to -1 at the turn
    boundary; the next turn's first actor causes no spurious close-out.

## Paired-seed A/B (new baseline)

Methodology per `diag_utils.py` house standard: mirror null games
(cand = frozen = `weights_best.json`), N=300 paired seeds
(base_seed=20260716), production gate schedule (`cand_is_away = i % 2`),
MCTS=100, TV=1200, vf_blend=0.0, policy=`weights_policy.json`. Baseline arm
on the pre-fix binary (fix stashed), candidate arm on the post-fix binary,
same seed list; McNemar on draws + paired TD/game delta.
Script: `diag_hasacted_fix_ab_20260715.py`; pipeline:
`run_hasacted_ab_20260715.sh`; log: `diag_hasacted_ab_20260715.log`;
arms: `arm_hasacted_base_20260715.json` / `arm_hasacted_fix_20260715.json`.

Behavior change was EXPECTED (illegal continuations removed from the action
space); the validation goals were (a) no watchdog/crash regression,
(b) establish the new baseline.

RESULTS:

```
[baseline (pre-fix)]  69W 165D 66L  draws 55.0%  TD/game 0.54  decisive share 51.1% (n=135)
[candidate (post-fix)] 56W 180D 64L  draws 60.0%  TD/game 0.45  decisive share 46.7% (n=120)

PAIRED A/B (draw)      post-fix 60.0% vs pre-fix 55.0%  delta +5.0pp  SE 3.9pp
                       95% CI [-2.7, +12.7]pp  McNemar z=+1.27  p=0.2349
                       VERDICT: INCONCLUSIVE (CI includes 0)

PAIRED A/B (home_win)  post-fix 18.7% vs pre-fix 23.0%  delta -4.3pp  SE 3.1pp
                       95% CI [-10.4, +1.7]pp  McNemar z=-1.39  p=0.1980
                       VERDICT: INCONCLUSIVE (CI includes 0)

PAIRED TD/game         post-fix 0.45 vs pre-fix 0.54  delta -0.093  SE 0.045
                       95% CI [-0.181, -0.005]  -- excludes 0, consistent with
                       illegal bonus-action scores being removed

watchdog skips: pre-fix 0/300, post-fix 0/300
```

No crash/watchdog regression (the actual validation goal). Draw-rate and
home_win shifts are directionally consistent with removing illegal bonus
actions but not statistically separable from noise at N=300 (see
[[feedback_draw_rate_noise_floor]] — <10pp delta at this N is inconclusive
by house standard). TD/game shift is the one metric whose CI excludes 0,
matching the mechanism directly (fewer illegal bonus BLOCK/PASS/FOUL scoring
chances after the fix).

## New baseline (post-fix)

draws 60.0%, TD/game 0.45, decisive share 46.7% (n=120), home_win 18.7%,
0/300 watchdog skips. This replaces the pre-fix numbers as the reference
point for all future A/B comparisons (vf_blend Phase 0 was measured on the
buggy pre-fix engine and should be re-run against this baseline before being
trusted).
