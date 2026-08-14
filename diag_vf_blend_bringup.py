#!/usr/bin/env python3
"""Paired-seed A/B: vf_blend=0.15 bring-up, search-only (no training).

Arms (same binary -- vf_blend is a runtime knob, no rebuild):
    vf0     control, both sides weights_best.json, vf_blend=0.0
            -> normally NOT run: reuse arm_chain_lane2b.json (same seeds,
               same binary); run only if the binary/SHA check fails
    vf015   both sides weights_best.json, vf_blend=0.15          (phase 0a)
    null015 HOME weights_path='' (pure heuristic) vs AWAY champion,
            vf_blend=0.15                                        (phase 0b)
    bm0 / bm015   _benchmark_game vs random, paired               (phase 0c)

Proposal: proposals_vf_blend_bringup_20260714.md
"""
import sys
from pathlib import Path

sys.path.insert(0, "python")
sys.path.insert(0, "engine/build")

import diag_utils as du
from run_iteration import _benchmark_game, _gate_game

W = "weights_best.json"
POLICY_PATH = "weights_policy.json"
TV, MCTS = 1000, 100
BASE_SEED = 20260714          # shared with the chain -> lane2b is the control
LANE2B = "arm_chain_lane2b.json"

ARMS = {
    "vf0":     dict(vf=0.0,  home=W,  n=400, fn=_gate_game),
    "vf015":   dict(vf=0.15, home=W,  n=400, fn=_gate_game),
    "null015": dict(vf=0.15, home="", n=300, fn=_gate_game),
    "bm0":     dict(vf=0.0,  n=200, fn=_benchmark_game),
    "bm015":   dict(vf=0.15, n=200, fn=_benchmark_game),
}


def tasks_for(arm: str, seeds):
    a = ARMS[arm]
    if a["fn"] is _benchmark_game:
        return [(s, i, W, MCTS, a["vf"], TV, POLICY_PATH)
                for i, s in enumerate(seeds)]
    return [(s, i, a["home"], W, MCTS, a["vf"], TV, False, POLICY_PATH)
            for i, s in enumerate(seeds)]


def run(arm: str) -> None:
    a = ARMS[arm]
    seeds = du.paired_seeds(a["n"], base_seed=BASE_SEED)
    print(f"=== vf_blend bring-up: arm={arm} N={a['n']} vf={a['vf']} ===",
          flush=True)
    res = du.run_arm(arm, tasks_for(arm, seeds), game_fn=a["fn"],
                     mcts_iterations=MCTS)
    du.save_arm(f"arm_vfb_{arm}.json", arm, seeds, res)


def compare() -> None:
    ctrl_path = Path("arm_vfb_vf0.json")
    if not ctrl_path.exists():
        ctrl_path = Path(LANE2B)          # reuse the chain baseline arm
    _, s0, r0 = du.load_arm(ctrl_path)
    _, s1, r1 = du.load_arm("arm_vfb_vf015.json")
    assert s0[:len(s1)] == s1[:len(s0)], "seed lists differ -- not paired"
    for outcome in ("draw", "home_win"):
        print(du.mcnemar_report(r1, r0, outcome,
                                label_a="vf015", label_b="vf0"))
        print()
    # TD/game paired delta -- same math as diag_h2_screens_chain.paired_td_delta
    common = sorted(set(r0) & set(r1))
    diffs = [(r1[i][0] + r1[i][1]) - (r0[i][0] + r0[i][1]) for i in common]
    n = len(diffs); mean = sum(diffs) / n
    var = sum((d - mean) ** 2 for d in diffs) / (n - 1)
    se = (var / n) ** 0.5
    print(f"TD/game paired delta vf015-vf0: {mean:+.3f} "
          f"(95% CI [{mean - 1.96 * se:+.3f}, {mean + 1.96 * se:+.3f}])")
    # 0b: null (heuristic) home vs champion away, slot-corrected by the
    # vf015 mirror home_win share
    if Path("arm_vfb_null015.json").exists():
        _, _, rn = du.load_arm("arm_vfb_null015.json")
        dec = [(r[0] > r[1]) for r in rn.values() if r[0] != r[1]]
        mir = [(r[0] > r[1]) for r in r1.values() if r[0] != r[1]]
        print(f"\n0b null@H vs champ@A (vf=0.15): "
              f"{sum(dec)}/{len(dec)} home(heuristic) decisive share "
              f"= {sum(dec)/len(dec):.1%}  |  slot edge from vf015 mirror "
              f"= {sum(mir)/len(mir):.1%}  (difference = head effect)")
        print(f"   draws null015 {100*sum(1 for r in rn.values() if r[0]==r[1])/len(rn):.1f}% "
              f"vs vf015 mirror {100*sum(1 for r in r1.values() if r[0]==r[1])/len(r1):.1f}%")
    if Path("arm_vfb_bm015.json").exists():
        _, _, b0 = du.load_arm("arm_vfb_bm0.json")
        _, _, b1 = du.load_arm("arm_vfb_bm015.json")
        print()
        print(du.mcnemar_report(b1, b0, "win", label_a="bm015", label_b="bm0"))


if __name__ == "__main__":
    mode = sys.argv[1] if len(sys.argv) > 1 else ""
    if mode == "compare":
        compare()
    elif mode in ARMS:
        run(mode)
    else:
        sys.exit(__doc__)
