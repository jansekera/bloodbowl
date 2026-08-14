#!/usr/bin/env python3
"""Paired-seed A/B: policy_blend=0.15 bring-up on the post-fix engine
(2026-07-16), the exact mirror of the vf_blend Phase 0 retest running today.

Question: the policy network IS trained in production (--policy-lr=0.01,
imitation on MCTS decisions) but its output is NEVER blended into the search
priors (policy_blend defaults to 0.0 everywhere: training_loop.py:52,
run_iteration.py POLICY_BLEND env default 0.0, and _gate_game/_benchmark_game
never pass policy_blend to simulate_game_logged at all). Does turning the
knob to a conservative 0.15 help, hurt, or do nothing on TODAY's engine
(post ADVANCE-prior-floor fix 61a96ed, hasActed 0bb378b, throw-in
2c4ff02/35d275f)?

Wiring (verified in engine/python/bb_module.cpp:406-457 and
engine/src/macro_mcts.cpp:242-430):
  * policy_blend is a RUNTIME kwarg of simulate_game_logged -- no rebuild.
    It is a single global knob: both sides of one game share the same
    policyNet AND the same policyBlend, so there is no per-side blend
    (measure_policy_metrics.py:274 already documented this). Hence no
    head-to-head "blend vs no-blend in one game" arm is possible without an
    engine change; the mirror arms measure game-DYNAMICS change and the
    benchmark arms measure per-side strength (only the macro_mcts side has
    a policy net; the random side never does).
  * expand() computes heuristic priors WITH the floor/cap battery first
    (incl. the brand-new unconditional ADVANCE floor), then
    prior = (1-blend)*heuristic + blend*softmax(policy logits), renorm.
    So policy_blend>0 dilutes the ADVANCE floor by (1-blend) unless the
    policy itself likes ADVANCE -- the same dilution mechanism as the
    caa99da "VF inversion". That is the specific risk this experiment
    measures on the new baseline.

Prior art checked, all stale or underpowered:
  * measure_C_policy_strength.py 07-01 (evidence/C_policy_strength_short.out):
    blend 0.0 vs 0.3, N=12/arm, flat -- pre-fix engine, no power.
  * measure_t1_search_wiring.py proposed 0.15 as the bring-up level
    (visit-entropy instrument only, no game outcomes).
  * diag_gate_policy_prior.py 07-02 tested policy-file PRESENCE (floors on,
    blend 0) -- that regime is production now and is this experiment's
    CONTROL, not its candidate.
0.15 keeps the vf_blend Phase 0 precedent and the t1 proposal.

Arms (same binary; all read ONLY the frozen policyblend snapshots, which are
md5-identical to today's vfbringup snapshots -- b426c64d / dd221471):
    pb0     mirror, champion both sides, policy net loaded, blend=0.0
            == production gate regime (control)                   N=300
    pb015   mirror, champion both sides, blend=0.15               N=300
    bm0     macro_mcts (champion+policy, blend=0.0) HOME vs random  N=200
    bm015   same but blend=0.15 -- caa99da failure-mode tripwire    N=200
All vf_blend=0.0 (production). MCTS=100, TV=1000, dirichlet 0.0, C=1.0
(GATE_* eval defaults), matching the vf retest regime exactly.

Seeds: fresh panel base_seed=20260722 (house sequence; 20260721 = vf retest).
bm arms use the explicit [:200] slice of the same 300-seed panel -- no
reliance on the random.sample prefix property.

Isolation: snapshot weights only (live training run overwrites
weights_policy.json when its iteration ends ~02:00); launch is queued behind
the running vf_blend retest (8 workers, CPU already at 0% idle) via
run_policy_blend_queued_20260716.sh, then uses WORKERS=8 itself.

Usage:  python3 diag_policy_blend_bringup_20260716.py <arm>|all|compare
        PB_SMOKE=1 ... -> n=8/arm, files arm_pb_smoke_*, 2 workers
"""
import math
import os
import statistics
import sys
from pathlib import Path

sys.path.insert(0, "python")
sys.path.insert(0, "engine/build")

import diag_utils as du
from run_iteration import _RACES, GATE_DIRICHLET_ALPHA, GATE_EXPLORATION_C

W = "weights_best_policyblend_snapshot_20260716.json"
POLICY = "weights_policy_policyblend_snapshot_20260716.json"
TV, MCTS = 1000, 100
BASE_SEED = 20260722
SMOKE = os.environ.get("PB_SMOKE") == "1"
WORKERS = int(os.environ.get("PB_WORKERS", "2" if SMOKE else "8"))

ARMS = {
    "pb0":   dict(pb=0.0,  bench=False, n=8 if SMOKE else 300),
    "pb015": dict(pb=0.15, bench=False, n=8 if SMOKE else 300),
    "bm0":   dict(pb=0.0,  bench=True,  n=8 if SMOKE else 200),
    "bm015": dict(pb=0.15, bench=True,  n=8 if SMOKE else 200),
}
ORDER = ["pb0", "pb015", "bm0", "bm015"]


def _pb_gate_game(args):
    """_gate_game clone + policy_blend passthrough (the production helper
    cannot express policy_blend -- that is the finding, not an oversight to
    fix here). Candidate always HOME; no side-swap (symmetric mirror)."""
    seed, race_idx, mcts, vf_blend, tv, policy_path, policy_blend = args
    import bb_engine
    hr = bb_engine.get_developed_roster(_RACES[race_idx % len(_RACES)], tv)
    ar = bb_engine.get_developed_roster(_RACES[(race_idx + 1) % len(_RACES)], tv)
    result = bb_engine.simulate_game_logged(
        hr, ar,
        home_ai='macro_mcts', away_ai='macro_mcts',
        seed=seed, mcts_iterations=mcts,
        weights_path=W, away_weights_path=W,
        epsilon=0.0, vf_blend=vf_blend,
        policy_weights_path=policy_path,
        policy_blend=policy_blend,
        dirichlet_alpha=GATE_DIRICHLET_ALPHA,
        exploration_c=GATE_EXPLORATION_C,
    ).result
    return result.home_score, result.away_score


