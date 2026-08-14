"""Paired-seed A/B validation of the hasActed double-activation fix.

Bug + fix: evidence/fable_hasacted_bug_20260715.md — a successful MOVE never
set hasActed, so a player could be reactivated later in the same team-turn
(free BLOCK/PASS/FOUL, 13.3% of team-turns). Fix: activation close-out at the
actor-switch boundary in executeAction (GameState.currentActivationId).

This is a C++ A/B across a rebuild (diag_utils save_arm/load_arm pattern):
  python diag_hasacted_fix_ab_20260715.py baseline  [N]   # pre-fix binary
  python diag_hasacted_fix_ab_20260715.py candidate [N]   # post-fix binary
  python diag_hasacted_fix_ab_20260715.py report          # compare arms

Both arms are mirror null games (cand = frozen = weights_best.json) over the
SAME seed list with the production gate schedule (cand_is_away = i % 2), so
the only difference between arms is the engine binary. Behavior change is
EXPECTED (illegal continuations removed from the action space); the goals are
(a) no watchdog/crash regression and (b) the new draw-rate/TD baseline.
"""
import math
import statistics
import sys

import diag_utils as du

MODE = sys.argv[1] if len(sys.argv) > 1 else "report"
N = int(sys.argv[2]) if len(sys.argv) > 2 else 300
BASE_SEED = 20260716          # launch date +1 (2nd experiment of 2026-07-15)
WEIGHTS = "weights_best.json"
POLICY = "weights_policy.json"
MCTS = 100
VF_BLEND = 0.0
TV = 1200

ARM_FILES = {
    "baseline": "arm_hasacted_base_20260715.json",
    "candidate": "arm_hasacted_fix_20260715.json",
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
    print(f"--- hasActed-fix A/B arm {MODE!r}: mirror null {WEIGHTS}  N={N} "
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

# Paired TD/game delta over common pairs.
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
print("NOTE: behavior change is expected by design (illegal reactivations "
      "removed); post-fix numbers are the NEW baseline.")
