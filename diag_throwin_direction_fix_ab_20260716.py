"""Paired-seed A/B validation of the throw-in direction-template fix.

Bug + fix: evidence/fable_throwin_bug_investigation_20260716.md Finding 2 --
resolveThrowIn reused the uniform 8-way Bounce scatter template for throw-in
direction, instead of the distinct LRB6 throw-in template (side exit: D6
1-2/3-4/5-6 = diagonal/straight-in/diagonal; corner exit: D3 straight-along-
one-edge/diagonal/straight-along-the-other-edge). Fix: resolveThrowIn now
classifies the exit edge (side vs corner) from the off-pitch destination and
picks direction accordingly.

This is rarer still than the missing-bounce fix (only throw-ins whose exit
edge/corner would have produced a different direction distribution are
affected) -- no material draw-rate shift is expected. Validation goal is
(a) no crash/watchdog regression, (b) confirm nothing else broke.

  python diag_throwin_direction_fix_ab_20260716.py baseline  [N]
  python diag_throwin_direction_fix_ab_20260716.py candidate [N]
  python diag_throwin_direction_fix_ab_20260716.py report
"""
import math
import statistics
import sys

import diag_utils as du

MODE = sys.argv[1] if len(sys.argv) > 1 else "report"
N = int(sys.argv[2]) if len(sys.argv) > 2 else 300
BASE_SEED = 20260718
WEIGHTS = "weights_best.json"
POLICY = "weights_policy.json"
MCTS = 100
VF_BLEND = 0.0
TV = 1200

ARM_FILES = {
    "baseline": "arm_throwindir_base_20260716.json",
    "candidate": "arm_throwindir_fix_20260716.json",
}


def arm_summary(label, res, n_total):
    w = d = l = tds = 0
    for r in res.values():
        cs, fs, _ = r
        tds += cs + fs
        if cs > fs:
            w += 1
        elif cs == fs:
            d += 1
        else:
            l += 1
    n = len(res)
    dec = w + l
    lines = [
        f"[{label}] completed {n}/{n_total} ({n_total - n} watchdog-skipped)",
        f"[{label}] {w}W {d}D {l}L  draws {d / n:.1%}  "
        f"TD/game {tds / n:.2f}  decisive share (cand) "
        f"{(w / dec if dec else float('nan')):.1%} (decisive n={dec})",
    ]
    return "\n".join(lines)


if MODE in ("baseline", "candidate"):
    seeds = du.paired_seeds(N, base_seed=BASE_SEED)
    tasks = [
        (s, i, WEIGHTS, WEIGHTS, MCTS, VF_BLEND, TV, False, POLICY, i % 2 == 1)
        for i, s in enumerate(seeds)
    ]
    print(f"--- throwindir-fix A/B arm {MODE!r}: mirror null {WEIGHTS}  N={N} "
          f"base_seed={BASE_SEED} MCTS={MCTS} TV={TV} policy={POLICY} ---",
          flush=True)
    res = du.run_arm(MODE, tasks, mcts_iterations=MCTS)
    du.save_arm(ARM_FILES[MODE], MODE, seeds, res)
    print(arm_summary(MODE, res, N), flush=True)
    sys.exit(0)

# ---- report ------------------------------------------------------------------
_, seeds_b, base = du.load_arm(ARM_FILES["baseline"])
_, seeds_c, cand = du.load_arm(ARM_FILES["candidate"])
assert seeds_b == seeds_c, "arms ran on different seed lists"
n_total = len(seeds_b)

print(arm_summary("baseline (pre-fix)", base, n_total))
print(arm_summary("candidate (post-fix)", cand, n_total))
print()
print(du.mcnemar_report(cand, base, "draw",
                        label_a="post-fix", label_b="pre-fix"))
print()
print(du.mcnemar_report(cand, base, "home_win",
                        label_a="post-fix", label_b="pre-fix"))

common = sorted(set(base) & set(cand))
diffs = [(cand[i][0] + cand[i][1]) - (base[i][0] + base[i][1])
         for i in common]
mean_d = statistics.fmean(diffs)
sd = statistics.stdev(diffs) if len(diffs) > 1 else 0.0
se = sd / math.sqrt(len(diffs))
print(f"\n=== PAIRED TD/game  post-fix vs pre-fix  n={len(common)} pairs ===")
print(f"  TD/game: post-fix "
      f"{statistics.fmean(cand[i][0] + cand[i][1] for i in common):.2f}  "
      f"vs  pre-fix "
      f"{statistics.fmean(base[i][0] + base[i][1] for i in common):.2f}")
print(f"  paired delta = {mean_d:+.3f} TD/game   SE = {se:.3f}   "
      f"95% CI [{mean_d - du.Z95 * se:+.3f}, {mean_d + du.Z95 * se:+.3f}]")

skips_b, skips_c = n_total - len(base), n_total - len(cand)
print(f"\nwatchdog skips: pre-fix {skips_b}/{n_total}  "
      f"post-fix {skips_c}/{n_total}")
print("NOTE: this is rarer than the missing-bounce fix; no material draw-rate "
      "shift is expected. Validation goal is no crash/watchdog regression.")
