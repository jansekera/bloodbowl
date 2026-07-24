#!/usr/bin/env python3
"""Item 3.5 Path A (2026-07-24): does the vs-random internal candidate
selector (run_iteration.py _run_benchmark, gates az_train vs train_best
purely on win-rate vs a RANDOM opponent) actually pick the same winner a
direct head-to-head between the two candidates would pick?

Finding motivating this (project_bloodbowl_candidate_selection_saturated_20260723):
across all 4 iterations of the just-finished --loop 4 run, the vs-random
score saturated in a narrow 93-97% band regardless of which epoch won,
barely discriminating between candidates that could differ meaningfully
in real strength.

Cheap validation path: iteration 4's own leftover weights files were
still on disk (weights_az_train.json, weights_train_best.json,
weights_frozen.json, weights_policy.json, all consistent with the SAME
iteration by timestamp/log cross-check) -- no new training run needed.
That iteration's vs-random gate picked az_train (94.5%) over train_best
(93.0%), a 1.5pp gap well inside the saturated band.

This reuses the PRODUCTION gate mechanism (_gate_game / _imap_watchdog)
directly, same pattern as diag_mirror_budget.py -- candidate vs candidate,
side-swapped, macro_mcts vs macro_mcts, GATE_VF_BLEND (production gate's
vf_blend, not the 0.0 used for training/benchmark).

PASS (vs-random selector validated): direct az_train-vs-train_best H2H
  picks the same winner (az_train) the vs-random filter already picked.
FAIL (vs-random selector is picking arbitrarily or wrong): H2H picks
  train_best, or is a statistical toss-up (no significant winner) despite
  vs-random's confident-looking 94.5/93.0 split.
"""
import random
import sys
from multiprocessing import Pool

sys.path.insert(0, "python"); sys.path.insert(0, "engine/build")
from run_iteration import (_gate_game, _imap_watchdog, _pool_init, TV,
                            GATE_VF_BLEND, GATE_DIRICHLET_ALPHA, GATE_EXPLORATION_C)

N = int(sys.argv[1]) if len(sys.argv) > 1 else 150
MCTS_ITERATIONS = 100  # production value, matches the iteration this pair came from
AZ_TRAIN = "weights_az_train.json"
TRAIN_BEST = "weights_train_best.json"
POLICY = "weights_policy.json"
WORKERS = 12
init_args = ("engine/build", "python")

print(f"Path A: az_train vs train_best direct H2H, n={N}, TV={TV}, "
      f"GATE_VF_BLEND={GATE_VF_BLEND}, MCTS={MCTS_ITERATIONS}", flush=True)
print(f"(vs-random filter picked az_train 94.5% > train_best 93.0% for this pair "
      f"-- iteration 4 of training_gatefix_20260722.log)", flush=True)

# _gate_game(seed, race_idx, gate_path, frozen_path, mcts_iterations, vf_blend, tv,
#            leaf_lookahead, policy_path, cand_is_away)
# Reuse "gate_path" slot for az_train (candidate) and "frozen_path" slot for
# train_best (opponent) -- same mechanic as production gating, just candidate
# vs candidate instead of candidate vs frozen champion.
tasks = [
    (random.randint(1, 999999), i, AZ_TRAIN, TRAIN_BEST, MCTS_ITERATIONS,
     GATE_VF_BLEND, TV, False, POLICY, i % 2 == 1)
    for i in range(N)
]

az_wins = draws = tb_wins = 0
done = 0
with Pool(WORKERS, initializer=_pool_init, initargs=init_args) as pool:
    for cs, fs, _side in _imap_watchdog(pool, _gate_game, tasks, 'pathA',
                                        mcts_iterations=MCTS_ITERATIONS):
        done += 1
        if cs > fs:
            az_wins += 1
        elif cs == fs:
            draws += 1
        else:
            tb_wins += 1
        if done % 10 == 0 or done == N:
            total = az_wins + draws + tb_wins
            print(f"  {done}/{N}: az_train {az_wins}W train_best {tb_wins}W {draws}D "
                  f"({100*az_wins/total:.1f}% / {100*tb_wins/total:.1f}% / {100*draws/total:.1f}%)",
                  flush=True)

total = az_wins + draws + tb_wins
print(f"\n=== Path A result: az_train vs train_best, n={total} (of {N} requested) ===", flush=True)
print(f"az_train {az_wins}W  train_best {tb_wins}W  {draws}D", flush=True)
print(f"az_train win-rate (draws as half): "
      f"{100*(az_wins + 0.5*draws)/total:.1f}%", flush=True)
print(f"For comparison, vs-random filter said az_train 94.5% > train_best 93.0% "
      f"(1.5pp gap, inside its ~93-97% saturated band).", flush=True)
