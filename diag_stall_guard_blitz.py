#!/usr/bin/env python3
"""Paired-seed A/B for the stall-guard blitz-awareness fix (a88f5e2, 2026-07-13).

The fix lets the ball carrier spend its full movement budget instead of the
throttled half whenever an opponent is already in blitz range: the movement the
throttle holds back only buys a cage if the carrier is still standing there next
turn, and it isn't when it can be blitzed.

This is a C++ change, so the two arms cannot run in one process -- run the
baseline against the PRE-fix binary, rebuild, then run the candidate:

    git stash            # or check out a88f5e2^ -- engine/src/macro_actions.cpp
    cmake --build engine/build -j
    python3 diag_stall_guard_blitz.py baseline [N]
    git stash pop        # restore the fix
    cmake --build engine/build -j
    python3 diag_stall_guard_blitz.py candidate [N]
    python3 diag_stall_guard_blitz.py compare

PRIMARY metric is touchdowns per game, not draw rate. The fix targets the
offensive weakness (carrier arrives late / gets blitzed off the ball), and per
feedback_draw_rate_noise_floor a draw-rate delta under ~10pp from a single run
is noise either way. Draw rate is reported as a companion, not as the verdict.
"""
import json
import sys
from pathlib import Path

sys.path.insert(0, "python")
sys.path.insert(0, "engine/build")

import diag_utils as du
from run_iteration import (GATE_DIRICHLET_ALPHA, GATE_EXPLORATION_C, _gate_game)

W = "weights_best.json"
POLICY_PATH = "weights_policy.json"
TV, VF_BLEND, MCTS = 1000, 0.0, 100
BASE_SEED = 20260713
ARMS = {"baseline": "arm_stall_guard_baseline.json",
        "candidate": "arm_stall_guard_candidate.json"}


def run(arm: str, n: int) -> None:
    seeds = du.paired_seeds(n, base_seed=BASE_SEED)
    tasks = [(s, i, W, W, MCTS, VF_BLEND, TV, False, POLICY_PATH)
             for i, s in enumerate(seeds)]
    print(f"=== stall-guard A/B: arm={arm}  N={n} ===", flush=True)
    print(f"gate config: dirichlet_alpha={GATE_DIRICHLET_ALPHA} "
          f"exploration_c={GATE_EXPLORATION_C}  mcts={MCTS}  vf_blend={VF_BLEND}",
          flush=True)
    results = du.run_arm(arm, tasks, game_fn=_gate_game, mcts_iterations=MCTS)
    du.save_arm(ARMS[arm], arm, seeds, results)
    tds = sum(hs + as_ for hs, as_ in results.values())
    draws = sum(1 for hs, as_ in results.values() if hs == as_)
    k = len(results)
    print(f"  {arm}: {k}/{n} completed ({n - k} watchdog skips), "
          f"{tds / k:.3f} TD/game, {100 * draws / k:.1f}% draws", flush=True)


def paired_td_delta(res_a, res_b) -> str:
    """Paired mean difference in total TDs per game.

    McNemar handles binary outcomes only, and 'did anyone score' throws away the
    thing this fix is supposed to move (how OFTEN they score). The paired
    difference keeps the seed-level variance cancellation that is the whole
    point of running both arms on one seed list.
    """
    common = sorted(set(res_a) & set(res_b))
    diffs = [(res_a[i][0] + res_a[i][1]) - (res_b[i][0] + res_b[i][1])
             for i in common]
    n = len(diffs)
    mean = sum(diffs) / n
    var = sum((d - mean) ** 2 for d in diffs) / (n - 1) if n > 1 else 0.0
    se = (var / n) ** 0.5
    lo, hi = mean - 1.96 * se, mean + 1.96 * se
    verdict = "CONFIRMED" if (lo > 0 or hi < 0) else "INCONCLUSIVE"
    a_td = sum(res_a[i][0] + res_a[i][1] for i in common) / n
    b_td = sum(res_b[i][0] + res_b[i][1] for i in common) / n
    return "\n".join([
        f"=== PAIRED A/B (touchdowns per game)  candidate vs baseline  "
        f"n={n} pairs ===",
        f"  TD/game: candidate {a_td:.3f}  vs  baseline {b_td:.3f}",
        f"  paired delta = {mean:+.3f} TD/game   SE = {se:.3f}   "
        f"95% CI [{lo:+.3f}, {hi:+.3f}]",
        f"  VERDICT: {verdict}"
        + ("" if verdict == "CONFIRMED"
           else " -- CI includes 0; NOT evidence of no effect, see CI width"),
    ])


def compare() -> None:
    missing = [a for a, p in ARMS.items() if not Path(p).exists()]
    if missing:
        sys.exit(f"missing arm(s): {missing} -- run them first")
    _, seeds_b, res_b = du.load_arm(ARMS["baseline"])
    _, seeds_a, res_a = du.load_arm(ARMS["candidate"])
    if seeds_a != seeds_b:
        sys.exit("ABORT: arms ran on different seed lists -- not paired")
    print(paired_td_delta(res_a, res_b), flush=True)
    print(flush=True)
    print(du.mcnemar_report(res_a, res_b, outcome="draw"), flush=True)
    print(flush=True)
    print(du.mcnemar_report(res_a, res_b, outcome="home_win"), flush=True)


if __name__ == "__main__":
    mode = sys.argv[1] if len(sys.argv) > 1 else ""
    if mode == "compare":
        compare()
    elif mode in ARMS:
        run(mode, int(sys.argv[2]) if len(sys.argv) > 2 else 400)
    else:
        sys.exit(__doc__)