def _pb_benchmark_game(args):
    """_benchmark_game clone + policy_blend, returning SCORES not bool so the
    bm arms also yield TD/game. Candidate always HOME vs random."""
    seed, race_idx, mcts, vf_blend, tv, policy_path, policy_blend = args
    import bb_engine
    hr = bb_engine.get_developed_roster(_RACES[race_idx % len(_RACES)], tv)
    ar = bb_engine.get_developed_roster(_RACES[(race_idx + 1) % len(_RACES)], tv)
    result = bb_engine.simulate_game_logged(
        hr, ar,
        home_ai='macro_mcts', away_ai='random',
        seed=seed, mcts_iterations=mcts,
        weights_path=W,
        epsilon=0.0, vf_blend=vf_blend,
        policy_weights_path=policy_path,
        policy_blend=policy_blend,
        dirichlet_alpha=GATE_DIRICHLET_ALPHA,
        exploration_c=GATE_EXPLORATION_C,
    ).result
    return result.home_score, result.away_score


def path_for(arm: str) -> str:
    return (f"arm_pb_smoke_{arm}_20260716.json" if SMOKE
            else f"arm_pb_{arm}_20260716.json")


def seeds_for(arm: str) -> list:
    panel = du.paired_seeds(8 if SMOKE else 300, base_seed=BASE_SEED)
    return panel[:ARMS[arm]["n"]]


def run(arm: str) -> None:
    a = ARMS[arm]
    seeds = seeds_for(arm)
    fn = _pb_benchmark_game if a["bench"] else _pb_gate_game
    tasks = [(s, i, MCTS, 0.0, TV, POLICY, a["pb"]) for i, s in enumerate(seeds)]
    print(f"=== policy_blend bring-up: arm={arm} N={a['n']} pb={a['pb']} "
          f"base_seed={BASE_SEED} MCTS={MCTS} TV={TV} workers={WORKERS} "
          f"weights={W} policy={POLICY} ===", flush=True)
    res = du.run_arm(arm, tasks, game_fn=fn, workers=WORKERS,
                     mcts_iterations=MCTS)
    du.save_arm(path_for(arm), arm, seeds, res)
    print(gate_summary(arm, res, a["n"]), flush=True)


def gate_summary(label: str, res: dict, n_total: int) -> str:
    w = d = l = tds = 0
    for r in res.values():
        tds += r[0] + r[1]
        w += r[0] > r[1]
        d += r[0] == r[1]
        l += r[0] < r[1]
    n = len(res)
    dec = w + l
    return (f"[{label}] completed {n}/{n_total} "
            f"({n_total - n} watchdog-skipped)\n"
            f"[{label}] {w}W {d}D {l}L (home-first)  draws {d / n:.1%}  "
            f"TD/game {tds / n:.2f}  home decisive share "
            f"{(w / dec if dec else float('nan')):.1%} (decisive n={dec})")


def paired_td(res_a, res_b, label_a, label_b):
    common = sorted(set(res_a) & set(res_b))
    diffs = [(res_a[i][0] + res_a[i][1]) - (res_b[i][0] + res_b[i][1])
             for i in common]
    mean_d = statistics.fmean(diffs)
    se = (statistics.stdev(diffs) / math.sqrt(len(diffs))
          if len(diffs) > 1 else 0.0)
    print(f"=== PAIRED TD/game  {label_a} vs {label_b}  n={len(common)} pairs ===")
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
        print(gate_summary(a, res, ARMS[a]["n"]))
    print()

    # --- mirror: dynamics (draw-collapse / stall tripwire) ------------------
    if "pb0" in arms and "pb015" in arms:
        _, s0, r0 = arms["pb0"]
        _, s1, r1 = arms["pb015"]
        assert s0 == s1, "mirror seed lists differ -- not paired"
        for outcome in ("draw", "home_win"):
            print(du.mcnemar_report(r1, r0, outcome,
                                    label_a="pb015", label_b="pb0"))
            print()
        paired_td(r1, r0, "pb015", "pb0")
        print()
        # reverse null-test: blend must demonstrably reach search; ~100%
        # identical per-seed scorelines would mean the knob is dead.
        common = sorted(set(r0) & set(r1))
        same = sum(tuple(r0[i]) == tuple(r1[i]) for i in common)
        print(f"reverse null-test: identical per-seed scorelines pb015 vs "
              f"pb0: {same}/{len(common)} ({same / len(common):.1%}) -- low "
              f"= blend demonstrably changes play; ~100% = knob dead")
        print()

    # --- benchmark vs random: per-side strength (caa99da tripwire) ----------
    if "bm0" in arms and "bm015" in arms:
        _, sb0, b0 = arms["bm0"]
        _, sb1, b1 = arms["bm015"]
        assert sb0 == sb1, "bm seed lists differ -- not paired"
        for outcome in ("home_win", "draw"):
            print(du.mcnemar_report(b1, b0, outcome,
                                    label_a="bm015", label_b="bm0"))
            print()
        paired_td(b1, b0, "bm015", "bm0")


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
