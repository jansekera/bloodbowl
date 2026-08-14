#!/usr/bin/env python3
"""Paired-seed A/B: SCORE-availability patches A+B (proposals_score_availability_20260714.md),
applied 2026-07-20 -- see evidence/score_availability_patch_20260720.md.

Patch A (macro_mcts.cpp): direct SCORE with a safe walk-in (no GFI) gets a
0.30 prior floor instead of the generic mid-game floor (~0.08 -> ~0.065
post-renorm, matching the observed SCORE-chosen-in-can-score-states of 7.7%).
Patch B (macro_actions.cpp): carrier-activation guard -- while a direct
SCORE is on the table, BLOCK/FOUL/BLITZ candidates may not spend the
carrier's activation (found necessary via a diagnostic replay showing Patch A
alone left the carrier vulnerable to being "blitzed away" on itself when it
was the only player able to blitz an adjacent marker).

"off" = HEAD (a5dd758, pre-patch) built in a git worktree at
../bloodbowl_scoreavail_off/engine/build. "on" = this tree's engine/build
(patches applied, uncommitted, 426/426 tests green).

Primary metrics per the proposal's own validation plan (docs section 6):
draw rate (expect DECREASE -- offensive fix, opposite direction from screen
fixes), home_win (secondary), TD/game via score-sum proxy is NOT captured by
_gate_game's (home_score, away_score) return -- draws/wins is what we get
from this harness; conversion-rate mining is a separate, cheaper decision-
level step (not run here).

Usage: python3 diag_score_availability_ab_20260720.py <arm>|all|compare
"""
import sys
from pathlib import Path

sys.path.insert(0, "python")
sys.path.insert(0, "engine/build")

import diag_utils as du

W = "weights_best.json"
POLICY_PATH = "weights_policy.json"
TV, MCTS = 1200, 100
BASE_SEED = 20260720
WORKERS = 10
N = 150

ARMS = {
    "off": dict(engine="../bloodbowl_scoreavail_off/engine/build", n=N),
    "on":  dict(engine="engine/build", n=N),
}
ORDER = ["off", "on"]


def path_for(arm: str) -> str:
    return f"arm_scoreavail_ab_{arm}_20260720.json"


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
    du.INIT_ARGS = (a["engine"], "python")
    seeds = du.paired_seeds(a["n"], base_seed=BASE_SEED)
    print(f"=== score-availability A/B: arm={arm} N={a['n']} "
          f"base_seed={BASE_SEED} MCTS={MCTS} TV={TV} workers={WORKERS} "
          f"weights={W} engine={a['engine']} ===", flush=True)
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
                                    label_a="scoreavail_on", label_b="scoreavail_off"))
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
