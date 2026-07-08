#!/usr/bin/env python3
"""Smoke/decisive test (2026-07-08) for the PICKUP expansion step-cap fix
(macro_actions.cpp expandPickup, audit finding 6,
project_bloodbowl_audit_findings_20260703.md). Candidate generation
(getAvailableMacros) admits pickers up to movementRemaining+2 squares away
(up to 11 for MA9), but expandPickup's move-to-ball call was hardcoded to
maxSteps=8 -- a picker legitimately selected at distance 9-11 walked only 8
steps, stopped short, and wasted the whole activation with the ball still
loose.

Fix: maxSteps = movementRemaining + 2, matching the generation-side reach
check exactly. Narrow trigger condition (only fires for high-MA pickers at
the edge of their reach) -- not expected to shift the draw-rate much, but
measure anyway per project convention.

Single-arm smoke, self-mirror, MCTS=100, policy_path=weights_policy.json
(production default). Compares against the current reference point:
BLITZ/Tentacles hang-fix N=150 result, 46.6% draws (n=148) -- the most
recent landed change.
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
      f"(+ PICKUP step-cap fix, finding 6) n={N} ---", flush=True)
tasks = [
    (random.randint(1, 999999), i, W, W, MCTS_ITERS, VF_BLEND, TV, False, POLICY_PATH)
    for i in range(N)
]
wins = draws = losses = 0
done = 0
with Pool(WORKERS, initializer=_pool_init, initargs=init_args) as pool:
    for hs, as_ in _imap_watchdog(pool, _gate_game, tasks, 'pickup-stepcap-fix',
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
print(f"\n=== MIRROR gate champion vs champion  pickup-stepcap-fix  n={total} (of {N} requested) ===", flush=True)
print(f"{wins}W {draws}D {losses}L = {100*draws/total:.1f}% draws, "
      f"{100*wins/total:.1f}% home win, {100*losses/total:.1f}% home loss", flush=True)
print(f"  Compare to pre-fix reference (diag_blitz_tentacles_hang_fix_150, N=150): "
      f"46.6% draws, 30.4% home win, 23.0% home loss (n=148).", flush=True)
print(f"  This fix is narrow/rare-trigger (only high-MA pickers at distance 9-11) -- "
      f"main check is no crash/regression, draw-rate shouldn't move much.", flush=True)
