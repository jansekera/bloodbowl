#!/usr/bin/env python3
"""Smoke test (2026-07-02) for the bounded greedy leaf-lookahead experiment
(macro_mcts.cpp: greedyLookaheadBonus / MCTSConfig.leafLookahead).

Mirror match (champion vs itself, weights_best.json, vf_blend=0, MCTS=100),
A/B: leaf_lookahead=False (current production default) vs True (new term).
Reuses the PRODUCTION _gate_game / _imap_watchdog helpers from
run_iteration.py -- same code path as diag_mirror_budget.py -- just adds the
leaf_lookahead flag as an 8th tuple element (backward-compatible; production
gating and diag_mirror_budget.py still pass 7-tuples and get False).

SMOKE ONLY: small N, not a decision-grade measurement. Does not touch
weights_best.json or production gating.
"""
import random
import sys
from multiprocessing import Pool

sys.path.insert(0, "python"); sys.path.insert(0, "engine/build")
from run_iteration import _gate_game, _imap_watchdog, _pool_init

N = int(sys.argv[1]) if len(sys.argv) > 1 else 30
W = "weights_best.json"
TV = 1000
VF_BLEND = 0.0
MCTS_ITERS = 100
WORKERS = 12
init_args = ("engine/build", "python")

for label, leaf_lookahead in (("baseline (off)", False), ("leaf-lookahead (on)", True)):
    print(f"\n--- starting MIRROR gating MCTS={MCTS_ITERS} leaf_lookahead={leaf_lookahead} n={N} ---", flush=True)
    tasks = [
        (random.randint(1, 999999), i, W, W, MCTS_ITERS, VF_BLEND, TV, leaf_lookahead)
        for i in range(N)
    ]
    wins = draws = losses = 0
    done = 0
    # leaf_lookahead adds a bounded but real per-leaf-eval cost (extra clone +
    # one extra greedyExpandMacro call when carrying the ball); give the "on"
    # arm a generous 2x watchdog timeout headroom so the smoke test measures
    # draw rate, not spurious watchdog skips from underestimated wall time.
    watchdog_mcts_arg = MCTS_ITERS * 2 if leaf_lookahead else MCTS_ITERS
    with Pool(WORKERS, initializer=_pool_init, initargs=init_args) as pool:
        for hs, as_ in _imap_watchdog(pool, _gate_game, tasks, f'leafla-{leaf_lookahead}',
                                      mcts_iterations=watchdog_mcts_arg):
            done += 1
            if hs > as_:
                wins += 1
            elif hs == as_:
                draws += 1
            else:
                losses += 1
            if done % 10 == 0 or done == N:
                total = wins + draws + losses
                print(f"  [{label}] {done}/{N} done -- so far {wins}W {draws}D {losses}L "
                      f"= {100*draws/total:.1f}% draws", flush=True)

    total = wins + draws + losses
    print(f"\n=== MIRROR gate champion vs champion  {label}  n={total} (of {N} requested) ===", flush=True)
    print(f"{wins}W {draws}D {losses}L = {100*draws/total:.1f}% draws, "
          f"{100*wins/total:.1f}% home win, {100*losses/total:.1f}% home loss", flush=True)
