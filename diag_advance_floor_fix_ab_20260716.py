"""Paired-seed A/B validation of the ADVANCE prior-floor fix.

Bug + fix: evidence/fable_advance_vs_block_diagnostic_20260716.md, mechanism 3
-- ADVANCE macro priors had no floor unless trailing by 2+ goals, while
BLOCK/CAGE get an unconditional 0.12 floor (mirrors the 2026-07-03 CAGE fix).
Diagnosed as a structural cause of a ball carrier stalling for multiple turns
even when ADVANCE is the objectively best (risk-free) macro on the board --
full search picked it only 0.8% of the time in that state despite having the
highest one-ply Q of ~20 candidates, because its prior-starved visit share
never let Q evidence accumulate. Fix: minPrior = trailing2plus ? 0.15f : 0.12f
for MacroType::ADVANCE (engine/src/macro_mcts.cpp).

This is a genuine behavior-shaping change (unlike the two throw-in fixes,
which only affected a rare mechanic) -- a real shift in draw-rate/TD-rate is
plausible and would be the interesting result here, not just "no crash".
Primary validation goals: (a) no crash/watchdog regression, (b) characterize
whether ball-carrying teams actually advance/score more.

  python diag_advance_floor_fix_ab_20260716.py baseline  [N]
  python diag_advance_floor_fix_ab_20260716.py candidate [N]
  python diag_advance_floor_fix_ab_20260716.py report
"""
import math
import statistics
import sys

import diag_utils as du

MODE = sys.argv[1] if len(sys.argv) > 1 else "report"
N = int(sys.argv[2]) if len(sys.argv) > 2 else 300
BASE_SEED = 20260720
WEIGHTS = "weights_best.json"
POLICY = "weights_policy.json"
MCTS = 100
VF_BLEND = 0.0
TV = 1200

ARM_FILES = {
    "baseline": "arm_advfloor_base_20260716.json",
    "candidate": "arm_advfloor_fix_20260716.json",
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
    print(f"--- advfloor-fix A/B arm {MODE!r}: mirror null {WEIGHTS}  N={N} "
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
print("NOTE: unlike the throw-in fixes, this IS expected to plausibly shift "
      "behavior (more advancing/scoring) -- a real effect here would be a "
      "positive, interesting result, not a red flag.")
