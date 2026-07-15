"""Tier (b) validation of the gating side-swap patch (commit d90227b).

Spec: proposals_gating_sideswap_20260714.md section 3(b).

Null gate: candidate := frozen := weights_best.json (identical weights on both
sides), 10-tuple _gate_game tasks with cand_is_away = (i % 2 == 1), exactly the
production gate's orientation schedule. Parameters mirror the production gate
(MCTS=100, vf_blend=0.0, TV=1200, leaf_lookahead=False,
policy_path=weights_policy.json -- GATE_USE_POLICY_PRIORS default).

Expectations (proposal 3b):
- pooled HtH candidate decisive share ~= 50%, |deviation| < 2*sigma,
  sigma = 0.5/sqrt(decisive). Old unswapped null: ~59.9% (bias this fixes).
- arm symmetry: cand@HOME win rate ~= cand@AWAY loss rate (structural home
  edge flows symmetrically between arms and cancels in the pool);
- per-arm draw rates without a large gap.

Usage (repo root):  python diag_sideswap_null_20260715.py [N]
"""
import math
import sys

import diag_utils as du

N = int(sys.argv[1]) if len(sys.argv) > 1 else 300
BASE_SEED = 20260715          # convention: launch date
WEIGHTS = "weights_best.json"
POLICY = "weights_policy.json"
MCTS = 100
VF_BLEND = 0.0
TV = 1200

seeds = du.paired_seeds(N, base_seed=BASE_SEED)
tasks = [
    (s, i, WEIGHTS, WEIGHTS, MCTS, VF_BLEND, TV, False, POLICY, i % 2 == 1)
    for i, s in enumerate(seeds)
]

print(f"--- side-swap NULL gate: cand=frozen={WEIGHTS}  N={N} "
      f"base_seed={BASE_SEED} MCTS={MCTS} TV={TV} policy={POLICY} ---",
      flush=True)

res = du.run_arm("sideswap-null", tasks, mcts_iterations=MCTS)
du.save_arm("arm_sideswap_null_20260715.json", "sideswap-null", seeds, res)

# ---- analysis ---------------------------------------------------------------
arm = {0: [0, 0, 0], 1: [0, 0, 0]}   # orientation -> [W, D, L] of candidate
for r in res.values():
    cs, fs, ca = r
    j = 0 if cs > fs else (1 if cs == fs else 2)
    arm[ca][j] += 1

W0, D0, L0 = arm[0]          # cand @ HOME
W1, D1, L1 = arm[1]          # cand @ AWAY
n0, n1 = W0 + D0 + L0, W1 + D1 + L1
wins, draws, losses = W0 + W1, D0 + D1, L0 + L1
dec = wins + losses
share = wins / dec
sigma = 0.5 / math.sqrt(dec)
dev = share - 0.5
ci = du.Z95 * math.sqrt(share * (1 - share) / dec)

print(f"\n=== RESULTS (completed {len(res)}/{N}, "
      f"{N - len(res)} watchdog-skipped) ===")
print(f"pooled: {wins}W {draws}D {losses}L   draws {draws / len(res):.1%}")
print(f"pooled decisive share (candidate): {share:.1%}  "
      f"(95% CI +/- {ci:.1%}, decisive n={dec})")
print(f"null deviation from 50%: {dev * 100:+.1f}pp   sigma={sigma:.1%}   "
      f"|dev|/sigma = {abs(dev) / sigma:.2f}")
print(f"old UNSWAPPED null reference: ~59.9% decisive share")

print(f"\narm cand@HOME (n={n0}): {W0}W {D0}D {L0}L   "
      f"win {W0 / n0:.1%}  draw {D0 / n0:.1%}  loss {L0 / n0:.1%}")
print(f"arm cand@AWAY (n={n1}): {W1}W {D1}D {L1}L   "
      f"win {W1 / n1:.1%}  draw {D1 / n1:.1%}  loss {L1 / n1:.1%}")
print(f"symmetry: cand@H win {W0 / n0:.1%} vs cand@A loss {L1 / n1:.1%} "
      f"(delta {(W0 / n0 - L1 / n1) * 100:+.1f}pp); "
      f"draw-rate gap {(D0 / n0 - D1 / n1) * 100:+.1f}pp")

verdict = "PASS" if abs(dev) < 2 * sigma else "FAIL"
print(f"\nVERDICT: {verdict}  (criterion |share-50%| < 2*sigma "
      f"= {2 * sigma:.1%})")
