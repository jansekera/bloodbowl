#!/usr/bin/env python3
"""Paired-seed A/B: vf_blend=0.15 Phase 0 bring-up RETEST on the post-fix
engine (2026-07-16).

Why a full re-run: the 2026-07-15 arms (arm_vfb_*.json) and the
arm_chain_lane2b.json control were measured on a binary that predates three
confirmed behavior-changing fixes -- hasActed double-activation (0bb378b),
throw-in bounce/direction (2c4ff02, 35d275f) and above all the ADVANCE
MCTS-prior-floor fix (61a96ed: draws 63.3%->47.0%, TD/game 0.43->0.73 in a
paired N=300 A/B). Those files are kept as historical records but are NOT
used here. Everything below is fresh: fresh seed panel (base_seed=20260721,
next free in the house sequence), fresh output filenames (arm_vfb2_*).

Isolation from the live training run (training.pid, started 14:00 today):
all arms read ONLY the frozen snapshots taken at experiment start --
  weights_best_vfbringup_snapshot_20260716.json   (md5 b426c64d...)
  weights_policy_vfbringup_snapshot_20260716.json (md5 dd221471...)
-- never the live weights_best.json / weights_policy.json. No rebuild
(vf_blend is a runtime knob; binary = engine/build 2026-07-16 12:41,
HEAD 61a96ed). WORKERS=8, not the house 12, to leave headroom for the
training run's own pool.

Arms (design = proposals_vf_blend_bringup_20260714.md, plus one addition):
    vf0     control, both sides champion snapshot, vf_blend=0.0   (phase 0a)
    vf015   both sides champion snapshot, vf_blend=0.15           (phase 0a)
    null0   HOME weights_path='' (pure heuristic) vs AWAY champion,
            vf_blend=0.0    -- NEW control arm vs the 07-14 design: gives a
            same-seed paired reference for null015, so the head effect is
            measured directly (null015 vs null0) instead of only via the
            mirror slot-edge arithmetic                           (phase 0b)
    null015 HOME '' vs AWAY champion, vf_blend=0.15               (phase 0b)
    bm0/bm015  _benchmark_game vs random, paired                  (phase 0c)

Seed pairing across different n relies on the paired_seeds prefix property
(random.Random(seed).sample first-300-of-400 == sample-of-300; verified True
for base_seed=20260721 before launch).

Usage:  python3 diag_vf_blend_phase0_retest_20260716.py <arm>|all|compare
"""
import math
import statistics
import sys
from pathlib import Path

sys.path.insert(0, "python")
sys.path.insert(0, "engine/build")

import diag_utils as du
from run_iteration import _benchmark_game, _gate_game

W = "weights_best_vfbringup_snapshot_20260716.json"
POLICY_PATH = "weights_policy_vfbringup_snapshot_20260716.json"
TV, MCTS = 1000, 100          # same eval regime as the 07-14/15 phase-0 design
BASE_SEED = 20260721          # house sequence: 16/17/18/20 taken this week
WORKERS = 8                   # live training run holds the other cores

ARMS = {
    "vf0":     dict(vf=0.0,  home=W,  n=400, fn=_gate_game),
    "vf015":   dict(vf=0.15, home=W,  n=400, fn=_gate_game),
    "null0":   dict(vf=0.0,  home="", n=300, fn=_gate_game),
    "null015": dict(vf=0.15, home="", n=300, fn=_gate_game),
    "bm0":     dict(vf=0.0,  n=200, fn=_benchmark_game),
    "bm015":   dict(vf=0.15, n=200, fn=_benchmark_game),
}
ORDER = ["vf0", "vf015", "null0", "null015", "bm0", "bm015"]


def path_for(arm: str) -> str:
    return f"arm_vfb2_{arm}_20260716.json"


