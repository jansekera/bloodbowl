#!/usr/bin/env python3
"""Item 3.5 Path B (2026-07-24): benchmark az_train and train_best
INDEPENDENTLY against the frozen champion (weights_frozen.json, iteration
4's actual gating opponent), instead of the vs-random filter used in
production. More expensive than Path A (two full match sets, N each,
instead of one direct N-game comparison) but closer to what the real
downstream anti-regression gate measures.

Run AFTER Path A (diag_item35_selection_pathA_20260724.py) and its result
has been reviewed -- per project convention, one change/measurement at a
time, verified before the next. Do not run both concurrently: same 12
worker pool, would just halve each other's throughput for no benefit.

Uses the same preserved iteration-4 weight files as Path A (weights_az_train.json,
weights_train_best.json, weights_frozen.json, weights_policy.json) -- no
new training run needed.

Compare against:
- vs-random filter (production): az_train 94.5% > train_best 93.0%
- Path A direct H2H: see diag_item35_pathA_20260724.log
"""
import random
import sys
from multiprocessing import Pool

sys.path.insert(0, "python"); sys.path.insert(0, "engine/build")
from run_iteration import (_gate_game, _imap_watchdog, _pool_init, TV,
                            GATE_VF_BLEND)

N = int(sys.argv[1]) if len(sys.argv) > 1 else 150
MCTS_ITERATIONS = 100
FROZEN = "weights_frozen.json"
POLICY = "weights_policy.json"
WORKERS = 12
init_args = ("engine/build", "python")


def run_vs_frozen(candidate_path: str, label: str) -> tuple[int, int, int]:
    tasks = [
        (random.randint(1, 999999), i, candidate_path, FROZEN, MCTS_ITERATIONS,
         GATE_VF_BLEND, TV, False, POLICY, i % 2 == 1)
        for i in range(N)
    ]
    wins = draws = losses = 0
    done = 0
    with Pool(WORKERS, initializer=_pool_init, initargs=init_args) as pool:
        for cs, fs, _side in _imap_watchdog(pool, _gate_game, tasks, label,
                                            mcts_iterations=MCTS_ITERATIONS):
            done += 1
            if cs > fs:
                wins += 1
            elif cs == fs:
                draws += 1
            else:
                losses += 1
            if done % 10 == 0 or done == N:
                total = wins + draws + losses
                print(f"  [{label}] {done}/{N}: {wins}W {draws}D {losses}L "
                      f"({100*(wins+0.5*draws)/total:.1f}% WR incl. half-draws)", flush=True)
    return wins, draws, losses


print(f"Path B: az_train and train_best each vs frozen champion, n={N} each, "
      f"TV={TV}, GATE_VF_BLEND={GATE_VF_BLEND}, MCTS={MCTS_ITERATIONS}", flush=True)

az_w, az_d, az_l = run_vs_frozen("weights_az_train.json", "az_train-vs-frozen")
tb_w, tb_d, tb_l = run_vs_frozen("weights_train_best.json", "train_best-vs-frozen")

az_total = az_w + az_d + az_l
tb_total = tb_w + tb_d + tb_l
az_wr = 100 * (az_w + 0.5 * az_d) / az_total
tb_wr = 100 * (tb_w + 0.5 * tb_d) / tb_total

print(f"\n=== Path B result ===", flush=True)
print(f"az_train  vs frozen: {az_w}W {az_d}D {az_l}L -> {az_wr:.1f}% WR (n={az_total})", flush=True)
print(f"train_best vs frozen: {tb_w}W {tb_d}D {tb_l}L -> {tb_wr:.1f}% WR (n={tb_total})", flush=True)
print(f"Path B picks: {'az_train' if az_wr >= tb_wr else 'train_best'} "
      f"(delta {abs(az_wr - tb_wr):.1f}pp)", flush=True)
print(f"For comparison: vs-random filter picked az_train (94.5% > 93.0%, 1.5pp gap).", flush=True)
