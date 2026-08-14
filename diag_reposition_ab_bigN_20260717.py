#!/usr/bin/env python3
"""Paired-seed A/B: REPOSITION prior-floor fix, big-N follow-up to the N=150
run (diag_reposition_ab_20260717.py, base_seed=20260724), which came back
INCONCLUSIVE -- and with the point estimate trending the WRONG direction
(draws 31.3% off -> 38.9% on, delta +7.4pp, CI [-3.2,+17.9]pp; home_win
-0.7pp, CI [-10.4,+9.1]pp -- both include zero). The two prior confirmed
fixes of this bug class (CAGE 2026-07-03, ADVANCE 2026-07-16) both REDUCED
draws, so a real draw-increasing effect here would be a ship-blocker. Per the
project noise-floor convention (<10pp at N=150 = inconclusive), this re-runs
the exact same A/B at N=400 with a fresh base seed to tell "truly no effect"
from "real but small effect" (in either direction).

Uses the SAME scratch python extension built against
engine_build_reposition_test/ (getenv-gated BB_REPOSITION_FIX, off=stock
07-16 behavior, on=unconditional 0.05 floor for REPOSITION regardless of
onDef) -- NOT production engine/build. BB_REPOSITION_FIX is set in this
process's environ before the worker Pool is created so forked/spawned
workers inherit it.

Usage: python3 diag_reposition_ab_bigN_20260717.py <arm>|all|compare
"""
import os
import sys
from pathlib import Path

sys.path.insert(0, "python")
sys.path.insert(0, "engine/build")

import diag_utils as du

W = "weights_best.json"
POLICY_PATH = "weights_policy.json"
TV, MCTS = 1000, 100
BASE_SEED = 20260727   # house sequence: next free (20260725=lever-c N400, 20260726=lever-a2 N150)
WORKERS = 4  # deliberately kept at 4: several concurrent background jobs today, ~12-core box
N = 400

ARMS = {
    "off": dict(fix=False, n=N),
    "on":  dict(fix=True,  n=N),
}
ORDER = ["off", "on"]


def path_for(arm: str) -> str:
    return f"arm_repo_ab_bigN_{arm}_20260717.json"


def tasks_for(seeds):
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
    if a["fix"]:
        os.environ["BB_REPOSITION_FIX"] = "1"
    else:
        os.environ.pop("BB_REPOSITION_FIX", None)
    du.INIT_ARGS = ("engine_build_reposition_test", "python")
    seeds = du.paired_seeds(a["n"], base_seed=BASE_SEED)
    print(f"=== reposition-fix A/B bigN: arm={arm} N={a['n']} fix={a['fix']} "
          f"base_seed={BASE_SEED} MCTS={MCTS} TV={TV} workers={WORKERS} "
          f"weights={W} engine=engine_build_reposition_test ===", flush=True)
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
                                    label_a="repo_on", label_b="repo_off"))
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
