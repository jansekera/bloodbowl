# Throw-in bug investigation (2026-07-16)

Found while manually walking situation 4 of the replay survey with the user
(ball recovery sequence in `g0001.json.gz`, half 2 turn 3): a knocked-down
Skaven ball carrier dropped the ball, it scattered off the pitch, and the
resulting throw-in landed cleanly on an empty square with no further
movement. User (working from memory of the BB2020 throw-in rule) flagged
two suspected deviations, both confirmed against `engine/src/ball_handler.cpp`.

## Finding 1: missing mandatory final bounce after throw-in -- CONFIRMED, frequent

`resolveThrowIn` (`ball_handler.cpp:87-120`) rolls direction (d8) + distance
(2d6), computes the landing square, and:
- if a player is there and fails to catch -> calls `resolveBounce` (extra
  scatter) -- this path is correct.
- if the square is **empty** -> `state.ball = BallState::onGround(dest)`,
  full stop. **No final bounce is rolled.**

Per the BB2020 throw-in rule, the ball should bounce one additional square
(standard Bounce) after the throw-in direction+distance roll resolves,
regardless of whether the landing square is empty or occupied -- this step
is unconditionally missing for the empty-square case.

**Measured impact (150-game corpus, `diag_perplayer_grounding_data/main/`):**
- 45/150 games (30%) had at least one ball-goes-off-pitch event
- 54 total throw-in events across the corpus (~0.36/game)
- **53/54 (98%) landed on an empty square** -- the path that skips the
  mandatory bounce
- Only 1/54 landed on a player (correctly handled)

So in practice this isn't an edge case -- it fires on essentially every
throw-in observed. The existing test `BallHandler.ThrowIn`
(`engine/tests/test_ball_handler.cpp:148-156`) explicitly asserts the current
(missing-bounce) behavior and will need updating alongside any fix.

## Finding 2: throw-in reuses the uniform 8-way bounce template -- SUSPECTED, not yet confirmed with certainty

`resolveThrowIn` calls the same `scatterDirection(d8)` (uniform N/NE/E/SE/S/
SW/W/NW, `helpers.cpp:197-204`) used by normal bounces (`resolveBounce`) to
pick the throw-in direction. Per the user's recollection of the BB2020 rule,
throw-in direction should instead come from a dedicated **throw-in template**
keyed to which third of the sideline the ball went out from, with directions
biased back onto the pitch -- not a uniform 8-way roll. Current code does
clamp the final destination to stay in-bounds (`ball_handler.cpp:99-104`), so
it never actually places the ball off-grid, but a clamp is a crude
approximation of the real template's distribution, not equivalent to it.

**Confidence: lower than Finding 1** -- based on rules memory (user's +
mine), not yet cross-checked against a written rules reference in this repo.
An authoritative BB2020 rules-text lookup for the exact throw-in template is
likely to be hard to track down cheaply (per user) -- treat as a flagged
suspicion for later, not a blocker on shipping Finding 1's fix.

## Finding 3: throw-in AND kick-off catch modifier -- RETRACTED, CONFIRMED via primary source

