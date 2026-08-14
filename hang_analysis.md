# Engine infinite-loop — static analysis (2026-06-25 CEST)

Read-only static analysis of the rare (~1/600) single-game hang seen on the
anti-regression gate in `macro_mcts vs macro_mcts` self-play (MCTS=100, TV=1200,
developed rosters, races {human, orc, skaven, dwarf, wood-elf}). No build, no
training, no commit. Reasoned from the code; every candidate cites `file:line`.

## Key framing fact
The two top-level game loops are hard-capped:
- `engine/src/game_simulator.cpp:388` and `:512` — `while (phase != GAME_OVER && totalActions < MAX_ACTIONS)`, `MAX_ACTIONS = 5000` (`:369`, `:480`).

So the hang **cannot** be the outer game loop. It must be an inner loop that spins
*within a single action/macro execution*, so `totalActions` never advances. That
narrows the search to per-action / per-macro loops with no fixed iteration cap.

---

## Ranked candidate loops

### #1 (PRIME SUSPECT) — BLITZ move-toward loop, no iteration cap, stalls on Tentacles
`engine/src/action_resolver.cpp:78` — `while (player.position.distanceTo(target.position) > 1)`

This loop has **no fixed iteration counter**. Each pass it picks `bestNext` = an
empty adjacent square strictly closer to the target (`:87-98`) and calls
`resolveMoveStep` (`:102`). Termination relies entirely on every iteration either
(a) moving the blitzer strictly closer, or (b) exiting via fail/turnover/knockdown.

The hole is **Tentacles**. In `resolveMoveStep`:
- `engine/src/move_handler.cpp:97-101` — if the step leaves a tackle zone, it calls
  `checkTentacles`; on a *caught* result it `return ActionResult::ok()` **before**
  the movement decrement.
- `engine/src/move_handler.cpp:33-37` (`checkTentacles`) — on a failed escape contest
  the mover **stays at `from`**, stays `STANDING`, and the function returns true.
- The MA decrement (`player.movementRemaining--`, `move_handler.cpp:104`) is *after*
  the Tentacles early return, so **MA is not decremented** either.

