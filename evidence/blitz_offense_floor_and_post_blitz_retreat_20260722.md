# Blitz: offense-side prior floor gap + post-blitz retreat gap (2026-07-22)

User-driven follow-up to yesterday's blitz doctrine
(`kickoff_setup_and_blitz_targeting_20260721.md`, item 3). Two new
findings, both confirmed in code, neither fixed yet.

## Finding 1: BLITZ has no MCTS prior floor on offense

`engine/src/macro_mcts.cpp:365-367`:

```cpp
case MacroType::BLITZ:
    if (onDef) minPrior = 0.20f;
    break;
```

BLITZ gets a floor (0.20 — the highest of any macro type) only when
`onDef` (defending). On offense it falls through with **zero** floor,
while SCORE-family/ADVANCE/CAGE/BLOCK/PICKUP all get offense-side floors
(0.12-0.35) in the same switch. Structurally identical to the ADVANCE
bug fixed 2026-07-16 (`project_bloodbowl_advance_floor_fix_20260716`):
that action was floorless unless trailing 2+ while BLOCK/CAGE had
unconditional floors, so it never accumulated enough MCTS visits to be
picked even when it was the objectively best move (0.8% pick rate in a
400-seed reconstruction despite highest one-ply Q). If offensive BLITZ
is in the same trap, the user's doctrine from yesterday ("blitz is
use-it-or-lose-it, bar for skipping should be very high") is being
violated by search structure, not by correct evaluation.

**Not yet verified against real decision data** — needs the same
400-seed-reconstruction-style diagnostic the ADVANCE fix used before any
patch.

## Finding 2: no code path lets a blitzer retreat after a non-knockdown blitz

User's concrete case: Wood Elf Wardancer (best blitzer, high move) blitzes;
if he doesn't knock the target down, he should use leftover movement to
dodge away and retreat behind a lineman screen, so he isn't left exposed
to a return blitz/foul next opponent turn — he's too valuable to leave
stranded.

Two stacked gaps, either one alone would already block this:

1. **`hasActed` is set `true` unconditionally after block resolution.**
   `engine/src/block_handler.cpp:508` (`att.hasActed = true;`, the
   general resolution exit point) plus every other branch that ends a
   block (lines 189/210/217/230/361/388/422/447) all do the same. The
   only places it's reset to `false` are Frenzy's mandatory second block
   (line 517) and the multi-block skill's second target (line 568) —
   both immediately followed by another forced block, not a free choice
   to move. Real LRB6 rules allow a player to keep moving after a block
   if he has movement left and hasn't exceeded his MA (Blitz itself only
   requires the one square of movement into blitz range) — the engine
   currently can't represent this at all, for any blocker, not just
   blitzers. This is the root cause, and it's broader than the user's
   specific Wardancer case: it blocks ANY post-block movement, offense
   or defense, blitz or plain Block macro.
2. **Even if (1) didn't hold, `REPOSITION` candidate generation
   explicitly excludes players adjacent to a standing enemy**
   (`macro_actions.cpp:568-584`, `if (hasAdjacentEnemy) return;`) — which
   describes a blitzer standing next to a target he failed to knock
   down. So even a free-standing player in exactly this situation has no
   macro available to retreat.

Net effect: a blitzer who fails to get a knockdown is *always* left
stranded adjacent to the enemy for the rest of the turn, regardless of
remaining movement or the player's value — confirmed structural, not
speculative. Unlike Finding 1, this isn't a search/tuning question, it's
a genuine rules-fidelity gap (same category as
`project_bloodbowl_divingtackle_rules_deviation_20260716`, but larger
blast radius: affects every completed block, not one niche skill
interaction).

## Not done here

No code changed, no diagnostic run yet. Priority/ordering recorded in
`project_bloodbowl_day_20260722.md`.
