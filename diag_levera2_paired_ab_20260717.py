#!/usr/bin/env python3
"""Paired-seed A/B: lever (a) v2 (BB_LEVER_A2) ADVANCE prior-floor bump,
game-level follow-up to the single-state deepdive
(engine_build_levera2_test/arm_off.out / arm_on.out, 2026-07-17).

That deepdive found the reworked lever (a) -- estimateMarkedCarrierDodgeFail()
using the real calculateDodgeTarget() over the carrier's legal exit squares,
gating an ADVANCE prior-floor bump 0.12->0.20 in MacroMCTSSearch::expand()
when the marked carrier's dodge-fail estimate is <=30% -- produces the
largest single-state effect measured today on the flagship stalled-carrier
scenario: ADVANCE chosen 3.8%->27.0% (7x), BLOCK dominance 62.5%->44.0%
(bigger than lever (c)'s 3.8%->16.2%). This script checks whether that
single-state signal survives to actual game-level draw-rate/TD-rate metrics.

Uses a SEPARATE python extension built against the scratch lever-a2 engine
(engine_build_levera2_test/bb_engine.cpython-312-*.so, dynamically linked to
engine_build_levera2_test/libbb_engine.so which has the getenv-gated lever
code) -- NOT the production engine/build module. BB_LEVER_A2 is set in this
process's environ before the worker Pool is created so forked/spawned
workers inherit it; BB_LEVER_A2_DEBUG, BB_LEVER_A and BB_LEVER_C are left
unset throughout (isolating lever a2 only).

Usage: python3 diag_levera2_paired_ab_20260717.py <arm>|all|smoke|compare
"""
import os
import sys
from pathlib import Path

sys.path.insert(0, "python")
sys.path.insert(0, "engine/build")

import diag_utils as du
from run_iteration import GATE_DIRICHLET_ALPHA, GATE_EXPLORATION_C

W = "weights_best.json"
POLICY_PATH = "weights_policy.json"
TV, MCTS = 1000, 100
BASE_SEED = 20260726   # house sequence: next free (20260723=lever-c N150,
                       # 20260724=reposition fix, 20260725=lever-c N400)
WORKERS = 8
N = 150
ENV_FLAG = "BB_LEVER_A2"
UNSET_FLAGS = ("BB_LEVER_A2_DEBUG", "BB_LEVER_A", "BB_LEVER_C")

ARMS = {
    "off": dict(lever=False, n=N),
    "on":  dict(lever=True,  n=N),
}
ORDER = ["off", "on"]


def path_for(arm: str, smoke: bool = False) -> str:
    tag = "smoke_" if smoke else ""
    return f"arm_levera2_ab_{tag}{arm}_20260717.json"


def tasks_for(seeds):
    # 10-tuple _gate_game form: candidate weights on both sides (mirror,
    # champion vs champion), no side-swap needed since env var alone
    # differs between arms, not weights.
    return [(s, i, W, W, MCTS, 0.0, TV, False, POLICY_PATH, False)
            for i, s in enumerate(seeds)]


def gate_summary(label: str, res: dict, n_total: int) -> str:
    w = d = l = tds = 0
    for r in res.values():
        hs, as_ = r[0], r[1]
        tds += hs + as_
        w += hs > as_
        d += hs == as_
        l += hs < as_
    n = len(res)
    dec = w + l
    return (f"[{label}] completed {n}/{n_total} "
            f"({n_total - n} watchdog-skipped)\n"
            f"[{label}] {w}W {d}D {l}L (home-first)  draws {d / n:.1%}  "
            f"TD/game {tds / n:.2f}  home decisive share "
            f"{(w / dec if dec else float('nan')):.1%} (decisive n={dec})")


def run(arm: str, n: int | None = None, smoke: bool = False) -> None:
    a = ARMS[arm]
    n = n if n is not None else a["n"]
    if a["lever"]:
        os.environ[ENV_FLAG] = "1"
    else:
        os.environ.pop(ENV_FLAG, None)
    for f in UNSET_FLAGS:
        os.environ.pop(f, None)
    du.INIT_ARGS = ("engine_build_levera2_test", "python")
    seeds = du.paired_seeds(n, base_seed=BASE_SEED)
    print(f"=== lever-a2 A/B{' SMOKE' if smoke else ''}: arm={arm} N={n} "
          f"lever_a2={a['lever']} base_seed={BASE_SEED} MCTS={MCTS} TV={TV} "
          f"workers={WORKERS} weights={W} "
          f"engine=engine_build_levera2_test ===", flush=True)
    res = du.run_arm(arm, tasks_for(seeds), workers=WORKERS,
                     mcts_iterations=MCTS)
    du.save_arm(path_for(arm, smoke=smoke), arm, seeds, res)
    print(gate_summary(arm, res, n), flush=True)


def compare() -> None:
    arms = {}
    for a in ORDER:
        p = Path(path_for(a))
        if p.exists():
            arms[a] = du.load_arm(p)
    print(f"\n{'=' * 16} COMPARE {'=' * 16}\n")
    for a, (lbl, seeds, res) in arms.items():
        print(gate_summary(a, res, ARMS[a]["n"]))
    print()
    if "off" in arms and "on" in arms:
        _, s_off, r_off = arms["off"]
        _, s_on, r_on = arms["on"]
        assert s_off == s_on, "seed lists differ -- not paired"
        for outcome in ("draw", "home_win"):
            print(du.mcnemar_report(r_on, r_off, outcome,
                                    label_a="leverA2_on",
                                    label_b="leverA2_off"))
            print()


if __name__ == "__main__":
    mode = sys.argv[1] if len(sys.argv) > 1 else ""
    if mode == "compare":
        compare()
    elif mode == "smoke":
        for a in ORDER:
            run(a, n=3, smoke=True)
        print("SMOKE OK", flush=True)
    elif mode == "all":
        for a in ORDER:
            run(a)
        compare()
    elif mode in ARMS:
        run(mode)
    else:
        sys.exit(__doc__)
