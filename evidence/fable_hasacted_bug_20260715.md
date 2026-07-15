# hasActed bug: successful MOVE never closes a player's activation (2026-07-15, Fable 5)

## Verdict: CONFIRMED BUG (systemic, 13.3% of team-turns, 145/150 games)

A player who completes a successful Move keeps `hasActed = false` and remains fully
eligible to be independently reactivated later in the same team-turn for a second
action (BLOCK / PASS / FOUL / HAND_OFF), after other players have acted in between.
This violates the one-activation-per-player rule and the engine's own evident intent.
No code was changed in this pass (confirm + measure only, per convention).

## Code trace (where the gap is)

Gate: `engine/include/bb/player.h:31` (`bool hasActed = false;`) and `:41`
`canBeActivated()`/`canAct()` = `bb::canAct(state) && !hasActed && !lostTacklezones`.
This is the ONLY per-player gate; team-level flags (`blitzUsedThisTurn`,
`passUsedThisTurn`, `foulUsedThisTurn`) are per-turn resource caps, not activation
tracking. There is NO "current activation" concept anywhere: the turn loop
(`game_simulator.cpp:445/592`, `mcts.cpp:95`, `macro_mcts.cpp:111/257`) just
re-generates legal actions/macros after every primitive/macro and picks again.

Where `hasActed = true` IS set — every non-move action, on all paths:
- `block_handler.cpp` (8 sites incl. 189/210/217/230/508; deliberate temporary
  resets at 517/568 for Frenzy/second block only), `pass_handler.cpp:119` (pass)
  and `:362` (hand-off), `foul_handler.cpp:101`, `gaze_handler.cpp:12`,
  `bomb_handler.cpp:13`, `ttm_handler.cpp:15`, `ball_and_chain_handler.cpp:99`.

Where it is NOT set — movement:
- `move_handler.cpp resolveMoveStep` (78–207): sets `hasActed = true` only on
  the three failure paths (failed dodge :147, failed GFI :171, failed pickup :201
  — all turnovers). The success path (:206 `return ActionResult::ok()`) sets only
  `hasMoved = true` and decrements `movementRemaining`. Same shape in
  `resolveLeap` (209–299).
- `action_resolver.cpp case ActionType::MOVE` (37–52): returns `resolveMoveStep`
  result directly; nothing set on success. (BLITZ is safe: its trailing
  `resolveBlock` sets `hasActed` on every path — confirmed by the comment at
  `macro_actions.cpp:1002`.)
- Nothing anywhere fires when a player's movement is "done" — movement is
  decomposed into single-step primitives, so no step can know it is the last,
  and no close-out exists at the layer that switches to another player.

What this permits at the primitive legal-action level (`rules_engine.cpp`):
- `:18-19` gates all generation on `p.canAct()` only. A player with `hasMoved=true`,
  `movementRemaining=0`, `hasActed=false` is still offered BLOCK (:40-48, no team
  cap at all), PASS/HAND_OFF (:71-94, capped only by `passUsedThisTurn`), FOUL
  (:96-107, capped by `foulUsedThisTurn`), plus further MOVE if MA remains
  (split movement). Note move-then-BLOCK is effectively a free Blitz that
  bypasses the once-per-turn blitz limit, for every player, every turn.

Macro layer (`macro_actions.cpp`) — the internal inconsistency that shows intent:
- `isFreeToAct` (:98-100) = `p.canAct() && !p.hasMoved` — the codebase's own
  "player still has their activation" predicate — is used for CAGE fill (:285),
  blitzer selection (:314), nearest-free-player (:123), and defensive macros
  (:440, :545).
- But BLOCK macro selection (:417) and FOUL fouler selection (:495) check only
  `canAct()/!hasActed` (redundantly doubled), and SCORE / HAND_OFF_SCORE /
  PASS_SCORE / CHAIN_SCORE / ADVANCE (:152-279) gate the carrier on `canAct()`
  only. So a player moved by an earlier macro is re-selected later as
  blocker/fouler/passer — a fresh, unlinked second activation.