def tasks_for(arm: str, seeds):
    a = ARMS[arm]
    if a["fn"] is _benchmark_game:
        # 7-tuple: candidate always HOME vs random (matches 07-15 design)
        return [(s, i, W, MCTS, a["vf"], TV, POLICY_PATH)
                for i, s in enumerate(seeds)]
    # 9-tuple: candidate always HOME, no side-swap -- REQUIRED for the null
    # arms (an empty away_weights_path falls back to home weights in
    # bb_module.cpp, so the heuristic side can only sit in the HOME slot)
    return [(s, i, a["home"], W, MCTS, a["vf"], TV, False, POLICY_PATH)
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
    seeds = du.paired_seeds(a["n"], base_seed=BASE_SEED)
    print(f"=== vfb2 retest: arm={arm} N={a['n']} vf={a['vf']} "
          f"base_seed={BASE_SEED} MCTS={MCTS} TV={TV} workers={WORKERS} "
          f"weights={W} ===", flush=True)
    res = du.run_arm(arm, tasks_for(arm, seeds), game_fn=a["fn"],
                     workers=WORKERS, mcts_iterations=MCTS)
    du.save_arm(path_for(arm), arm, seeds, res)
    if a["fn"] is _gate_game:
        print(gate_summary(arm, res, a["n"]), flush=True)
    else:
        wins = sum(bool(r) for r in res.values())
        print(f"[{arm}] completed {len(res)}/{a['n']}  "
              f"win-rate vs random {wins / len(res):.1%}", flush=True)


def paired_td(res_a, res_b, label_a, label_b):
    common = sorted(set(res_a) & set(res_b))
    diffs = [(res_a[i][0] + res_a[i][1]) - (res_b[i][0] + res_b[i][1])
             for i in common]
    mean_d = statistics.fmean(diffs)
    se = (statistics.stdev(diffs) / math.sqrt(len(diffs))
          if len(diffs) > 1 else 0.0)
    print(f"=== PAIRED TD/game  {label_a} vs {label_b}  "
          f"n={len(common)} pairs ===")
    print(f"  TD/game: {label_a} "
          f"{statistics.fmean(res_a[i][0] + res_a[i][1] for i in common):.2f}"
          f"  vs  {label_b} "
          f"{statistics.fmean(res_b[i][0] + res_b[i][1] for i in common):.2f}")
    print(f"  paired delta = {mean_d:+.3f} TD/game   SE = {se:.3f}   "
          f"95% CI [{mean_d - du.Z95 * se:+.3f}, {mean_d + du.Z95 * se:+.3f}]")


def compare() -> None:
    arms = {}
    for a in ORDER:
        p = Path(path_for(a))
        if p.exists():
            arms[a] = du.load_arm(p)
    for a, (lbl, seeds, res) in arms.items():
        if ARMS[a]["fn"] is _gate_game:
            print(gate_summary(a, res, ARMS[a]["n"]))
        else:
            wins = sum(bool(r) for r in res.values())
            print(f"[{a}] completed {len(res)}/{ARMS[a]['n']}  "
                  f"win-rate vs random {wins / len(res):.1%}")
    print()

    # --- 0a: symmetric mirror, vf015 vs vf0 (draw-collapse tripwire) -------
    if "vf0" in arms and "vf015" in arms:
        _, s0, r0 = arms["vf0"]
        _, s1, r1 = arms["vf015"]
        assert s0 == s1, "0a seed lists differ -- not paired"
        for outcome in ("draw", "home_win"):
            print(du.mcnemar_report(r1, r0, outcome,
                                    label_a="vf015", label_b="vf0"))
            print()
        paired_td(r1, r0, "vf015", "vf0")
        print()

    # --- 0b: head vs pure heuristic, paired null015 vs null0 ---------------
    if "null0" in arms and "null015" in arms:
        _, sn0, rn0 = arms["null0"]
        _, sn1, rn1 = arms["null015"]
        assert sn0 == sn1, "0b seed lists differ -- not paired"
        # home_loss = champion(away) win; home_win = heuristic(home) win
        for outcome in ("draw", "home_loss", "home_win"):
            print(du.mcnemar_report(rn1, rn0, outcome,
                                    label_a="null015", label_b="null0"))
            print()
        paired_td(rn1, rn0, "null015", "null0")
        print()
        # continuity with the 07-14 design: mirror slot-edge calibration
        if "vf015" in arms:
            _, _, r1 = arms["vf015"]
            for tag, rn in (("null0", rn0), ("null015", rn1)):
                dec = [(r[0] > r[1]) for r in rn.values() if r[0] != r[1]]
                mir = [(r[0] > r[1]) for r in r1.values() if r[0] != r[1]]
                print(f"0b[{tag}] home(heuristic) decisive share "
                      f"{sum(dec)}/{len(dec)} = {sum(dec) / len(dec):.1%}  |  "
                      f"vf015-mirror home slot edge = "
                      f"{sum(mir) / len(mir):.1%}  "
                      f"(difference = head effect, {tag})")
            print()
        # reverse null-test of the instrument: null015 must be
        # DISTINGUISHABLE from null0 for the blend to be flowing at all;
        # per-seed disagreement is the cheapest detector on paired seeds.
        common = sorted(set(rn0) & set(rn1))
        same = sum(tuple(rn0[i]) == tuple(rn1[i]) for i in common)
        print(f"0b reverse null-test: identical per-seed scorelines "
              f"null015 vs null0: {same}/{len(common)} "
              f"({same / len(common):.1%}) -- low value = blend demonstrably "
              f"changes play; ~100% would mean vf_blend never reaches search")
        print()

    # --- 0c: benchmark vs random (caa99da failure-mode replication) --------
    if "bm0" in arms and "bm015" in arms:
        _, sb0, b0 = arms["bm0"]
        _, sb1, b1 = arms["bm015"]
        assert sb0 == sb1, "0c seed lists differ -- not paired"
        print(du.mcnemar_report(b1, b0, "win", label_a="bm015",
                                label_b="bm0"))


if __name__ == "__main__":
    mode = sys.argv[1] if len(sys.argv) > 1 else ""
    if mode == "compare":
        compare()
    elif mode == "all":
        for a in ORDER:
            run(a)
        print("\n================ COMPARE ================\n", flush=True)
        compare()
    elif mode in ARMS:
        run(mode)
    else:
        sys.exit(__doc__)