Went through two rounds of suspicion (first "throw-in -1", then "throw-in
AND kick-off both -1, grouped separately from ordinary bounce") before
settling this with an actual downloaded rules reference: the official
"Blood Bowl reference compendium" cheat sheet PDF
(manticoredesign.wordpress.com), **CATCHING MODIFIERS** table, page 3:

```
Catching an accurate pass                                     +1
Catching an missed pass, bouncing ball, kick-off or throw-in   +0
Per opposing tackle zone on the player                         -1
Attempting to land after an inaccurate throw                   -0
Per opposing tackle zone on the square the player is thrown to -1
```

**Kick-off and throw-in are explicitly +0**, same bucket as a normal
bounce/missed pass -- confirms the brief's existing Catching modifiers table
and confirms current code (`modifier=0` in both `ball_handler.cpp`'s
`resolveThrowIn` and `kickoff_handler.cpp:311`) is correct. Not a bug. This
closes the loop opened by the two earlier (incorrect) memory-based
suspicions.

Note: this reference is a compact modifiers cheat sheet, not the full
rulebook -- it does NOT cover the throw-in direction/distance/bounce
*procedure* itself, so it doesn't confirm or deny Findings 1 or 2.

**Source of the confusion (user, 2026-07-16):** newer Blood Bowl rules
editions reworked the catching-modifier table significantly (e.g. some
`bloodbowlbase.ru`/community sources found during this same search session
described "-1" for throw-in/bounce, contradicting the compendium PDF above)
-- the user's rules memory was drawing on the reworked/newer edition, not
the one this engine targets. Reinforces the existing brief note ("Implementovat
podle engine kódu, ne online zdrojů" -- `team1_brief_per_player.md:304`):
when checking rules against web sources, cross-reference the **edition**
explicitly, since modifier tables can differ significantly between them.

## Recommendation (not yet actioned)

Per this project's standard process (see `feedback_bugfix_priority_over_speed`,
`feedback_implementation_style`): before implementing,
1. Finding 3 is retracted and confirmed via primary source -- no action needed.
2. Finding 2 stays flagged but deprioritized (hard to verify cheaply) --
   don't block on it; revisit only if a reliable rules reference surfaces.
3. Write the fix for Finding 1 (add the mandatory final bounce for the
   empty-square path) + update `BallHandler.ThrowIn` test + add a regression
   test for the previously-missing bounce.
4. Full C++ test suite green.
5. Paired-seed A/B (same house methodology as the hasActed fix, see
   `evidence/fable_hasacted_fix_20260715.md`) to check for crash/watchdog
   regressions and characterize any behavior shift before pushing.

## Fix SHIPPED + VALIDATED (2026-07-16)

Implemented Finding 1's fix: `resolveThrowIn` (`engine/src/ball_handler.cpp`) now
delegates the final landing-square resolution to `resolveBounce()` instead of
manually re-implementing (and skipping, for the empty-square case) that step.
Removed the now-duplicated player-check logic. 418/418 C++ tests pass (1 new:
`BallHandler.ThrowInFinalBounceOntoPlayer`; existing `BallHandler.ThrowIn`
updated to expect the mandatory extra bounce).

**Paired-seed A/B (N=300, same house methodology as the hasActed fix):**

```
[baseline (pre-fix)]  69W 164D 67L  draws 54.7%  TD/game 0.52  decisive share 50.7% (n=136)
[candidate (post-fix)] 56W 170D 74L  draws 56.7%  TD/game 0.49  decisive share 43.1% (n=130)

PAIRED A/B (draw)      post-fix 56.7% vs pre-fix 54.7%  delta +2.0pp  SE 4.0pp
                       95% CI [-5.8, +9.8]pp  VERDICT: INCONCLUSIVE

PAIRED A/B (home_win)  post-fix 18.7% vs pre-fix 23.0%  delta -4.3pp  SE 3.4pp
                       95% CI [-10.9, +2.3]pp  VERDICT: INCONCLUSIVE

PAIRED TD/game         post-fix 0.49 vs pre-fix 0.52  delta -0.033  SE 0.046
                       95% CI [-0.123, +0.057]  -- includes 0, unlike the
                       hasActed fix's TD/game shift; consistent with this bug
                       being far rarer (~0.36 throw-ins/game vs hasActed's
                       13.3% of team-turns)

watchdog skips: pre-fix 0/300, post-fix 0/300
```

**Primary validation goal met: no crash/watchdog regression.** All three
behavior metrics are inconclusive at this N, exactly as expected for a rare
mechanic -- no material draw-rate/TD shift needed to trust the fix, since
the goal here (unlike hasActed) was never "detect a behavior change," just
"confirm nothing broke." Scripts: `diag_throwin_fix_ab_20260716.py`,
`run_throwin_ab_20260716.sh`; log: `diag_throwin_ab_20260716.log`; arms:
`arm_throwin_base_20260716.json` / `arm_throwin_fix_20260716.json`.