## Intentional design? No.

- Real BB rules: one activation per player per team-turn. Pass/Foul actions may
  include movement, but only as one continuous declared action — not "move now,
  let four teammates act, pass later". Block allows no movement at all.
- Engine intent: `hasActed` + `resetPlayersForNewTurn` + every non-move handler
  setting it on all paths + `isFreeToAct(!hasMoved)` in half the macro selectors
  all point to one-activation-per-player being the intended model. No comment,
  doc, or test anywhere endorses reactivation after movement.
- Test coverage: `test_rules_engine.cpp:148 ActedPlayerCannotAct` verifies the
  gate works when `hasActed` is set manually, but NO test asserts a completed
  successful MOVE sets it, and no test covers move-then-later-action. The exact
  scenario is untested — a genuine gap, not a tested design choice.

## Prevalence (150-game dataset, diag_perplayer_grounding_data/main/)

Scanner: group each turn's actor-attributable events (MOVE/BLOCK/PASS/FOUL/
PICKUP/DODGE/GFI, active-team ids only) into contiguous per-player runs; flag a
player whose run reappears after >=1 other player's run (interleaving excludes
"single activation logged as steps" — BLITZ and macro executions emit each
player's events contiguously, so interleaved = genuinely separate selections).
Any reappearance implies the earlier run succeeded (failures set hasActed and
end the turn via turnover).

- Team-turns scanned: 4803 (150/150 games)
- Interleaved SECOND-ACTION reactivation (later run contains BLOCK/PASS/FOUL):
  **641 turns = 13.3%**, in **145/150 games**
  - offending action events: FOUL 398, BLOCK 267, PASS 83
  - moved-earlier -> BLOCK-later ("free blitz" bypassing blitz cap): 248 turns
- MOVE-only interleaved reactivation (split movement, milder): 162 turns = 3.4%
- Supplementary: turns with >=2 contiguous move+block runs (exceeds the 1-blitz
  allowance even without interleaving; conservative, may include Frenzy):
  24 = 0.5%
- The originally-inspected case (g0000 h1t1, id11 full-MA move+pickup, 4 other
  players act, then id11 PASS -> turnover) is the scanner's example #1.

Systemic, not an edge case: ~1 in 7.5 team-turns of self-play/training data
contains an illegal second activation. FOUL and BLOCK dominate, so it inflates
attrition/pressure for whichever side exploits it more, and MCTS is planning in
an action space with illegal continuations.

## Suggested fix (NOT APPLIED — for later review only)

Preferred: activation close-out at the actor-switch boundary. Add
`int currentActivationId = -1` to GameState (or track in the turn loop); in
`executeAction` (`action_resolver.cpp:179`) / macro execution entry, when the
incoming action's `playerId` differs from `currentActivationId`, set
`hasActed = true` on the previous player if they had `hasMoved` (then update
`currentActivationId`; reset on END_TURN/turnover/kickoff). This preserves
legal continuous move->pass/foul/score sequences and multi-step movement, while
killing interleaved reactivation at BOTH the primitive (rules_engine/MCTS) and
macro layers with one mechanism.
Cheaper but partial alternative: use `isFreeToAct` (i.e. add `!hasMoved`)
consistently in macro BLOCK (:417) and FOUL (:495) selection — but this leaves
the primitive layer (mcts.cpp / game_simulator fallback paths) unfixed and
still allows split movement.
Interaction risks for the fix pass: carrier-revisit macro plans (ADVANCE now,
PASS_SCORE/SCORE later) become unavailable by design — expect behavior change;
like every engine fix this week it resets the draw-rate baseline, so bundle
with the next paired-seed A/B (noise floor rules apply: single N=150 delta
<10pp = INCONCLUSIVE).
