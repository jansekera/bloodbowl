#!/usr/bin/env python3
"""Paired-seed multi-arm A/B for the 2026-07-14 fix chain (one baseline reset).

Four arms, one shared seed list, adjacent comparisons give per-fix attribution:

    pre     251d21b  yesterday's HEAD (post stall-guard baseline)
    h2      a79d164  + H2 kickoff reverses the OPENING roles, not the last drive
    repo2a  bd21d4a  + REPOSITION walks the real movement budget (item 8)
    lane2b  9baeb04  + Strategy 0.5 intercept lane (item 9)

Each arm needs its own engine binary -- run via run_h2_screens_ab.sh, which
drives the checkout/rebuild chain and calls this script per arm, then compare.

Reading key (proposals_screen_fixes_20260713.md):
  - h2 vs pre: structural rule change; watch home_win (first-possession bias
    should shrink -- the late-H1-scorer no longer receives again) + TD/game.
  - repo2a/lane2b: DEFENSIVE fixes; a draw-rate RISE is the expected outcome,
    not a regression. TD/game and conceded-side detail get their own probes
    later (arrival rate, lane coverage, greedy-scorer) -- this chain is the
    shared-baseline tripwire + attribution layer.
Per feedback_draw_rate_noise_floor: single-run draw deltas under ~10pp are
noise; McNemar CIs printed for every pair.
"""
import sys
from itertools import pairwise
from pathlib import Path

sys.path.insert(0, "python")
sys.path.insert(0, "engine/build")

import diag_utils as du
from run_iteration import (GATE_DIRICHLET_ALPHA, GATE_EXPLORATION_C, _gate_game)

W = "weights_best.json"
POLICY_PATH = "weights_policy.json"
TV, VF_BLEND, MCTS = 1000, 0.0, 100
BASE_SEED = 20260714
CHAIN = ["pre", "h2", "repo2a", "lane2b"]
ARMS = {a: f"arm_chain_{a}.json" for a in CHAIN}


def run(arm: str, n: int) -> None:
    seeds = du.paired_seeds(n, base_seed=BASE_SEED)
    tasks = [(s, i, W, W, MCTS, VF_BLEND, TV, False, POLICY_PATH)
             for i, s in enumerate(seeds)]
    print(f"=== fix-chain A/B: arm={arm}  N={n} ===", flush=True)
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


def paired_td_delta(res_a, res_b, label_a, label_b) -> str:
    """Paired mean difference in total TDs per game (seed-level cancellation)."""
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
        f"=== PAIRED A/B (touchdowns per game)  {label_a} vs {label_b}  "
        f"n={n} pairs ===",
        f"  TD/game: {label_a} {a_td:.3f}  vs  {label_b} {b_td:.3f}",
        f"  paired delta = {mean:+.3f} TD/game   SE = {se:.3f}   "
        f"95% CI [{lo:+.3f}, {hi:+.3f}]",
        f"  VERDICT: {verdict}"
        + ("" if verdict == "CONFIRMED"
           else " -- CI includes 0; NOT evidence of no effect, see CI width"),
    ])


def compare_pair(name_new: str, name_old: str) -> None:
    _, seeds_old, res_old = du.load_arm(ARMS[name_old])
    _, seeds_new, res_new = du.load_arm(ARMS[name_new])
    if seeds_old != seeds_new:
        sys.exit(f"ABORT: arms {name_old}/{name_new} ran on different seed "
                 f"lists -- not paired")
    print(f"########## {name_new} (candidate) vs {name_old} (reference) "
          f"##########", flush=True)
    print(paired_td_delta(res_new, res_old, name_new, name_old), flush=True)
    print(flush=True)
    print(du.mcnemar_report(res_new, res_old, outcome="draw"), flush=True)
    print(flush=True)
    print(du.mcnemar_report(res_new, res_old, outcome="home_win"), flush=True)
    print(flush=True)


def compare() -> None:
    missing = [a for a, p in ARMS.items() if not Path(p).exists()]
    if missing:
        sys.exit(f"missing arm(s): {missing} -- run them first")
    for older, newer in pairwise(CHAIN):
        compare_pair(newer, older)
    # Cumulative effect of the whole day in one line of attribution.
    compare_pair("lane2b", "pre")


if __name__ == "__main__":
    mode = sys.argv[1] if len(sys.argv) > 1 else ""
    if mode == "compare":
        compare()
    elif mode in ARMS:
        run(mode, int(sys.argv[2]) if len(sys.argv) > 2 else 400)
    else:
        sys.exit(__doc__)
