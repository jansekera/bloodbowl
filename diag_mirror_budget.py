#!/usr/bin/env python3
"""Cheapest falsification test proposed independently by Opus 4.8 and Sonnet 5
(fresh-eyes cross-model review, 2026-07-01): does raising MCTS search budget
reduce the DRAW rate in a true MIRROR match (champion vs itself), the way it
already reduced draws in the asymmetric diag_vs_scorer test (vs greedy)?

diag_vs_scorer only tested champion vs greedy/random (asymmetric). Gating's
~61-67% draw rate is a MIRROR match (candidate vs frozen, similar strength).
This reuses the PRODUCTION gate game function (_gate_game) and its watchdog
(_imap_watchdog, tolerates hung games) from run_iteration.py directly, so
this IS a real gating-scale run, not a re-implementation, with true
per-game incremental logging (prints as each game completes -- survives
disconnection with something to pick up from) at MCTS=100 vs MCTS=400,
candidate=frozen=current champion (weights_best.json), vf_blend=0 (the
production regime -- value head is disconnected either way).

n=48 pilot (2026-07-01): MCTS100 draws 56.2%, MCTS400 draws 50.0% -- partial
effect, not a collapse. This run (N=150 default) tightens the estimate to
production gating scale.

PASS (search-budget hypothesis holds): draw rate meaningfully lower at 400
  than 100 (e.g. drops out of the ~60-75% band).
FAIL (structural explanation dominates): draw rate stays pinned near the
  100-iter baseline despite 4x budget.
"""
import random
import sys
from multiprocessing import Pool

sys.path.insert(0, "python"); sys.path.insert(0, "engine/build")
from run_iteration import _gate_game, _imap_watchdog, _pool_init

N = int(sys.argv[1]) if len(sys.argv) > 1 else 150
W = "weights_best.json"
TV = 1000
VF_BLEND = 0.0
WORKERS = 12
init_args = ("engine/build", "python")

for iters in (100, 400):
    print(f"\n--- starting MIRROR gating MCTS={iters} n={N} ---", flush=True)
    tasks = [
        (random.randint(1, 999999), i, W, W, iters, VF_BLEND, TV)
        for i in range(N)
    ]
    wins = draws = losses = 0
    done = 0
    with Pool(WORKERS, initializer=_pool_init, initargs=init_args) as pool:
        for hs, as_ in _imap_watchdog(pool, _gate_game, tasks, f'mirror-{iters}',
                                      mcts_iterations=iters):
            done += 1
            if hs > as_:
                wins += 1
            elif hs == as_:
                draws += 1
            else:
                losses += 1
            if done % 10 == 0 or done == N:
                total = wins + draws + losses
                print(f"  [{iters} iter] {done}/{N} done -- so far {wins}W {draws}D {losses}L "
                      f"= {100*draws/total:.1f}% draws", flush=True)

    total = wins + draws + losses
    print(f"\n=== MIRROR gate champion vs champion  MCTS={iters}  n={total} (of {N} requested) ===", flush=True)
    print(f"{wins}W {draws}D {losses}L = {100*draws/total:.1f}% draws, "
          f"{100*wins/total:.1f}% home win, {100*losses/total:.1f}% home loss", flush=True)
    print(f"  Compare to production gating baseline ~61-67% draws (MCTS=100); "
          f"n=48 pilot MCTS100=56.2%/MCTS400=50.0%.", flush=True)
