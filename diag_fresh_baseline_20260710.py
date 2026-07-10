#!/usr/bin/env python3
"""Fresh mirror baseline after the half-clock and gate-config fixes (2026-07-10).

Two fixes landed today that each invalidate every draw-rate number this project
has ever recorded:

  c020212  kickoff no longer resets the half clock after a touchdown. Games were
           running to 11-0..15-0 and 1700-2200 actions against a weak opponent
           because every TD granted both teams a fresh 8-turn clock. Post-fix a
           game is at most ~385 actions. Game-length dynamics are simply
           different now.
  3a7f208  gating and benchmarking stop measuring with the training search
           config (Dirichlet root noise 0.3, exploration C 0.5) and use the eval
           config they always claimed to use (0.0 / 1.0).

So there is no reference to compare against: the point of this run is to
establish the new one. It deliberately does not print a delta against any
historical number, because none is comparable.

Part 0 is a determinism pre-flight. The whole paired-seed methodology
(diag_utils) rests on a game being reproducible from its seed. It should be --
one DiceRoller and both MCTS policies are seeded from the game seed, rosters are
static -- but that has never actually been tested through the worker Pool. If it
fails, that is a bigger finding than the baseline itself, and the run aborts.

N=400 rather than the historical 150: games are now ~5x shorter, so the tighter
SE (~2.5pp vs ~4.1pp) is nearly free. Even so, treat this as ONE draw of a noisy
quantity, not as truth to three digits.

Usage (from repo root):  python3 diag_fresh_baseline_20260710.py [N]
"""
import sys
from multiprocessing import Pool

sys.path.insert(0, "python")
sys.path.insert(0, "engine/build")

import diag_utils as du
from run_iteration import (GATE_DIRICHLET_ALPHA, GATE_EXPLORATION_C,
                           _gate_game, _imap_watchdog, _pool_init)

N = int(sys.argv[1]) if len(sys.argv) > 1 else 400
W = "weights_best.json"
POLICY_PATH = "weights_policy.json"
TV, VF_BLEND, MCTS, WORKERS = 1000, 0.0, 100, 12
BASE_SEED = 20260710

print(f"=== fresh mirror baseline, N={N} ===", flush=True)
print(f"weights={W}  policy={POLICY_PATH}  MCTS={MCTS}  vf_blend={VF_BLEND}",
      flush=True)
print(f"gate config: dirichlet_alpha={GATE_DIRICHLET_ALPHA} "
      f"exploration_c={GATE_EXPLORATION_C}  (post-3a7f208 eval config)",
      flush=True)

seeds = du.paired_seeds(N, base_seed=BASE_SEED)
tasks = [(s, i, W, W, MCTS, VF_BLEND, TV, False, POLICY_PATH)
         for i, s in enumerate(seeds)]

# --- Part 0: determinism pre-flight -----------------------------------------
print("\n--- Part 0: determinism pre-flight (5 games, run twice) ---", flush=True)
probe = tasks[:5]


def _run(label):
    out = []
    with Pool(WORKERS, initializer=_pool_init, initargs=du.INIT_ARGS) as pool:
        for r in _imap_watchdog(pool, _gate_game, probe, label,
                                mcts_iterations=MCTS):
            out.append(r)
    return out


a, b = _run("preflight-1"), _run("preflight-2")
print(f"  run 1: {a}", flush=True)
print(f"  run 2: {b}", flush=True)
if a != b or len(a) != len(probe):
    print("\n*** ABORT: engine is NOT reproducible from its seed through the "
          "worker Pool. Paired-seed A/B (diag_utils) rests on this. This is a "
          "finding in its own right -- investigate before trusting ANY paired "
          "measurement. ***", flush=True)
    sys.exit(1)
print("  reproducible: YES -- paired-seed methodology is sound here", flush=True)

# --- Part 1: the baseline itself --------------------------------------------
print(f"\n--- Part 1: mirror baseline, n={N} ---", flush=True)
wins = draws = losses = 0
actions_seen = 0
with Pool(WORKERS, initializer=_pool_init, initargs=du.INIT_ARGS) as pool:
    for hs, as_ in _imap_watchdog(pool, _gate_game, tasks, "fresh-baseline",
                                  mcts_iterations=MCTS):
        if hs > as_:
            wins += 1
        elif hs == as_:
            draws += 1
        else:
            losses += 1
        done = wins + draws + losses
        if done % 25 == 0 or done == N:
            print(f"  {done}/{N}: {wins}W {draws}D {losses}L "
                  f"= {100 * draws / done:.1f}% draws", flush=True)

total = wins + draws + losses
skipped = N - total
draw_rate = draws / total if total else 0.0
se = (draw_rate * (1 - draw_rate) / total) ** 0.5 if total else 0.0
decisive = wins + losses
chess = wins / decisive if decisive else 0.5

print("\n" + "=" * 62, flush=True)
print(f"FRESH BASELINE (post c020212 + 3a7f208), n={total}/{N}", flush=True)
print(f"  {wins}W {draws}D {losses}L", flush=True)
print(f"  draw rate   : {100 * draw_rate:.1f}%  (SE {100 * se:.1f}pp, "
      f"95% CI +-{100 * 1.96 * se:.1f}pp)", flush=True)
print(f"  home win    : {100 * wins / total:.1f}%   home loss: "
      f"{100 * losses / total:.1f}%", flush=True)
print(f"  decisive    : {decisive} games, chess score {100 * chess:.1f}%", flush=True)
print(f"  watchdog skips: {skipped}/{N}"
      f"{'  <-- expect 0; the old skips were long games, not hangs' if skipped else '  (clean)'}",
      flush=True)
print("\nThis is the NEW reference. Do not compare it to 42.6-50.7% or any other", flush=True)
print("pre-2026-07-10 number: those were measured on a different game (half", flush=True)
print("clock resetting every TD) with a different search (training noise).", flush=True)
print("=" * 62, flush=True)