Back in the BLITZ loop the returned result is `success && !turnover`, the player is
still `STANDING`, and `player.position` is unchanged ⇒ `distanceTo(target)` is
unchanged ⇒ **the loop re-runs the identical step**. There is no per-iteration
budget to exhaust (MA isn't spent, no step counter), so the only exit is finally
winning a Tentacles contest.

Why it matches the symptom profile:
- **Rare / state-dependent**: requires a blitz where the only path to the target
  forces stepping out of the tackle zone of an adjacent **Tentacles** model. That
  skill lives on big guys — Skaven *Rat Ogre*, Orc *Troll*, plausibly on the dwarf/
  human/wood-elf rosters' star/big-guy slots — exactly the developed-TV rosters in
  the gate. One such pinned blitz per ~hundreds of games is consistent with ~1/600.
- **Not a permanent deadlock by dice alone**: the escape contest is
  `(d6+moverST) > (d6+tentST)` (`move_handler.cpp:28`); no race has an ST gap ≥6, so
  escape always has nonzero probability and a normal `mt19937` roller
  (`engine/src/dice.cpp:12`) eventually passes. ⇒ this is a **heavy-tailed slow loop**
  (could run far past 300s before escaping), which is precisely what a watchdog, not
  a true `for(;;)`, catches. With a low moverST vs a high tentST the per-iteration
  escape probability is small, so the tail can be enormous.
- This loop runs both in the real game and **during MCTS expansion** via
  `expandBlitz` (`macro_actions.cpp:899` executes the BLITZ action through
  `executeAction` → this `case ActionType::BLITZ`), multiplying exposure by the
  per-move search budget (MCTS=100), again consistent with a rare per-*game* hit.

### #2 — TTM scatter "until empty or off-pitch", no cap
`engine/src/ttm_handler.cpp:106` — `while (state.getPlayerAtPosition(landPos) != nullptr) { ... scatter ... }`

Pure board-state-bounded loop with no iteration cap. Each pass scatters by a
non-zero d8 offset (`helpers.cpp:197-204`, every direction is non-zero), so on a
finite pitch it is a random walk that leaves the pitch with probability 1 and exits
(`:111-116`). Terminates w.p.1 but **has no hard cap**, so a dense board where the
landing keeps hitting occupied squares can spin a long tail. Lower than #1 because
TTM (Throw Team-Mate) requires a Right Stuff thrower; not all listed races field one,
and the displacement-per-step random walk escapes a 26×15 pitch fast in expectation.

### #3 — BLITZ-chase loop (`while distance>1`) sibling in pathing helpers
`engine/src/action_resolver.cpp:78` is the instance; note the same *no-cap,
distance-gated* shape is the risk pattern. `movePlayerToward`
(`macro_actions.cpp:693`, `for step<maxSteps`) and the blitz-chain
(`macro_actions.cpp:903`, `for step<12`) use the **same Tentacles-stalling step**
but are **for-loops with fixed caps** (4/12/14 or `movementRemaining`), so a
Tentacles catch there merely *wastes* capped iterations — bounded, not a hang.
Listed to show the fix scope: the fault is the *uncapped* BLITZ `while`, not the
underlying no-progress step.

### #4 (RULED OUT, documented) — `expandScore` TZ-probe walk
`engine/src/macro_actions.cpp:737` — `while (cx != targetX || cy != testY)`.
`targetX` is the endzone column 25/0 (`:11-13`) and `cx` steps by `dx = ±1` toward
it (`:740`, `forwardDx :20-22`); `cy` converges to `testY` by ±1 (`:738-739`).
Both coordinates step exactly 1 toward fixed targets ⇒ always terminates. Safe.

### #5 (RULED OUT) — MCTS tree-walk and game-sim loops
`engine/src/macro_mcts.cpp:110` (capped by `maxIterations`), `:171` / `:532` /
`:543` (tree walks; a tree has no cycles), and the Frenzy second-block recursion
`engine/src/block_handler.cpp:511-519` (guarded by `!frenzySecondBlock`, single
re-entry). The pathfinder BFS `engine/src/pathfinder.cpp:65` is bounded by the
`visited[]` array. All terminate.

### Historical corroboration
`engine/src/pass_handler.cpp:24-30` documents a *previously fixed* `while(true)`
hang: a Bresenham walk that overshot its endpoint so the break never fired and "the
game hung forever." Same failure family (loop termination tied to a geometric/board
condition that a degenerate state never satisfies), which raises prior probability
that another such loop (#1/#2) is the current one.

---

## Planning answers

### 1. Effort to root-cause GIVEN a deterministic repro seed
**Fastest path: live stack sample of the wedged worker.** When `fuzz_gate.py`
yields a seed, run that single game, find the spinning PID, and grab a few stacks:
- `py-spy dump --pid <PID>` won't help (the spin is in native C++), so use
  **gdb**: `gdb -p <PID> -batch -ex 'thread apply all bt'` two or three times a few
  seconds apart. If the top frames sit in `resolveMoveStep` / `checkTentacles`
  called from the BLITZ `while` at `action_resolver.cpp:78`, candidate #1 is
  confirmed in minutes. If they sit in `ttm_handler.cpp` scatter, it's #2.
- Cross-check by sampling `player.position` / `movementRemaining` and the opponent's
  skills in the captured frame (`p player`, `p *opp`) — a Tentacles model adjacent
  with the blitzer's MA not decreasing nails it.
- Confirmatory instrumentation if stacks are ambiguous: add a debug-only iteration
  counter + `assert(iters < 1000)` to the BLITZ `while` (and the TTM `while`), run
  the seed, see which assert trips. ~30 min including rebuild.

**Estimate: 1–2 hours, difficulty LOW** once a deterministic seed exists. The hang
is a single-threaded native spin with a tiny suspect set; gdb-attach makes it almost
immediate. The only "med" risk is if the repro is still probabilistic per-seed
(open-loop fresh dice each replay, `macro_mcts.cpp:114-115`) — then the seed may not
deterministically reproduce and you fall back to the iteration-assert + many-runs
bisection, pushing it toward ~3 hours / med.

### 2. Effort to FIX once localized
**One-line / few-line guard. LOW.** Two clean options:
- Add a hard iteration cap to the BLITZ `while` (`action_resolver.cpp:78`): bound it
  by the blitzer's reachable budget, e.g. `for (int guard = 0; guard < 16 && dist>1;
  ++guard)` and `return fail()` on exhaustion. Mirrors the already-capped sibling
  loops (`macro_actions.cpp:693/903`).
- Better, fixes the root no-progress: when `resolveMoveStep` returns "caught by
  Tentacles, no move," it must terminate the *movement* (it already ends the dodge),
  so the caller should treat a no-displacement `ok()` as "movement over." Either give
  `checkTentacles`/`resolveMoveStep` a distinct return (e.g. a `movementEnded` flag
  or a sentinel result) and break the loop on it, or detect `position unchanged after
  a successful step` in the loop and break. ~5–15 lines, no structural change.
- Add the same hard cap to the TTM scatter `while` (`ttm_handler.cpp:106`), e.g. cap
  at a few hundred scatters then force off-pitch / touchback. Trivial.

No data-structure or search-architecture change is required.

### 3. Correctness risk — liveness-only or quality-affecting?
**Potentially a QUALITY bug, not purely liveness — but the quality error is small
and pre-existing.** In *near-miss states that terminate*, the same Tentacles "caught,
no move, MA not decremented" path (`move_handler.cpp:99-104`) means:
- A blitz/move step that is *caught by Tentacles* does **not consume MA** and leaves
  the player in place yet still able to keep trying. Real Blood Bowl: a failed
  Tentacles escape **ends that player's movement** for the activation. Here the
  caller loops and retries from the same square, so the engine effectively grants
  *free retries* until it escapes (it just spends loop iterations, not MA). That can
  let a blitzer reach a target it should not have reached, and skews block/cage
  evaluations the search learns from. So non-hanging games can contain subtly wrong
  moves whenever a Tentacles model is adjacent on a blitz path.
- The capped sibling loops (`movePlayerToward`) hide this as "wasted steps" but the
  uncapped BLITZ loop turns it into both a hang *and* a free-retry rules error.

**Verdict on triage:** the *liveness* symptom is rare and already mitigated by the
per-game timeout, but because the underlying Tentacles no-progress / no-MA-decrement
path is a **rules-correctness defect** that fires (silently) in normal games too, it
should not just ride as a low-priority robustness item — fixing it removes a wrong
turnover/positioning behavior the self-play learns from. Recommend fixing the
root (movement-ends-on-Tentacles-catch) rather than only band-aiding with an
iteration cap, though the cap is a safe immediate guard to land first.

## Top-line summary
- **#1 BLITZ `while` at `action_resolver.cpp:78`** stalling on the **Tentacles
  no-move / no-MA-decrement** step (`move_handler.cpp:97-104`, `:33-37`) is the prime
  suspect — uncapped, heavy-tailed (watchdog-caught, not hard deadlock), fires on
  developed rosters with big-guy Tentacles models, runs inside MCTS `expandBlitz`.
- **#2 TTM scatter `while` at `ttm_handler.cpp:106`** is the secondary uncapped loop.
- Root-cause given a seed: **1–2 h, LOW** (gdb-attach + 2–3 backtraces).
- Fix: **one-line cap LOW**, or a few-line root fix (movement ends on Tentacles catch).
- Correctness: **quality-affecting**, not liveness-only — Tentacles catches grant
  free retries and skip MA cost in *all* games, so prefer the root fix.

## REPRO (fuzz 2026-06-25 17:00 CEST)
Deterministický hang: **seed=353705 race_idx=10 (human vs orc)**, MCTS=100 TV=1200, weights_az_train vs weights_frozen. Přeteklo >240s. Pro root-cause: gdb-attach běžící hru s tímto seedem nebo iteration-count assert.
