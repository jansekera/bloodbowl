#!/usr/bin/env python3
"""Paired-seed A/B: lever (c) stallPacing-discount, big-N follow-up to the
N=150 run (diag_lever_c_paired_ab_20260717.py, base_seed=20260723), which
came back INCONCLUSIVE (draw delta -2.0pp, CI [-13.3,+9.3]pp; home_win
-4.7pp, CI [-14.1,+4.8]pp -- both include zero). Per the project noise-floor
convention (<10pp at N=150 = inconclusive), this re-runs the exact same A/B
at N=400 with a fresh base seed to tell "truly no effect" from "real but
small effect".

Uses the SAME scratch python extension built against the lever engine
(engine_build_lever_test/bb_engine.cpython-312-*.so, dynamically linked to
engine_build_lever_test/libbb_engine.so with the getenv-gated lever code) --
NOT the production engine/build module. BB_LEVER_C is set in this process's
environ before the worker Pool is created so forked/spawned workers inherit
it; BB_LEVER_A is left unset throughout.

Usage: python3 diag_lever_c_paired_ab_bigN_20260717.py <arm>|all|compare
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
BASE_SEED = 20260725   # house sequence: next free (20260723=lever-c N150, 20260724=reposition fix)
WORKERS = 8
N = 400

ARMS = {
    "off": dict(lever=False, n=N),
    "on":  dict(lever=True,  n=N),
}
ORDER = ["off", "on"]


def path_for(arm: str) -> str:
    return f"arm_leverc_ab_bigN_{arm}_20260717.json"


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


def run(arm: str) -> None:
    a = ARMS[arm]
    if a["lever"]:
        os.environ["BB_LEVER_C"] = "1"
    else:
        os.environ.pop("BB_LEVER_C", None)
    os.environ.pop("BB_LEVER_A", None)
    du.INIT_ARGS = ("engine_build_lever_test", "python")
    seeds = du.paired_seeds(a["n"], base_seed=BASE_SEED)
    print(f"=== lever-c A/B bigN: arm={arm} N={a['n']} lever_c={a['lever']} "
          f"base_seed={BASE_SEED} MCTS={MCTS} TV={TV} workers={WORKERS} "
          f"weights={W} engine=engine_build_lever_test ===", flush=True)
    res = du.run_arm(arm, tasks_for(seeds), workers=WORKERS,
                     mcts_iterations=MCTS)
    du.save_arm(path_for(arm), arm, seeds, res)
    print(gate_summary(arm, res, a["n"]), flush=True)


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
                                    label_a="leverC_on", label_b="leverC_off"))
            print()


if __name__ == "__main__":
    mode = sys.argv[1] if len(sys.argv) > 1 else ""
    if mode == "compare":
        compare()
    elif mode == "all":
        for a in ORDER:
            run(a)
        compare()
    elif mode in ARMS:
        run(mode)
    else:
        sys.exit(__doc__)
