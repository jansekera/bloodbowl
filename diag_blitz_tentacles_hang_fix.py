#!/usr/bin/env python3
"""Smoke/decisive test (2026-07-07) for the BLITZ/Tentacles no-progress hang
fix (action_resolver.cpp, commit c6d7b5b). Root-caused via a pre-existing
static analysis (hang_analysis.md, 2026-06-25): the BLITZ move-toward-target
`while` loop had no progress guard, so a step that reports "success" without
moving the player (an adjacent Tentacles model winning the escape contest)
retried the identical step forever until the escape dice eventually
succeeded -- a heavy-tailed stall that the 180s gating watchdog catches as a
skipped/hung game (~3-5/150 in recent runs), not a hard deadlock.

Fix: bail with ActionResult::fail() if position didn't change after a step,
mirroring this function's existing bail-out style. Pure hang-prevention +
minor rules-correctness fix (a Tentacles catch should end movement, not
grant free retries) -- not expected to shift the draw-rate meaningfully,
but measure anyway per project convention. The metric that matters most
here is the WATCHDOG-SKIP COUNT, which should drop (not necessarily to
zero -- the secondary TTM-scatter suspect in ttm_handler.cpp is untouched).

Single-arm smoke, self-mirror, MCTS=100, policy_path=weights_policy.json
(production default). Compares against the current reference point:
diag_fpu_children0_fix_150's N=150 result, 45.2% draws (n=146) -- this
session's fresh baseline (post-FPU-fix, the most recent landed change).
"""
import random
import sys
from pathlib import Path
from multiprocessing import Pool

sys.path.insert(0, "python"); sys.path.insert(0, "engine/build")
from run_iteration import _gate_game, _imap_watchdog, _pool_init

N = int(sys.argv[1]) if len(sys.argv) > 1 else 30
W = "weights_best.json"
POLICY_PATH = "weights_policy.json"
TV = 1000
VF_BLEND = 0.0
MCTS_ITERS = 100
WORKERS = 12
init_args = ("engine/build", "python")

if not Path(POLICY_PATH).exists():
    print(f"ERROR: {POLICY_PATH} not found in cwd -- run from repo root.", file=sys.stderr)
    sys.exit(1)

print(f"\n--- starting MIRROR gating MCTS={MCTS_ITERS} policy_path={POLICY_PATH!r} "
      f"(+ BLITZ/Tentacles hang fix) n={N} ---", flush=True)
tasks = [
    (random.randint(1, 999999), i, W, W, MCTS_ITERS, VF_BLEND, TV, False, POLICY_PATH)
    for i in range(N)
]
wins = draws = losses = 0
done = 0
with Pool(WORKERS, initializer=_pool_init, initargs=init_args) as pool:
    for hs, as_ in _imap_watchdog(pool, _gate_game, tasks, 'blitz-tentacles-hang-fix',
                                  mcts_iterations=MCTS_ITERS):
        done += 1
        if hs > as_:
            wins += 1
        elif hs == as_:
            draws += 1
        else:
            losses += 1
        if done % 10 == 0 or done == N:
            total = wins + draws + losses
            print(f"  {done}/{N} done -- so far {wins}W {draws}D {losses}L "
                  f"= {100*draws/total:.1f}% draws", flush=True)

total = wins + draws + losses
print(f"\n=== MIRROR gate champion vs champion  blitz-tentacles-hang-fix  n={total} (of {N} requested) ===", flush=True)
print(f"{wins}W {draws}D {losses}L = {100*draws/total:.1f}% draws, "
      f"{100*wins/total:.1f}% home win, {100*losses/total:.1f}% home loss", flush=True)
print(f"  Compare to pre-fix reference (diag_fpu_children0_fix_150, N=150): "
      f"45.2% draws, 30.1% home win, 24.7% home loss (n=146).", flush=True)
print(f"  KEY METRIC for this fix specifically: watchdog-skip count above "
      f"should be lower than the ~3-5/150 rate seen in recent runs "
      f"(FPU-fix run skipped 4/150, halfclock-fix run skipped 5/150).", flush=True)
