#!/usr/bin/env python3
"""Paired-seed A/B: REPOSITION prior-floor fix (loose-ball context), the
biggest single finding of today's systematic macro-floor audit (evidence/
fable_macro_floor_audit_20260717.md). REPOSITION currently has NO prior
floor outside onDef contexts; in the loose-ball corpus screen it was the
argmaxQ candidate in 82% of roots where it was ever best, with margins up
to +0.76, and still got starved of visits -- same bug class as CAGE
(2026-07-03) and ADVANCE (2026-07-16), both of which were real, shipped,
confirmed-effect fixes. This checks whether the single-corpus-screen signal
survives to actual game-level draw-rate/TD-rate deltas before shipping.

Uses a scratch python extension built against engine_build_reposition_test/
(getenv-gated BB_REPOSITION_FIX, off=stock 07-16 behavior, on=unconditional
0.05 floor for REPOSITION regardless of onDef) -- NOT production engine/build.

Usage: python3 diag_reposition_ab_20260717.py <arm>|all|compare
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
BASE_SEED = 20260724   # house sequence: next free after 20260723 (lever c)
WORKERS = 4  # lever-c A/B already holds 8 cores concurrently (2026-07-17)
N = 150

ARMS = {
    "off": dict(fix=False, n=N),
    "on":  dict(fix=True,  n=N),
}
ORDER = ["off", "on"]


def path_for(arm: str) -> str:
    return f"arm_repo_ab_{arm}_20260717.json"


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
    print(f"=== reposition-fix A/B: arm={arm} N={a['n']} fix={a['fix']} "
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
