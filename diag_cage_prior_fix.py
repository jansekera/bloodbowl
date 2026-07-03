#!/usr/bin/env python3
"""Smoke/decisive test (2026-07-03) for the CAGE prior-floor fix in
macro_mcts.cpp's expand(): CAGE's minPrior was 0.08 vs BLOCK's 0.12, but
BLOCK gets one prior-floored candidate PER favorable attacker/defender pair
(~2.17 on average, macro_actions.cpp getAvailableMacros) while CAGE only
ever emits a single candidate -- so the floors alone gave BLOCK-type macros
~3.3x CAGE's total prior mass. An independent Fable 5 outcome-level mining
investigation (97 self-mirror games, drive-level comparison controlling for
distance-to-endzone/turns-remaining/carrier-TZ context) found CAGE leads to
MORE touchdowns and FEWER lost balls than BLOCK from comparable situations
(drive-level Fisher exact p=0.036: TD 22.2% CAGE vs 12.0% BLOCK), including
in the exact "carrier already marked" states BLOCK is supposed to protect
best. Fix: raised CAGE's minPrior to 0.12 for per-candidate parity with
BLOCK (macro_mcts.cpp, case MacroType::CAGE).

Single-arm test (no clean "before" binary without a wasteful rebuild/revert
cycle). Compares against the last established reference point BEFORE this
fix: diag_negamax_fix.py's N=150 result, 38W 71D 34L = 49.7% draws (n=143)
-- includes all of today's other fixes (pickup-cage-throttle, backprop
turnover-discard, negamax, PASS targetId) but predates this CAGE prior fix.

Mirror match (champion vs itself, weights_best.json, vf_blend=0, MCTS=100),
policy_path=weights_policy.json (matches shipped production default,
GATE_USE_POLICY_PRIORS=1). Reuses production _gate_game/_imap_watchdog.
"""
import random
import sys
from pathlib import Path
from multiprocessing import Pool

sys.path.insert(0, "python"); sys.path.insert(0, "engine/build")
from run_iteration import _gate_game, _imap_watchdog, _pool_init

N = int(sys.argv[1]) if len(sys.argv) > 1 else 30
W = "weights_best.json"
POLICY_PATH = "weights_policy.json"
TV = 1000
VF_BLEND = 0.0
MCTS_ITERS = 100
WORKERS = 12
init_args = ("engine/build", "python")

if not Path(POLICY_PATH).exists():
    print(f"ERROR: {POLICY_PATH} not found in cwd -- run from repo root.", file=sys.stderr)
    sys.exit(1)

print(f"\n--- starting MIRROR gating MCTS={MCTS_ITERS} policy_path={POLICY_PATH!r} "
      f"(+ CAGE prior parity fix) n={N} ---", flush=True)
tasks = [
    (random.randint(1, 999999), i, W, W, MCTS_ITERS, VF_BLEND, TV, False, POLICY_PATH)
    for i in range(N)
]
wins = draws = losses = 0
done = 0
with Pool(WORKERS, initializer=_pool_init, initargs=init_args) as pool:
    for hs, as_ in _imap_watchdog(pool, _gate_game, tasks, 'cage-prior-fix',
                                  mcts_iterations=MCTS_ITERS):
        done += 1
        if hs > as_:
            wins += 1
        elif hs == as_:
            draws += 1
        else:
            losses += 1
        if done % 10 == 0 or done == N:
            total = wins + draws + losses
            print(f"  {done}/{N} done -- so far {wins}W {draws}D {losses}L "
                  f"= {100*draws/total:.1f}% draws", flush=True)

total = wins + draws + losses
print(f"\n=== MIRROR gate champion vs champion  cage-prior-fix  n={total} (of {N} requested) ===", flush=True)
print(f"{wins}W {draws}D {losses}L = {100*draws/total:.1f}% draws, "
      f"{100*wins/total:.1f}% home win, {100*losses/total:.1f}% home loss", flush=True)
print(f"  Compare to pre-fix reference (diag_negamax_fix.py, N=150): "
      f"49.7% draws, 26.6% home win, 23.8% home loss (n=143).", flush=True)
